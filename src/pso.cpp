
#define _CRT_SECURE_NO_WARNINGS
#include <ctime>
#include <mutex>
#include <fstream>
#include <thread>
#include <array>
#include <random>
#include <sstream>
#include <iostream>
#include <chrono>
#include <queue>
#include <atomic>

#include "tetris_core.h"
#include "search_tspin.h"
#include "ai_zzz.h"
#include "rule_srs.h"
#include "sb_tree.h"

#if _MSC_VER
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

#define USE_V08 1

namespace zzz
{
    template<size_t N> struct is_key_char
    {
        bool operator()(char c, char const *arr)
        {
            return arr[N - 2] == c || is_key_char<N - 1>()(c, arr);
        }
    };
    template<> struct is_key_char<1U>
    {
        bool operator()(char c, char const *arr)
        {
            return false;
        }
    };
    template<size_t N> void split(std::vector<std::string> &out, std::string const &in, char const (&arr)[N])
    {
        out.clear();
        std::string temp;
        for (auto c : in)
        {
            if (is_key_char<N>()(c, arr))
            {
                if (!temp.empty())
                {
                    out.emplace_back(std::move(temp));
                    temp.clear();
                }
            }
            else
            {
                temp += c;
            }
        }
        if (!temp.empty())
        {
            out.emplace_back(std::move(temp));
        }
    }
}

struct pso_dimension
{
    double x;
    double p;
    double v;
};
union pso_data
{
    pso_data() {}
    struct
    {
        std::array<double, 64> x;
        std::array<double, 64> p;
        std::array<double, 64> v;
    };
#if !USE_V08
    ai_zzz::TOJ::Param param;
#else
    ai_zzz::TOJ_v08::Param param;
#endif
};

struct pso_config_element
{
    double x_min, x_max, v_max;
};
struct pso_config
{
    std::vector<pso_config_element> config;
    double c1, c2, w, d;
};


void pso_init(pso_config const &config, pso_data &item, std::mt19937 &mt)
{
    for(size_t i = 0; i < config.config.size(); ++i)
    {
        auto &cfg = config.config[i];
        item.x[i] = std::uniform_real_distribution<double>(cfg.x_min, cfg.x_max)(mt);
        item.p[i] = item.x[i];
        item.v[i] = std::uniform_real_distribution<double>(-cfg.v_max, cfg.v_max)(mt);
    }
}
void pso_logic(pso_config const &config, pso_data const &best, pso_data &item, std::mt19937 &mt)
{
    for(size_t i = 0; i < config.config.size(); ++i)
    {
        auto &cfg = config.config[i];
        if (std::abs(item.v[i]) <= config.d && std::abs(best.p[i] - item.p[i]) <= std::abs(best.p[i] * config.d))
        {
            item.v[i] = std::uniform_real_distribution<double>(-cfg.v_max, cfg.v_max)(mt);
        }
        item.x[i] += item.v[i];
        item.v[i] = item.v[i] * config.w
            + std::uniform_real_distribution<double>(0, config.c1)(mt) * (item.p[i] - item.x[i])
            + std::uniform_real_distribution<double>(0, config.c2)(mt) * (best.p[i] - item.x[i])
            ;
        if(item.v[i] > cfg.v_max)
        {
            item.v[i] = cfg.v_max;
        }
        if(item.v[i] < -cfg.v_max)
        {
            item.v[i] = -cfg.v_max;
        }
    }
}

struct test_ai
{
#if !USE_V08
    m_tetris::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ, search_tspin::Search> ai;
#else
    m_tetris::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ_v08, search_tspin::Search> ai;
#endif
    m_tetris::TetrisMap map;
    std::mt19937 r_next, r_garbage;
    size_t next_length;
    size_t run_ms;
    int const *combo_table;
    int combo_table_max;
    std::vector<char> next;
    std::deque<int> recv_attack;
    int send_attack;
    int combo;
    int max_combo;
    char hold;
    bool b2b;
    bool dead;
    int total_block;
    int total_clear;
    int total_attack;
    int total_receive;

#if !USE_V08
    test_ai(m_tetris::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ, search_tspin::Search> &global_ai, int const *_combo_table, int _combo_table_max)
#else
    test_ai(m_tetris::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ_v08, search_tspin::Search>& global_ai, int const* _combo_table, int _combo_table_max)
#endif
        : ai(global_ai.context())
        , combo_table(_combo_table)
        , combo_table_max(_combo_table_max)
    {
    }

    void init(pso_data const &data, pso_config const &config, size_t round_ms)
    {
        map = m_tetris::TetrisMap(10, 40);
        ai.ai_config()->param = data.param;
        r_next.seed(std::random_device()());
        r_garbage.seed(r_next());
        next.clear();
        recv_attack.clear();
        send_attack = 0;
        combo = 0;
        max_combo = 0;
        hold = ' ';
        b2b = false;
        dead = false;
        next_length = 6;
        run_ms = round_ms;
        total_block = 0;
        total_clear = 0;
        total_attack = 0;
        total_receive = 0;
    }
    m_tetris::TetrisNode const *node() const
    {
        return ai.context()->generate(next.front());
    }
    void prepare()
    {
        if(!next.empty())
        {
            next.erase(next.begin());
        }
        while(next.size() <= next_length)
        {
            for (size_t i = 0; i < ai.context()->type_max(); ++i)
            {
                next.push_back(ai.context()->convert(i));
            }
            std::shuffle(next.end() - ai.context()->type_max(), next.end(), r_next);
        }
    }
    void run()
    {
        ai.search_config()->allow_rotate_move = true;
        ai.search_config()->allow_180 = false;
        ai.search_config()->allow_d = false;
        ai.search_config()->is_20g = false;
        ai.search_config()->last_rotate = false;
        ai.ai_config()->table = combo_table;
        ai.ai_config()->table_max = combo_table_max;
#if !USE_V08
        ai.ai_config()->safe = ai.ai()->get_safe(map, next.front());
        ai.status()->acc_value = 0;
        ai_zzz::TOJ::Status::init_t_value(map, ai.status()->t2_value, ai.status()->t3_value);
#else
        ai.status()->attack = 0;
        ai.status()->combo_attack = 0;
        ai.status()->max_attack = 0;
        ai.status()->max_combo = 0;
#endif
        ai.status()->death = 0;
        ai.status()->combo = combo;
        ai.status()->under_attack = std::accumulate(recv_attack.begin(), recv_attack.end(), 0);
        ai.status()->map_rise = 0;
        ai.status()->b2b = !!b2b;
        ai.status()->like = 0;
        ai.status()->value = 0;

        char current = next.front();
        auto result = ai.run_hold(map, ai.context()->generate(current), hold, true, next.data() + 1, next_length, run_ms);
        if(result.target == nullptr || result.target->low >= 20)
        {
            dead = true;
            return;
        }
        if (result.change_hold)
        {
            if (hold == ' ')
            {
                next.erase(next.begin());
            }
            hold = current;
        }
        int attack = 0;
        auto get_combo_attack = [&](int c)
        {
            return combo_table[std::min(combo_table_max - 1, c)];
        };
        int clear = result.target->attach(ai.context().get(), map);
        total_clear += clear;
        switch (clear)
        {
        case 0:
            combo = 0;
            break;
        case 1:
#if !USE_V08
            if (result.target.type == ai_zzz::TOJ::TSpinType::TSpinMini)
            {
                attack += 1 + b2b;
                b2b = 1;
            }
            else if (result.target.type == ai_zzz::TOJ::TSpinType::TSpin)
            {
                attack += 2 + b2b;
                b2b = 1;
            }
            else
            {
                b2b = 0;
            }
            attack += get_combo_attack(++combo);
            break;
        case 2:
            if (result.target.type != ai_zzz::TOJ::TSpinType::None)
            {
                attack += 4 + b2b;
                b2b = 1;
            }
            else
            {
                attack += 1;
                b2b = 0;
            }
            attack += get_combo_attack(++combo);
            break;
        case 3:
            if (result.target.type != ai_zzz::TOJ::TSpinType::None)
            {
                attack += 6 + b2b * 2;
                b2b = 1;
            }
            else
            {
                attack += 2;
                b2b = 0;
            }
            attack += get_combo_attack(++combo);
            break;
#else
            if (result.target.type == ai_zzz::TOJ_v08::TSpinType::TSpinMini)
            {
                attack += 1 + b2b;
                b2b = 1;
            }
            else if (result.target.type == ai_zzz::TOJ_v08::TSpinType::TSpin)
            {
                attack += 2 + b2b;
                b2b = 1;
            }
            else
            {
                b2b = 0;
            }
            attack += get_combo_attack(++combo);
            break;
        case 2:
            if (result.target.type != ai_zzz::TOJ_v08::TSpinType::None)
            {
                attack += 4 + b2b;
                b2b = 1;
            }
            else
            {
                attack += 1;
                b2b = 0;
            }
            attack += get_combo_attack(++combo);
            break;
        case 3:
            if (result.target.type != ai_zzz::TOJ_v08::TSpinType::None)
            {
                attack += 6 + b2b * 2;
                b2b = 1;
            }
            else
            {
                attack += 2;
                b2b = 0;
            }
            attack += get_combo_attack(++combo);
            break;
#endif
        case 4:
            attack += get_combo_attack(++combo) + 4 + b2b;
            b2b = 1;
            break;
        }
        if (map.count == 0)
        {
            attack += 6;
        }
        max_combo = std::max(combo, max_combo);
        ++total_block;
        total_attack += attack;
        send_attack = attack;
        while (!recv_attack.empty())
        {
            if (send_attack > 0)
            {
                if (recv_attack.front() <= send_attack)
                {
                    send_attack -= recv_attack.front();
                    recv_attack.pop_front();
                    continue;
                }
                else
                {
                    recv_attack.front() -= send_attack;
                    send_attack = 0;
                }
            }
            if (send_attack > 0 || combo > 0)
            {
                break;
            }
            int line = recv_attack.front();
            total_receive += line;
            recv_attack.pop_front();
            for (int y = map.height - 1; y >= line; --y)
            {
                map.row[y] = map.row[y - line];
            }
            uint32_t row = ai.context()->full() & ~(1 << std::uniform_int_distribution<uint32_t>(0, ai.context()->width() - 1)(r_garbage));
            for (int y = 0; y < line; ++y)
            {
                map.row[y] = row;
            }
            map.count = 0;
            for (int my = 0; my < map.height; ++my)
            {
                for (int mx = 0; mx < map.width; ++mx)
                {
                    if (map.full(mx, my))
                    {
                        map.top[mx] = map.roof = my + 1;
                        ++map.count;
                    }
                }
            }
        }
    }
    void under_attack(int line)
    {
        if(line > 0)
        {
            recv_attack.emplace_back(line);
        }
    }

    static void match(test_ai& ai1, test_ai& ai2, std::function<void(test_ai const &, test_ai const &)> out_put, size_t match_round)
    {
        size_t round = 0;
        for (; ; )
        {
            ++round;
            ai1.prepare();
            ai2.prepare();
            if (out_put)
            {
                out_put(ai1, ai2);
            }

            ai1.run();
            ai2.run();

            if (ai1.dead || ai2.dead)
            {
                return;
            }
            if (round > match_round)
            {
                return;
            }

            ai1.under_attack(ai2.send_attack);
            ai2.under_attack(ai1.send_attack);
        }
    }
};

struct BaseNode
{
    BaseNode *parent, *left, *right;
    size_t size : sizeof(size_t) * 8 - 1;
    size_t is_nil : 1;
};

struct NodeData
{
    NodeData()
    {
        score = 0;
        total_score = 0;
        apl = 0;
        app = 0;
        total_apl = 0;
        total_app = 0;
        best = std::numeric_limits<double>::quiet_NaN();
        match = 0;
        gen = 0;
    }
    char name[64];
    double score;
    double total_score;
    double apl;
    double total_apl;
    double total_app;
    double app;
    double best;
    uint32_t match;
    uint32_t gen;
    pso_data data;
};

struct Node : public BaseNode
{
    Node(NodeData const &d) : data(d)
    {
    }
    NodeData data;
};

struct SBTreeInterface
{
    typedef double key_t;
    typedef BaseNode node_t;
    typedef Node value_node_t;
    static key_t const &get_key(Node *node)
    {
        return *reinterpret_cast<double const *>(&node->data.score);
    }
    static bool is_nil(BaseNode *node)
    {
        return node->is_nil;
    }
    static void set_nil(BaseNode *node, bool nil)
    {
        node->is_nil = nil;
    }
    static BaseNode *get_parent(BaseNode *node)
    {
        return node->parent;
    }
    static void set_parent(BaseNode *node, BaseNode *parent)
    {
        node->parent = parent;
    }
    static BaseNode *get_left(BaseNode *node)
    {
        return node->left;
    }
    static void set_left(BaseNode *node, BaseNode *left)
    {
        node->left = left;
    }
    static BaseNode *get_right(BaseNode *node)
    {
        return node->right;
    }
    static void set_right(BaseNode *node, BaseNode *right)
    {
        node->right = right;
    }
    static size_t get_size(BaseNode *node)
    {
        return node->size;
    }
    static void set_size(BaseNode *node, size_t size)
    {
        node->size = size;
    }
    static bool predicate(key_t const &left, key_t const &right)
    {
        return left > right;
    }
};

int main(int argc, char const *argv[])
{
    std::atomic<uint32_t> count{std::max<uint32_t>(1, std::thread::hardware_concurrency() - 1)};
    std::string file = "data.bin";
    if (argc > 1)
    {
        uint32_t arg_count = std::stoul(argv[1], nullptr, 10);
        if (arg_count != 0)
        {
            count = arg_count;
        }
    }
    uint32_t node_count = count * 2;
    if (argc > 2)
    {
        uint32_t arg_count = std::stoul(argv[2], nullptr, 10);
        if (arg_count >= 2)
        {
            node_count = arg_count;
        }
    }
    std::atomic<bool> view{false};
    std::atomic<uint32_t> view_index{0};
    if (argc > 3)
    {
        file = argv[3];
    }
    std::recursive_mutex rank_table_lock;
    zzz::sb_tree<SBTreeInterface> rank_table;
    std::ifstream ifs(file, std::ios::in | std::ios::binary);
    if (ifs.good())
    {
        NodeData data;
        while (ifs.read(reinterpret_cast<char *>(&data), sizeof data).gcount() == sizeof data)
        {
            rank_table.insert(new Node(data));
        }
        ifs.close();
    }
    pso_config pso_cfg =
    {
        {}, 1, 1, 0.5, 0.01,
    };
    size_t elo_max_match = std::max<size_t>(node_count * 15, 256);
    size_t elo_min_match = elo_max_match / 2;

    auto v = [&pso_cfg](double v, double r, double s)
    {
        pso_config_element d =
        {
            v - r, v + r, s
        };
        pso_cfg.config.emplace_back(d);
    };
    std::mt19937 mt;

    {
#if !USE_V08
        ai_zzz::TOJ::Param p;

        v(p.base       ,  100,   2);
        v(p.roof       , 1000,   8);
        v(p.col_trans  , 1000,   8);
        v(p.row_trans  , 1000,   8);
        v(p.hole_count , 1000,   8);
        v(p.hole_line  , 1000,   8);
        v(p.clear_width, 1000,   2);
        v(p.wide_2     , 1000,   8);
        v(p.wide_3     , 1000,   8);
        v(p.wide_4     , 1000,   8);
        v(p.safe       , 1000,   8);
        v(p.b2b        , 1000,   8);
        v(p.attack     , 1000,   8);
        v(p.hold_t     ,  100,   2);
        v(p.hold_i     ,  100,   2);
        v(p.waste_t    ,  100,   2);
        v(p.waste_i    ,  100,   2);
        v(p.clear_1    ,  100,   2);
        v(p.clear_2    ,  100,   2);
        v(p.clear_3    ,  100,   2);
        v(p.clear_4    ,  100,   2);
        v(p.t2_slot    ,  100, 0.5);
        v(p.t3_slot    ,  100, 0.5);
        v(p.tspin_mini ,  100,   2);
        v(p.tspin_1    ,  100,   2);
        v(p.tspin_2    ,  100,   2);
        v(p.tspin_3    ,  100,   2);
        v(p.combo      ,  100,   2);
        v(p.ratio      ,   10, 0.5);
#else
        ai_zzz::TOJ_v08::Param p;

        v(p.roof       , 1000,   8);
        v(p.col_trans  , 1000,   8);
        v(p.row_trans  , 1000,   8);
        v(p.hole_count , 1000,   8);
        v(p.hole_line  , 1000,   8);
        v(p.well_depth , 1000,   8);
        v(p.hole_depth , 1000,   8);
        v(p.b2b        , 1000,   8);
        v(p.attack     , 1000,   8);
        v(p.max_attack , 1000,   8);
        v(p.hold_t     ,  100,   2);
        v(p.hold_i     ,  100,   2);
        v(p.waste_t    ,  100,   2);
        v(p.waste_i    ,  100,   2);
        v(p.clear_1    ,  100,   2);
        v(p.clear_2    ,  100,   2);
        v(p.clear_3    ,  100,   2);
        v(p.clear_4    ,  100,   2);
        v(p.t2_slot    ,  100, 0.5);
        v(p.t3_slot    ,  100, 0.5);
        v(p.tspin_mini ,  100,   2);
        v(p.tspin_1    ,  100,   2);
        v(p.tspin_2    ,  100,   2);
        v(p.tspin_3    ,  100,   2);
        v(p.combo      ,  100,   2);
        v(p.ratio      ,   10, 0.5);
#endif

        if (rank_table.empty())
        {
            NodeData init_node;

            strncpy(init_node.name, "*default", sizeof init_node.name);
            memset(&init_node.data, 0, sizeof init_node.data);
            init_node.data.param = p;
            init_node.data.p = init_node.data.x;
            rank_table.insert(new Node(init_node));
	}
    }

    while (rank_table.size() < node_count)
    {
        NodeData init_node = rank_table.at(std::uniform_int_distribution<size_t>(0, rank_table.size() - 1)(mt))->data;

        strncpy(init_node.name, ("init_" + std::to_string(rank_table.size())).c_str(), sizeof init_node.name);
        pso_logic(pso_cfg, init_node.data, init_node.data, mt);
        rank_table.insert(new Node(init_node));
    }

    std::vector<std::thread> threads;
    int combo_table[] = { 0,0,0,1,1,2,2,3,3,4,4,4,5 };
    int combo_table_max = 13;
#if !USE_V08
    m_tetris::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ, search_tspin::Search> global_ai;
#else
    m_tetris::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ_v08, search_tspin::Search> global_ai;
#endif
    global_ai.prepare(10, 40);

    for (size_t i = 1; i <= count; ++i)
    {
        threads.emplace_back([&, i]()
        {
            uint32_t index = i + 1;
            auto rand_match = [&](auto &mt, size_t max)
            {
                std::pair<size_t, size_t> ret;
                ret.first = std::uniform_int_distribution<size_t>(0, max - 1)(mt);
                do
                {
                    ret.second = std::uniform_int_distribution<size_t>(0, max - 1)(mt);
                } while (ret.second == ret.first);
                return ret;
            };
            test_ai ai1(global_ai, combo_table, combo_table_max);
            test_ai ai2(global_ai, combo_table, combo_table_max);
            rank_table_lock.lock();
            rank_table_lock.unlock();
            for (; ; )
            {
                rank_table_lock.lock();
                auto m12 = rand_match(mt, rank_table.size());
                auto m1 = rank_table.at(m12.first);
                auto m2 = rank_table.at(m12.second);
#if _MSC_VER
                auto view_func = [m1, m2, index, &view, &view_index, &rank_table_lock](test_ai const &ai1, test_ai const &ai2)
                {
                    COORD coordScreen = { 0, 0 };
                    DWORD cCharsWritten;
                    CONSOLE_SCREEN_BUFFER_INFO csbi;
                    DWORD dwConSize;
                    HANDLE hConsole;
                    if (view && view_index == 0)
                    {
                        rank_table_lock.lock();
                        if (view && view_index == 0)
                        {
                            view_index = index;
                            hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                            GetConsoleScreenBufferInfo(hConsole, &csbi);
                            dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
                            FillConsoleOutputCharacterA(hConsole, ' ', dwConSize, coordScreen, &dwConSize);
                        }
                        rank_table_lock.unlock();
                    }
                    if (index != view_index)
                    {
                        return;
                    }

                    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

                    GetConsoleScreenBufferInfo(hConsole, &csbi);
                    dwConSize = csbi.dwSize.X * 2;
                    FillConsoleOutputCharacterA(hConsole, ' ', dwConSize, coordScreen, &dwConSize);
                    GetConsoleScreenBufferInfo(hConsole, &csbi);
                    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
                    SetConsoleCursorPosition(hConsole, coordScreen);

                    char out[81920] = "";
                    char box_0[3] = "  ";
                    char box_1[3] = "[]";

                    out[0] = '\0';
                    int up1 = std::accumulate(ai1.recv_attack.begin(), ai1.recv_attack.end(), 0);
                    int up2 = std::accumulate(ai2.recv_attack.begin(), ai2.recv_attack.end(), 0);
                    snprintf(out, sizeof out, "HOLD = %c NEXT = %c%c%c%c%c%c COMBO = %d B2B = %d UP = %2d ATK = %4d BLOCK = %4d NAME = %s\n"
                                              "HOLD = %c NEXT = %c%c%c%c%c%c COMBO = %d B2B = %d UP = %2d ATK = %4d BLOCK = %4d NAME = %s\n",
                        ai1.hold, ai1.next[1], ai1.next[2], ai1.next[3], ai1.next[4], ai1.next[5], ai1.next[6], ai1.combo, ai1.b2b, up1, ai1.total_attack , ai1.total_block, m1->data.name,
                        ai2.hold, ai2.next[1], ai2.next[2], ai2.next[3], ai2.next[4], ai2.next[5], ai2.next[6], ai2.combo, ai2.b2b, up2, ai2.total_attack , ai2.total_block, m2->data.name);
                    m_tetris::TetrisMap map_copy1 = ai1.map;
                    m_tetris::TetrisMap map_copy2 = ai2.map;
                    ai1.node()->attach(ai1.ai.context().get(), map_copy1);
                    ai2.node()->attach(ai2.ai.context().get(), map_copy2);
                    for (int y = 21; y >= 0; --y)
                    {
                        for (int x = 0; x < 10; ++x)
                        {
                            strcat_s(out, map_copy1.full(x, y) ? box_1 : box_0);
                        }
                        strcat_s(out, "  ");
                        for (int x = 0; x < 10; ++x)
                        {
                            strcat_s(out, map_copy2.full(x, y) ? box_1 : box_0);
                        }
                        strcat_s(out, "\r\n");
                    }
                    WriteConsoleA(hConsole, out, strlen(out), nullptr, nullptr);
                    Sleep(100);
                };
#else
                auto view_func = [m1, m2, index, &view, &view_index, &rank_table_lock](test_ai const &ai1, test_ai const &ai2)
                {
                    if (view && view_index == 0)
                    {
                        rank_table_lock.lock();
                        if (view && view_index == 0)
                        {
                            view_index = index;
                        }
                        rank_table_lock.unlock();
                    }
                    if (index != view_index)
                    {
                        return;
                    }

                    char out[81920] = "";
                    char box_0[3] = "  ";
                    char box_1[3] = "[]";

                    out[0] = '\0';
                    int up1 = std::accumulate(ai1.recv_attack.begin(), ai1.recv_attack.end(), 0);
                    int up2 = std::accumulate(ai2.recv_attack.begin(), ai2.recv_attack.end(), 0);
                    snprintf(out, sizeof out, "HOLD = %c NEXT = %c%c%c%c%c%c COMBO = %d B2B = %d UP = %2d ATK = %4d BLOCK = %4d NAME = %s\n"
                                              "HOLD = %c NEXT = %c%c%c%c%c%c COMBO = %d B2B = %d UP = %2d ATK = %4d BLOCK = %4d NAME = %s\n",
                        ai1.hold, ai1.next[1], ai1.next[2], ai1.next[3], ai1.next[4], ai1.next[5], ai1.next[6], ai1.combo, ai1.b2b, up1, ai1.total_attack , ai1.total_block, m1->data.name,
                        ai2.hold, ai2.next[1], ai2.next[2], ai2.next[3], ai2.next[4], ai2.next[5], ai2.next[6], ai2.combo, ai2.b2b, up2, ai2.total_attack , ai2.total_block, m2->data.name);
                    m_tetris::TetrisMap map_copy1 = ai1.map;
                    m_tetris::TetrisMap map_copy2 = ai2.map;
                    ai1.node()->attach(ai1.ai.context().get(), map_copy1);
                    ai2.node()->attach(ai1.ai.context().get(), map_copy2);
                    for (int y = 21; y >= 0; --y)
                    {
                        for (int x = 0; x < 10; ++x)
                        {
                            strcat(out, map_copy1.full(x, y) ? box_1 : box_0);
                        }
                        strcat(out, "  ");
                        for (int x = 0; x < 10; ++x)
                        {
                            strcat(out, map_copy2.full(x, y) ? box_1 : box_0);
                        }
                        strcat(out, "\r\n");
                    }
		    printf("%s", out);
                    usleep(333000);
                };
#endif
                // size_t round_ms_min = (45 + m1->data.match) / 3;
                // size_t round_ms_max = (45 + m2->data.match) / 3;
                // if (round_ms_min > round_ms_max)
                // {
                //     std::swap(round_ms_min, round_ms_max);
                // }
                // size_t round_ms = std::uniform_int_distribution<size_t>(round_ms_min, round_ms_max)(mt);
                size_t round_ms = 0;
                size_t round_count = 3600;
                ai1.init(m1->data.data, pso_cfg, round_ms);
                ai2.init(m2->data.data, pso_cfg, round_ms);
                rank_table_lock.unlock();
                test_ai::match(ai1, ai2, view_func, round_count);
                rank_table_lock.lock();

                rank_table.erase(m1);
                rank_table.erase(m2);
                static double best_ai_score;
                double m1s = m1->data.score;
                double m2s = m2->data.score;
                double ai1_vs = ai1.total_clear == 0 ? 0. : static_cast<double>(ai1.total_attack) / ai1.total_block;
                double ai2_vs = ai2.total_clear == 0 ? 0. : static_cast<double>(ai2.total_attack) / ai2.total_block;
                double ai1vs = ai1_vs;
                if (ai1.dead == ai2.dead)
                {
                    if (ai1_vs > ai2_vs)
                    {
                        double ai_2 = ai2_vs;
                        ai2_vs -= ai1_vs;
                        ai1_vs += ai_2;
                    }
                    else if (ai2_vs > ai1_vs)
                    {
                        double ai_1 = ai1_vs;
                        ai1_vs -= ai2_vs;
                        ai2_vs += ai_1;
                    }
                    else
                    {
                        ai1_vs += ai2_vs;
                        ai2_vs += ai1vs;
                    }
                }
                else
                {
                    ai1_vs += !ai1.dead ? ai2_vs : -ai2_vs;
                    ai2_vs += !ai2.dead ? ai1vs : -ai1vs;
                }
                ++m1->data.match;
                ++m2->data.match;
                m1->data.total_score += ai1_vs * 100;
                m2->data.total_score += ai2_vs * 100;
                m1->data.total_apl += static_cast<double>(ai1.total_attack) / (ai1.total_clear == 0 ? 1 : ai1.total_clear);
                m1->data.total_app += static_cast<double>(ai1.total_attack) / ai1.total_block;
                m2->data.total_apl += static_cast<double>(ai2.total_attack) / (ai2.total_clear == 0 ? 1 : ai2.total_clear);
                m2->data.total_app += static_cast<double>(ai2.total_attack) / ai2.total_block;
                m1->data.app = m1->data.total_app / m1->data.match;
                m2->data.app = m2->data.total_app / m2->data.match;
                m1->data.apl = m1->data.total_apl / m1->data.match;
                m2->data.apl = m2->data.total_apl / m2->data.match;
                m1->data.score = m1->data.total_score / m1->data.match;
                m2->data.score = m2->data.total_score / m2->data.match;
                rank_table.insert(m1);
                rank_table.insert(m2);

                auto export_best_param = [&](Node* node)
                {
                    FILE* best_param = fopen("best_param.txt", "w");
                    if (best_param == NULL)
                    {
                        std::fstream file("best_param.txt", std::ios::out);
                        file.close();
                        best_param = fopen("best_param.txt", "w");
                    }
#if !USE_V08
                    fprintf(best_param, "//NAME: %s\n//GEN: %d\n//SCORE: %.2f\n//APL: %.2f\n//APP: %.2f\nsrs_ai.ai_config()->param = "
                        "{%.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                        " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                        " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f};\n"
                        , node->data.name
                        , node->data.gen
                        , node->data.score
                        , node->data.apl
                        , node->data.app
                        , node->data.data.x[0]
                        , node->data.data.x[1]
                        , node->data.data.x[2]
                        , node->data.data.x[3]
                        , node->data.data.x[4]
                        , node->data.data.x[5]
                        , node->data.data.x[6]
                        , node->data.data.x[7]
                        , node->data.data.x[8]
                        , node->data.data.x[9]
                        , node->data.data.x[10]
                        , node->data.data.x[11]
                        , node->data.data.x[12]
                        , node->data.data.x[13]
                        , node->data.data.x[14]
                        , node->data.data.x[15]
                        , node->data.data.x[16]
                        , node->data.data.x[17]
                        , node->data.data.x[18]
                        , node->data.data.x[19]
                        , node->data.data.x[20]
                        , node->data.data.x[21]
                        , node->data.data.x[22]
                        , node->data.data.x[23]
                        , node->data.data.x[24]
                        , node->data.data.x[25]
                        , node->data.data.x[26]
                        , node->data.data.x[27]
                        , node->data.data.x[28]
                    );
                    fclose(best_param);
                };
#else
                    fprintf(best_param, "//NAME: %s\n//GEN: %d\n//SCORE: %.2f\n//APL: %.2f\n//APP: %.2f\nsrs_ai.ai_config()->param = "
                        "{%.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                        " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                        " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f};\n"
                        , node->data.name
                        , node->data.gen
                        , node->data.score
                        , node->data.apl
                        , node->data.app
                        , node->data.data.x[0]
                        , node->data.data.x[1]
                        , node->data.data.x[2]
                        , node->data.data.x[3]
                        , node->data.data.x[4]
                        , node->data.data.x[5]
                        , node->data.data.x[6]
                        , node->data.data.x[7]
                        , node->data.data.x[8]
                        , node->data.data.x[9]
                        , node->data.data.x[10]
                        , node->data.data.x[11]
                        , node->data.data.x[12]
                        , node->data.data.x[13]
                        , node->data.data.x[14]
                        , node->data.data.x[15]
                        , node->data.data.x[16]
                        , node->data.data.x[17]
                        , node->data.data.x[18]
                        , node->data.data.x[19]
                        , node->data.data.x[20]
                        , node->data.data.x[21]
                        , node->data.data.x[22]
                        , node->data.data.x[23]
                        , node->data.data.x[24]
                        , node->data.data.x[25]
                    );
                    fclose(best_param);
                };
#endif
                auto do_pso_logic = [&](Node* node)
                {
                    best_ai_score = 0;
                    for (auto it = rank_table.begin(); it != rank_table.end(); ++it)
                    {
                        if (it->data.best > best_ai_score)
                        {
                            best_ai_score = it->data.best;
                        }
                    }
                    if (node->data.score > best_ai_score)
                    {
                        if (!view)
#if !USE_V08
                            printf(
                                "\n________________________________________________\nNAME: %s\nGEN: %d\nSCORE: %.2f\n//APL: %.2f\n//APP: %.2f\nPARAM: "
                                "{%.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                                " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                                " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f}\n"
                                "\n________________________________________________\n"
                                , node->data.name
                                , node->data.gen
                                , node->data.score
                                , node->data.apl
                                , node->data.app
                                , node->data.data.x[0]
                                , node->data.data.x[1]
                                , node->data.data.x[2]
                                , node->data.data.x[3]
                                , node->data.data.x[4]
                                , node->data.data.x[5]
                                , node->data.data.x[6]
                                , node->data.data.x[7]
                                , node->data.data.x[8]
                                , node->data.data.x[9]
                                , node->data.data.x[10]
                                , node->data.data.x[11]
                                , node->data.data.x[12]
                                , node->data.data.x[13]
                                , node->data.data.x[14]
                                , node->data.data.x[15]
                                , node->data.data.x[16]
                                , node->data.data.x[17]
                                , node->data.data.x[18]
                                , node->data.data.x[19]
                                , node->data.data.x[20]
                                , node->data.data.x[21]
                                , node->data.data.x[22]
                                , node->data.data.x[23]
                                , node->data.data.x[24]
                                , node->data.data.x[25]
                                , node->data.data.x[26]
                                , node->data.data.x[27]
                                , node->data.data.x[28]
                            );
#else
                            printf(
                                "\n________________________________________________\nNAME: %s\nGEN: %d\nSCORE: %.2f\n//APL: %.2f\n//APP: %.2f\nPARAM: "
                                "{%.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                                " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                                " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f}\n"
                                "\n________________________________________________\n"
                                , node->data.name
                                , node->data.gen
                                , node->data.score
                                , node->data.apl
                                , node->data.app
                                , node->data.data.x[0]
                                , node->data.data.x[1]
                                , node->data.data.x[2]
                                , node->data.data.x[3]
                                , node->data.data.x[4]
                                , node->data.data.x[5]
                                , node->data.data.x[6]
                                , node->data.data.x[7]
                                , node->data.data.x[8]
                                , node->data.data.x[9]
                                , node->data.data.x[10]
                                , node->data.data.x[11]
                                , node->data.data.x[12]
                                , node->data.data.x[13]
                                , node->data.data.x[14]
                                , node->data.data.x[15]
                                , node->data.data.x[16]
                                , node->data.data.x[17]
                                , node->data.data.x[18]
                                , node->data.data.x[19]
                                , node->data.data.x[20]
                                , node->data.data.x[21]
                                , node->data.data.x[22]
                                , node->data.data.x[23]
                                , node->data.data.x[24]
                                , node->data.data.x[25]
                            );
#endif
                        export_best_param(node);
                        best_ai_score = node->data.score;
                    }
                    NodeData* data = &node->data;
                    if (std::isnan(data->best) || data->score > data->best)
                    {
                        data->best = data->score;
                        data->data.p = data->data.x;
                    }
                    else
                    {
                        data->best = data->best * 0.95 + data->score * 0.05;
                    }
                    data->total_score = 0;
                    data->match = 0;
                    data->total_apl = 0;
                    data->total_app = 0;
                    data->app = 0;
                    data->apl = 0;
                    ++data->gen;
                    rank_table.erase(node);
                    data->score = 0;
                    rank_table.insert(node);
                    //if (node->data.name[0] == '*' || node->data.name[0] == '-')
                    //{
                    //    return;
                    //}
                    double best;
                    pso_data* best_data = nullptr;
                    for (auto it = rank_table.begin(); it != rank_table.end(); ++it)
                    {
                        if (it->data.name[0] == '*' || std::isnan(it->data.best))
                        {
                            continue;
                        }
                        if (best_data == nullptr || it->data.best > best)
                        {
                            best = it->data.best;
                            best_data = &it->data.data;
                        }
                    }
                    pso_logic(pso_cfg, best_data != nullptr ? *best_data : data->data, data->data, mt);
                };
                bool check = m1->data.match >= elo_min_match;
                bool check_2 = m2->data.match >= elo_min_match;
                bool reset = m1->data.score < m1->data.best;
                bool reset_2 = m2->data.score < m2->data.best;
                if (m1->data.match >= elo_max_match || (check && reset))
                {
                    do_pso_logic(m1);
                }
                if (m2->data.match >= elo_max_match || (check_2 && reset_2))
                {
                    do_pso_logic(m2);
                }
                rank_table_lock.unlock();
            }
        });
    }
    Node *edit = nullptr;
    auto print_config = [&rank_table, &rank_table_lock](Node *node)
    {
        rank_table_lock.lock();
#if !USE_V08
        printf(
            "\n________________________________________________\nNAME: %s\nGEN: %d\nSCORE: %.2f\n//APL: %.2f\n//APP: %.2f\nPARAM: "
            "{%.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
            " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
            " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f}\n"
            "\n________________________________________________\n"
            , node->data.name
            , node->data.gen
            , node->data.score
            , node->data.apl
            , node->data.app
            , node->data.data.x[0]
            , node->data.data.x[1]
            , node->data.data.x[2]
            , node->data.data.x[3]
            , node->data.data.x[4]
            , node->data.data.x[5]
            , node->data.data.x[6]
            , node->data.data.x[7]
            , node->data.data.x[8]
            , node->data.data.x[9]
            , node->data.data.x[10]
            , node->data.data.x[11]
            , node->data.data.x[12]
            , node->data.data.x[13]
            , node->data.data.x[14]
            , node->data.data.x[15]
            , node->data.data.x[16]
            , node->data.data.x[17]
            , node->data.data.x[18]
            , node->data.data.x[19]
            , node->data.data.x[20]
            , node->data.data.x[21]
            , node->data.data.x[22]
            , node->data.data.x[23]
            , node->data.data.x[24]
            , node->data.data.x[25]
            , node->data.data.x[26]
            , node->data.data.x[27]
            , node->data.data.x[28]
        );
#else
        printf(
            "\n________________________________________________\nNAME: %s\nGEN: %d\nSCORE: %.2f\n//APL: %.2f\n//APP: %.2f\nPARAM: "
            "{%.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
            " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
            " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f}\n"
            "\n________________________________________________\n"
            , node->data.name
            , node->data.gen
            , node->data.score
            , node->data.apl
            , node->data.app
            , node->data.data.x[0]
            , node->data.data.x[1]
            , node->data.data.x[2]
            , node->data.data.x[3]
            , node->data.data.x[4]
            , node->data.data.x[5]
            , node->data.data.x[6]
            , node->data.data.x[7]
            , node->data.data.x[8]
            , node->data.data.x[9]
            , node->data.data.x[10]
            , node->data.data.x[11]
            , node->data.data.x[12]
            , node->data.data.x[13]
            , node->data.data.x[14]
            , node->data.data.x[15]
            , node->data.data.x[16]
            , node->data.data.x[17]
            , node->data.data.x[18]
            , node->data.data.x[19]
            , node->data.data.x[20]
            , node->data.data.x[21]
            , node->data.data.x[22]
            , node->data.data.x[23]
            , node->data.data.x[24]
            , node->data.data.x[25]
        );
#endif
        rank_table_lock.unlock();
    };

    std::map<std::string, std::function<bool(std::vector<std::string> const &)>> command_map;
    command_map.insert(std::make_pair("select", [&edit, &print_config, &rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        if (token.size() == 2)
        {
            size_t index = std::atoi(token[1].c_str()) - 1;
            if (index < rank_table.size())
            {
                rank_table_lock.lock();
                edit = rank_table.at(index);
                print_config(edit);
                rank_table_lock.unlock();
            }
        }
        else if (token.size() == 1 && edit != nullptr)
        {
            rank_table_lock.lock();
            print_config(edit);
            rank_table_lock.unlock();
        }
        return true;
    }));
    command_map.insert(std::make_pair("set", [&edit, &print_config, &rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        if (token.size() >= 3 && token.size() <= 5 && edit != nullptr)
        {
            size_t index = std::atoi(token[1].c_str());
            rank_table_lock.lock();
            if (index == 99 && token[2].size() < 64)
            {
                memcpy(edit->data.name, token[2].c_str(), token[2].size() + 1);
            }
#if !USE_V08
            else if (index < sizeof(ai_zzz::TOJ::Param) / sizeof(double))
#else
            else if (index < sizeof(ai_zzz::TOJ_v08::Param) / sizeof(double))
#endif
            {
                edit->data.data.x[index] = std::atof(token[2].c_str());
                if (token.size() >= 4)
                {
                    edit->data.data.p[index] = std::atof(token[3].c_str());
                }
                if (token.size() >= 5)
                {
                    edit->data.data.v[index] = std::atof(token[4].c_str());
                }
            }
            print_config(edit);
            rank_table_lock.unlock();
        }
        return true;
    }));
    command_map.insert(std::make_pair("copy", [&edit, &print_config, &rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        if (token.size() == 2 && token[1].size() < 64 && edit != nullptr)
        {
            rank_table_lock.lock();
            NodeData data = edit->data;
            memcpy(data.name, token[1].c_str(), token[1].size() + 1);
            data.match = 0;
            data.score = 0;
            Node *node = new Node(data);
            rank_table.insert(node);
            print_config(node);
            edit = node;
            rank_table_lock.unlock();
        }
        return true;
    }));
    command_map.insert(std::make_pair("rank", [&rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        rank_table_lock.lock();
        size_t begin = 0, end = rank_table.size();
        if (token.size() == 2)
        {
            begin = std::atoi(token[1].c_str()) - 1;
            end = begin + 1;
        }
        if (token.size() == 3)
        {
            begin = std::atoi(token[1].c_str()) - 1;
            end = begin + std::atoi(token[2].c_str());
        }
        for (size_t i = begin; i < end && i < rank_table.size(); ++i)
        {
            auto node = rank_table.at(i);
            printf("#%3d | SCORE = %12.1f | HIGHSCORE = %12.1f | MATCH = %3zd | GEN = %5zd | NAME = %s\n", i + 1, node->data.score, node->data.best, node->data.match, node->data.gen, node->data.name);
        }
        rank_table_lock.unlock();
        return true;
    }));
    command_map.insert(std::make_pair("view", [&view](std::vector<std::string> const &token)
    {
        view = true;
        return true;
    }));
    command_map.insert(std::make_pair("best", [&rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        if (token.size() != 2)
        {
            return true;
        }
        rank_table_lock.lock();
        double best;
        pso_data* node = nullptr;
        for (auto it = rank_table.begin(); it != rank_table.end(); ++it)
        {
            if (std::isnan(it->data.best))
            {
                continue;
            }
            if (node == nullptr || it->data.best > best)
            {
                best = it->data.best;
                node = &it->data.data;
            }
        }
        if (node == nullptr)
        {
            node = &rank_table.front()->data.data;
        }
        if (token[1] == "bat")
        {
#if !USE_V08
            printf(
                "SET base=%.27f\n"
                "SET roof=%.27f\n"
                "SET col_trans=%.27f\n"
                "SET row_trans=%.27f\n"
                "SET hole_count=%.27f\n"
                "SET hole_line=%.27f\n"
                "SET clear_width=%.27f\n"
                "SET wide_2=%.27f\n"
                "SET wide_3=%.27f\n"
                "SET wide_4=%.27f\n"
                "SET safe=%.27f\n"
                "SET b2b=%.27f\n"
                "SET attack=%.27f\n"
                "SET hold_t=%.27f\n"
                "SET hold_i=%.27f\n"
                "SET waste_t=%.27f\n"
                "SET waste_i=%.27f\n"
                "SET clear_1=%.27f\n"
                "SET clear_2=%.27f\n"
                "SET clear_3=%.27f\n"
                "SET clear_4=%.27f\n"
                "SET t2_slot=%.27f\n"
                "SET t3_slot=%.27f\n"
                "SET tspin_mini=%.27f\n"
                "SET tspin_1=%.27f\n"
                "SET tspin_2=%.27f\n"
                "SET tspin_3=%.27f\n"
                "SET combo=%.27f\n"
                "SET ratio=%.27f\n"
                , node->p[ 0]
                , node->p[ 1]
                , node->p[ 2]
                , node->p[ 3]
                , node->p[ 4]
                , node->p[ 5]
                , node->p[ 6]
                , node->p[ 7]
                , node->p[ 8]
                , node->p[ 9]
                , node->p[10]
                , node->p[11]
                , node->p[12]
                , node->p[13]
                , node->p[14]
                , node->p[15]
                , node->p[16]
                , node->p[17]
                , node->p[18]
                , node->p[19]
                , node->p[20]
                , node->p[21]
                , node->p[22]
                , node->p[23]
                , node->p[24]
                , node->p[25]
                , node->p[26]
                , node->p[27]
                , node->p[28]
            );
        }
        else if (token[1] == "cpp")
        {
            printf(
                "{%.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f}\n"
                , node->p[ 0]
                , node->p[ 1]
                , node->p[ 2]
                , node->p[ 3]
                , node->p[ 4]
                , node->p[ 5]
                , node->p[ 6]
                , node->p[ 7]
                , node->p[ 8]
                , node->p[ 9]
                , node->p[10]
                , node->p[11]
                , node->p[12]
                , node->p[13]
                , node->p[14]
                , node->p[15]
                , node->p[16]
                , node->p[17]
                , node->p[18]
                , node->p[19]
                , node->p[20]
                , node->p[21]
                , node->p[22]
                , node->p[23]
                , node->p[24]
                , node->p[25]
                , node->p[26]
                , node->p[27]
                , node->p[28]
            );
#else
              printf(
                "SET roof=%.27f\n"
                "SET col_trans=%.27f\n"
                "SET row_trans=%.27f\n"
                "SET hole_count=%.27f\n"
                "SET hole_line=%.27f\n"
                "SET well_depth=%.27f\n"
                "SET hole_depth=%.27f\n"
                "SET b2b=%.27f\n"
                "SET attack=%.27f\n"
                "SET max_attack=%.27f\n"
                "SET hold_t=%.27f\n"
                "SET hold_i=%.27f\n"
                "SET waste_t=%.27f\n"
                "SET waste_i=%.27f\n"
                "SET clear_1=%.27f\n"
                "SET clear_2=%.27f\n"
                "SET clear_3=%.27f\n"
                "SET clear_4=%.27f\n"
                "SET t2_slot=%.27f\n"
                "SET t3_slot=%.27f\n"
                "SET tspin_mini=%.27f\n"
                "SET tspin_1=%.27f\n"
                "SET tspin_2=%.27f\n"
                "SET tspin_3=%.27f\n"
                "SET combo=%.27f\n"
                "SET ratio=%.27f\n"
                , node->p[ 0]
                , node->p[ 1]
                , node->p[ 2]
                , node->p[ 3]
                , node->p[ 4]
                , node->p[ 5]
                , node->p[ 6]
                , node->p[ 7]
                , node->p[ 8]
                , node->p[ 9]
                , node->p[10]
                , node->p[11]
                , node->p[12]
                , node->p[13]
                , node->p[14]
                , node->p[15]
                , node->p[16]
                , node->p[17]
                , node->p[18]
                , node->p[19]
                , node->p[20]
                , node->p[21]
                , node->p[22]
                , node->p[23]
                , node->p[24]
                , node->p[25]
            );
        }
        else if (token[1] == "cpp")
        {
            printf(
                "{%.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f, %.27f,"
                " %.27f, %.27f, %.27f, %.27f, %.27f, %.27f}\n"
                , node->p[ 0]
                , node->p[ 1]
                , node->p[ 2]
                , node->p[ 3]
                , node->p[ 4]
                , node->p[ 5]
                , node->p[ 6]
                , node->p[ 7]
                , node->p[ 8]
                , node->p[ 9]
                , node->p[10]
                , node->p[11]
                , node->p[12]
                , node->p[13]
                , node->p[14]
                , node->p[15]
                , node->p[16]
                , node->p[17]
                , node->p[18]
                , node->p[19]
                , node->p[20]
                , node->p[21]
                , node->p[22]
                , node->p[23]
                , node->p[24]
                , node->p[25]
            );
#endif
        }
        rank_table_lock.unlock();
        return true;
    }));
    command_map.insert(std::make_pair("save", [&file, &rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        rank_table_lock.lock();
        std::ofstream ofs(file, std::ios::out | std::ios::binary);
        for (size_t i = 0; i < rank_table.size(); ++i)
        {
            ofs.write(reinterpret_cast<char const *>(&rank_table.at(i)->data), sizeof rank_table.at(i)->data);
        }
        ofs.flush();
        ofs.close();
        printf("%d node(s) saved\n", rank_table.size());
        rank_table_lock.unlock();
        return true;
    }));
    command_map.insert(std::make_pair("exit", [&file, &rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        rank_table_lock.lock();
        std::ofstream ofs(file, std::ios::out | std::ios::binary);
        for (size_t i = 0; i < rank_table.size(); ++i)
        {
            ofs.write(reinterpret_cast<char const *>(&rank_table.at(i)->data), sizeof rank_table.at(i)->data);
        }
        ofs.flush();
        ofs.close();
        rank_table_lock.unlock();
        exit(0);
        return true;
    }));
    command_map.insert(std::make_pair("help", [](std::vector<std::string> const &token)
    {
        printf(
            "help                 - ...\n"
            "view                 - view a match (press enter to stop)\n"
            "rank                 - show all nodes\n"
            "best                 - print current best\n"
            "rank [rank]          - show a node at rank\n"
            "rank [rank] [length] - show nodes at rank\n"
            "select [rank]        - select a node and view info\n"
            "set [index] [value]  - set node name or config which last selected\n"
            "copy [name]          - copy a new node which last selected\n"
            "save                 - ...\n"
            "exit                 - save & exit\n"
        );
        return true;
    }));
    std::string line, last;
    while (true)
    {
        std::getline(std::cin, line);
        if (view)
        {
            view = false;
            view_index = 0;
            continue;
        }
        std::vector<std::string> token;
        zzz::split(token, line, " ");
        if (token.empty())
        {
            line = last;
            zzz::split(token, line, " ");
        }
        if (token.empty())
        {
            continue;
        }
        auto find = command_map.find(token.front());
        if (find == command_map.end())
        {
            continue;
        }
        if (find->second(token))
        {
            printf("-------------------------------------------------------------\n");
            last = line;
            std::cout.flush();
        }
    }
}