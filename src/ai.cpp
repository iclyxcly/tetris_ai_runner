#ifdef _WIN32
#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall
#else
#define DECLSPEC_EXPORT
#define WINAPI
#define __cdecl
#endif

#include <ctime>
#include "tetris_core.h"
#include "search_simple.h"
#include "search_path.h"
#include "search_simulate.h"
#include "search_cautious.h"
#include "search_tspin.h"
#include "ai_ax.h"
#include "ai_zzz.h"
#include "ai_tag.h"
#include "ai_farter.h"
#include "ai_misaka.h"
#include "rule_st.h"
#include "rule_qq.h"
#include "rule_srs.h"
#include "rule_toj.h"
#include "rule_c2.h"
#include "random.h"

m_tetris::TetrisEngine<rule_st::TetrisRule, ai_zzz::Dig, search_path::Search> tetris_ai;
//m_tetris::TetrisEngine<rule_st::TetrisRule, ai_ax::AI, search_simple::Search> tetris_ai;
//m_tetris::TetrisEngine<rule_st::TetrisRule, ai_farteryhr::AI, search_simple::Search> tetris_ai;

extern "C" void attach_init()
{
    ege::mtsrand((unsigned int)(time(nullptr)));
}

//����AI���֣�����ʾ�ڽ�����
extern "C" DECLSPEC_EXPORT char const *WINAPI Name()
{
    static std::string name = tetris_ai.ai_name();
    return name.c_str();
}

/*
 ***********************************************************************************************
 * ���ڶ�next�汾��ST...���Լ�MOD����...�������http://misakamm.com/blog/504�����demo.cpp��AIPath
 ***********************************************************************************************
 * path ���ڽ��ղ������̲����أ������ַ�����
 *      'l': ����һ��
 *      'r': ����һ��
 *      'd': ����һ��
 *      'L': ���Ƶ�ͷ
 *      'R': ���Ƶ�ͷ
 *      'D': ���Ƶ��ף�����ճ�ϣ��ɼ����ƶ���
 *      'z': ��ʱ����ת
 *      'c': ˳ʱ����ת
 * �ַ���ĩβҪ��'\0'����ʾ��ز�������Ӳ���䣩
 *
 * ������֧������·��������������Ҫ�˺���ֻ��ʹ������һ���Ļ�����ɾ������������
 *
 ***********************************************************************************************
 * �����ļ�(ai.cpp)�ӹ����ų�,����demo.cpp�����Ϳ�����.���ֱ��ʹ�ñ�׼��ST����...�ᷢ��δ�������Ϊ!
 ***********************************************************************************************
 */
extern "C" DECLSPEC_EXPORT int WINAPI AIPath(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char const *nextPiece, char path[])
{
    if (!tetris_ai.prepare(boardW, boardH))
    {
        return 0;
    }
    m_tetris::TetrisMap map(boardW, boardH);
    for (int y = 0, add = 0; y < boardH; ++y, add += boardW)
    {
        for (int x = 0; x < boardW; ++x)
        {
            if (board[x + add] == '1')
            {
                map.top[x] = map.roof = y + 1;
                map.row[y] |= 1 << x;
                ++map.count;
            }
        }
    }
    m_tetris::TetrisBlockStatus status(curPiece, curX - 1, curY - 1, curR - 1);
    std::string next(nextPiece);
    m_tetris::TetrisNode const *node = tetris_ai.get(status);
    auto target = tetris_ai.run(map, node, next.data(), next.size(), 49).target;
    if (target != nullptr)
    {
        std::vector<char> ai_path = tetris_ai.make_path(node, target, map);
        std::memcpy(path, ai_path.data(), ai_path.size());
        path[ai_path.size()] = '\0';
    }
    return 0;
}
#define USE_V08 1
#define USE_THREAD 1
#define USE_PC 0

#if !USE_V08
#if USE_THREAD
m_tetris::TetrisThreadEngine<rule_toj::TetrisRule, ai_zzz::TOJ, search_tspin::Search> srs_ai;
#else
m_tetris::TetrisEngine<rule_toj::TetrisRule, ai_zzz::TOJ, search_tspin::Search> srs_ai;
#endif
#else
#if USE_THREAD
m_tetris::TetrisThreadEngine<rule_toj::TetrisRule, ai_zzz::TOJ_v08, search_tspin::Search> srs_ai;
#else
m_tetris::TetrisEngine<rule_toj::TetrisRule, ai_zzz::TOJ_v08, search_tspin::Search> srs_ai;
#endif
#endif
#if USE_PC
std::unique_ptr<m_tetris::TetrisThreadEngine<rule_toj::TetrisRule, ai_zzz::TOJ_PC, search_tspin::Search>> srs_pc;
#endif
std::mutex srs_ai_lock;

extern "C" DECLSPEC_EXPORT int __cdecl AIDllVersion()
{
    return 2;
}

extern "C" DECLSPEC_EXPORT char *__cdecl AIName(int level)
{
    static char name[200];
    strcpy(name, srs_ai.ai_name().c_str());
    return name;
}

/*
all 'char' type is using the characters in ' ITLJZSO'

field data like this:
00........   -> 0x3
00.0......   -> 0xb
00000.....   -> 0x1f

b2b: the count of special attack, the first one set b2b=1, but no extra attack. Have extra attacks when b2b>=2
combo: first clear set combo=1, so the comboTable in toj rule is [0, 0, 0, 1, 1, 2, 2, 3, ...]
next: array size is 'maxDepth'
x, y, spin: the active piece's x/y/orientation,
x/y is the up-left corner's position of the active piece.
see tetris_gem.cpp for the bitmaps.
curCanHold: indicates whether you can use hold on current move.
might be caused by re-think after a hold move.
canhold: false if hold is completely disabled.
comboTable: -1 is the end of the table.
*/
extern "C" DECLSPEC_EXPORT char *__cdecl TetrisAI(int overfield[], int field[], int field_w, int field_h, int b2b, int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, int level, int player)
{
    static char result_buffer[8][1024];
    char *result = result_buffer[player];
    std::unique_lock<std::mutex> lock(srs_ai_lock);

    if (field_w != 10 || field_h != 22 || !srs_ai.prepare(10, 40))
    {
        *result = '\0';
        return result;
    }
#if USE_PC
    if (!srs_pc || srs_pc->context() != srs_ai.context())
    {
        srs_pc.reset(new m_tetris::TetrisThreadEngine<rule_toj::TetrisRule, ai_zzz::TOJ_PC, search_tspin::Search>(srs_ai.context()));
        memset(srs_pc->status(), 0, sizeof *srs_pc->status());
    }
#endif
    m_tetris::TetrisMap map(10, 40);
    for (size_t d = 0, s = 22; d < 23; ++d, --s)
    {
        map.row[d] = field[s];
    }
    for (size_t d = 23, s = 0; s < 8; ++d, ++s)
    {
        map.row[d] = overfield[s];
    }
    for (int my = 0; my < map.height; ++my)
    {
        for (int mx = 0; mx < map.width; ++mx)
        {
            if (map.full(mx, my))
            {
                map.top[mx] = map.roof = my + 1;
                map.row[my] |= 1 << mx;
                ++map.count;
            }
        }
    }
    srs_ai.search_config()->allow_rotate_move = false;
    srs_ai.search_config()->allow_180 = can180spin;
    srs_ai.search_config()->allow_d = true;
    srs_ai.search_config()->is_20g = false;
    srs_ai.search_config()->last_rotate = false;
#if USE_PC
    *srs_pc->search_config() = *srs_ai.search_config();
#endif
    struct ComboTable {
        int table[24] = {0};
        int table_max = 0;
    };
    static ComboTable table;
    if (table.table_max == 0) {
        size_t max = 0;
        while (comboTable[max] != -1)
        {
            table.table[max] = comboTable[max];
            ++max;
        }
        table.table_max = max - 1;
    }
    srs_ai.ai_config()->table = table.table;
    srs_ai.ai_config()->table_max = table.table_max;
#if USE_PC
    srs_pc->ai_config()->table = table.table;
    srs_pc->ai_config()->table_max = table.table_max;
#endif
    srs_ai.memory_limit(256ull << 20);
#if !USE_V08
    srs_ai.ai_config()->safe = srs_ai.ai()->get_safe(map, active);
    //NAME: init_1
    //GEN: 2139
    //SCORE: 107.38
    //APL: 1.14
    //APP: 0.65
    srs_ai.ai_config()->param = { 28.701623013737663114852693980, 295.279853398865384406235534698, 377.682867211490929548745043576, 194.578524021247972086712252349, 358.517545721265832980861887336, 65.275259254247743001542403363, 5.637687051561649731468151003, -262.745777531149030892265727744, -129.939368423034323996034800075, -88.102622536349585402604134288, 31.397345004664671819227805827, 504.624310570664931674400577322, 164.528490037548010604950832203, 0.283101085502819316275235906, -0.074027859430396358852988214, -28.574672038323836176232362050, -13.843036736670310204999623238, -27.359081303948350694099644898, -59.981404167144120265220408328, -47.693497695867975494365964551, 13.285092122100508049697964452, -0.084367316356987825942681525, 0.442901855701022972855440685, -5.413297620552509847868805082, 10.537138714651431925517499621, -0.506376435172552707975057729, 4.699163025452855357855241891, 92.041294278090987290852353908, 0.279778139865527575302905916 };
    srs_ai.status()->death = 0;
    srs_ai.status()->combo = combo;
    if (srs_ai.status()->under_attack != upcomeAtt)
    {
        srs_ai.update();
    }
    srs_ai.status()->under_attack = upcomeAtt;
    srs_ai.status()->map_rise = 0;
    srs_ai.status()->b2b = !!b2b;
    srs_ai.status()->acc_value = 0;
    srs_ai.status()->like = 0;
    srs_ai.status()->value = 0;
    ai_zzz::TOJ::Status::init_t_value(map, srs_ai.status()->t2_value, srs_ai.status()->t3_value);
#else
    //NAME: init_3
    //GEN: 198
    //SCORE: 113.19
    //APL: 0.99
    //APP: 0.68
    srs_ai.ai_config()->param = { 160.243384434415133910079021007, 156.539465058019032994707231410, 160.605627401933759301755344495, 163.127975560301422319753328338, 155.564294819757776622282108292, 152.430622736739081801715656184, 166.645372979675357782980427146, 164.373335634876582389551913366, 261.998040047964082077669445425, 30.121015999186727896130832960, 5.460668455963028300459427555, 2.070224220332052667004063551, -0.187748010973191881145538673, -1.438471598779677229629214708, -0.390319790142479028371980121, -1.128847571557698215016785070, 0.726473838348016598409628841, -0.659246515903566310257133409, 1.589063943142735135793941481, 0.259838462574848882979949849, 0.778246814382433593770826974, 0.791063271826942471243171440, 7.351630369119032337721364456, 12.957728502535514891746970534, 39.927561008107588236271112692, 1.186079224627106132317067022 };
    srs_ai.status()->max_combo = 0;
    srs_ai.status()->max_attack = 0;
    srs_ai.status()->death = 0;
    srs_ai.status()->combo = combo;
    srs_ai.status()->attack = 0;
    srs_ai.status()->combo_attack = 0;
    if (srs_ai.status()->under_attack != upcomeAtt)
    {
        srs_ai.update();
    }
    srs_ai.status()->under_attack = upcomeAtt;
    srs_ai.status()->map_rise = 0;
    srs_ai.status()->b2b = !!b2b;
    srs_ai.status()->like = 0;
    srs_ai.status()->value = 0;
#endif
#if USE_PC
    srs_pc->memory_limit(768ull << 20);
    srs_pc->status()->attack = 0;
    srs_pc->status()->b2b = !!b2b;
    srs_pc->status()->combo = combo;
    srs_pc->status()->like = 0;
    srs_pc->status()->pc = false;
    srs_pc->status()->recv_attack = 0;
    if (srs_pc->status()->under_attack != upcomeAtt)
    {
        srs_pc->update();
    }
    srs_pc->status()->under_attack = upcomeAtt;
    srs_pc->status()->value = 0;
#endif

    m_tetris::TetrisBlockStatus status(active, x, 22 - y, (4 - spin) % 4);
    m_tetris::TetrisNode const *node = srs_ai.get(status);
    static double const base_time = std::pow(100, 1.0 / 8);
    if (canhold)
    {
#if USE_PC
        srs_pc->run_hold(map, node, hold, curCanHold, next, maxDepth, time_t(0));
#endif
        auto run_result = srs_ai.run_hold(map, node, hold, curCanHold, next, maxDepth, time_t(std::pow(base_time, level)));
#if USE_PC
        auto pc_result = srs_pc->run_hold(map, node, hold, curCanHold, next, maxDepth, time_t(0));
        if (pc_result.status.pc)
        {
            run_result.change_hold = pc_result.change_hold;
            run_result.target = pc_result.target;
        }
#endif
        if (run_result.change_hold)
        {
            result++[0] = 'v';
            if (run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai.make_path(srs_ai.context()->generate(run_result.target->status.t), run_result.target, map);
                std::memcpy(result, ai_path.data(), ai_path.size());
                result += ai_path.size();
            }
        }
        else
        {
            if (run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai.make_path(node, run_result.target, map);
                std::memcpy(result, ai_path.data(), ai_path.size());
                result += ai_path.size();
            }
        }
    }
    else
    {
#if USE_PC
        srs_pc->run(map, node, next, maxDepth, time_t(0));
#endif
        auto run_result = srs_ai.run(map, node, next, maxDepth, time_t(std::pow(base_time, level)));
#if USE_PC
        auto pc_result = srs_pc->run(map, node, next, maxDepth, time_t(0));
        if (pc_result.status.pc)
        {
            run_result.change_hold = pc_result.change_hold;
            run_result.target = pc_result.target;
        }
#endif
        if (run_result.target != nullptr)
        {
            std::vector<char> ai_path = srs_ai.make_path(node, run_result.target, map);
            std::memcpy(result, ai_path.data(), ai_path.size());
            result += ai_path.size();
        }
    }
    result++[0] = 'V';
    result[0] = '\0';
    return result_buffer[player];
}

class QQTetrisSearch
{

public:
    enum Config
    {
        Simple, Simulate, Path
    };
    void init(m_tetris::TetrisContext const *context, Config const *config)
    {
        simple_.init(context);
        simulate_.init(context);
        path_.init(context);
        config_ptr = config;
    }
    std::vector<char> make_path(m_tetris::TetrisNode const *node, m_tetris::TetrisNode const *land_point, m_tetris::TetrisMap const &map)
    {
        switch (*config_ptr)
        {
        case Simple:
            return simple_.make_path(node, land_point, map);
        case Simulate:
            return simulate_.make_path(node, land_point, map);
        case Path:
            return path_.make_path(node, land_point, map);
        default:
            return std::vector<char>();
        }
    }
    std::vector<m_tetris::TetrisNode const *> const *search(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node, size_t depth)
    {
        switch (*config_ptr)
        {
        case Simple:
            return simple_.search(map, node, depth);
        case Simulate:
            return simulate_.search(map, node, depth);
        case Path:
            return path_.search(map, node, depth);
        default:
            empty_.resize(1);
            empty_.front() = node->drop(map);
            return &empty_;
        }
    }
private:
    Config const *config_ptr;
    search_simple::Search simple_;
    search_simulate::Search simulate_;
    search_path::Search path_;
    std::vector<m_tetris::TetrisNode const *> empty_;
};
m_tetris::TetrisEngine<rule_qq::TetrisRule, ai_zzz::qq::Attack, QQTetrisSearch> qq_ai;

extern "C" DECLSPEC_EXPORT int __cdecl QQTetrisAI(int boardW, int boardH, int board[], char nextPiece[], int curX, int curY, int curR, int level, int mode, char path[], size_t limit)
{
    if (!qq_ai.prepare(boardW, boardH))
    {
        *path = '\0';
        return 0;
    }
    m_tetris::TetrisMap map(boardW, boardH);
    std::memcpy(map.row, board, boardH * sizeof(int));
    for (int my = 0; my < map.height; ++my)
    {
        for (int mx = 0; mx < map.width; ++mx)
        {
            if (map.full(mx, my))
            {
                map.top[mx] = map.roof = my + 1;
                map.row[my] |= 1 << mx;
                ++map.count;
            }
        }
    }
    m_tetris::TetrisBlockStatus status(nextPiece[0], curX, curY, (4 - curR) % 4);
    size_t next_length = std::strlen(nextPiece) - 1;
    if (level < 10)
    {
        next_length = std::min<size_t>(level, next_length);
    }
    std::string next_str(nextPiece + 1, nextPiece + 1 + next_length);
    if (next_length <= 2)
    {
        std::string next_new = "?";
        for (auto c : next_str)
        {
            next_new += c;
            next_new += '?';
        }
        next_str.swap(next_new);
    }
    if (level == 10)
    {
        *qq_ai.search_config() = QQTetrisSearch::Path;
    }
    else if (mode == 0 || map.count <= boardW * 2)
    {
        *qq_ai.search_config() = QQTetrisSearch::Simulate;
    }
    else
    {
        *qq_ai.search_config() = QQTetrisSearch::Simple;
    }
    qq_ai.ai_config()->level = level;
    qq_ai.ai_config()->mode = mode;
    qq_ai.status()->land_point = 0;
    qq_ai.status()->attack = 0;
    qq_ai.status()->rubbish = 0;
    qq_ai.status()->value = 0;
    m_tetris::TetrisNode const *node = qq_ai.get(status);
    while (node == nullptr && status.y > 0)
    {
        --status.y;
        node = qq_ai.get(status);
    }
    auto target = qq_ai.run(map, node, next_str.data(), next_str.length(), 60).target;
    std::vector<char> ai_path;
    if (target != nullptr)
    {
        ai_path = qq_ai.make_path(node, target, map);
        std::memcpy(path, ai_path.data(), ai_path.size());
    }
    path[ai_path.size()] = 'V';
    path[ai_path.size() + 1] = '\0';
    return 0;
}

m_tetris::TetrisThreadEngine<rule_c2::TetrisRule, ai_zzz::C2, search_cautious::Search> c2_ai;

struct c2_out_put
{
    char move;
    int8_t x;
    int8_t y;
    uint8_t r;
};

struct c2_param
{
    int boardW;
    int boardH;
    int const *board;
    char const *nextPiece;
    int curX, curY, curR;
    int safe;
    int combo;
    int combo_limit;
    int danger;
    c2_out_put *path;
    size_t ai_width;
    size_t limit;
    int mode;
    int vp;
    int soft_drop;
};


extern "C" DECLSPEC_EXPORT int __cdecl C2TetrisAI(c2_param *param)
{
    int const &boardW = param->boardW;
    int const &boardH = param->boardH;
    int const *board = param->board;
    char const *nextPiece = param->nextPiece;
    int const &curX = param->curX, &curY = param->curY, &curR = param->curR;
    int const &safe = param->safe;
    int const &combo = param->combo;
    int const &combo_limit = param->combo_limit;
    int const &danger = param->danger;
    c2_out_put *path = param->path;
    size_t const &limit = param->limit;
    int const &mode = param->mode;
    int const &vp = param->vp;
    int const &soft_drop = param->soft_drop;
    if (!c2_ai.prepare(boardW, boardH))
    {
        path[0] = { '\0' };
        return 0;
    }
    m_tetris::TetrisMap map(boardW, boardH);
    std::memcpy(map.row, board, boardH * sizeof(int));
    for (int my = 0; my < map.height; ++my)
    {
        for (int mx = 0; mx < map.width; ++mx)
        {
            if (map.full(mx, my))
            {
                map.top[mx] = map.roof = my + 1;
                map.row[my] |= 1 << mx;
                ++map.count;
            }
        }
    }
    c2_ai.memory_limit(1ull << 30);
    c2_ai.search_config()->fast_move_down = true;
    c2_ai.ai_config()->p =
    {
        2.87224, 0.372169, 0.102604, 0.723501, 3.08721, 0.802789, -0.786174, 107.713, -0.540719, 109.116, -3.84305, 116.58, -1.00066, 49.7899, -1.23986, 391.808, -4.30493, 91.0623, -1.60608, 67.7934, 2.36365, 46016.9, 34.2515, 0.285739,
    };
    c2_ai.ai_config()->p_rate = 1;
    c2_ai.ai_config()->safe = safe;
    c2_ai.ai_config()->mode = mode;
    c2_ai.ai_config()->danger = danger;
    c2_ai.ai_config()->soft_drop = soft_drop;
    c2_ai.status()->combo = combo;
    c2_ai.status()->combo_limit = combo_limit;
    c2_ai.status()->value = 0;
    m_tetris::TetrisBlockStatus status(nextPiece[0], curX, curY, curR);
    size_t next_length = nextPiece[1] == ' ' ? 0 : 1;
    std::string next;
    if (vp)
    {
        next += '?';
    }
    for (char const *n = nextPiece + 1, *const ne = nextPiece + 1 + next_length; n != ne; ++n)
    {
        next += *n;
        if (vp)
        {
            next += '?';
        }
    }
    m_tetris::TetrisNode const *node = c2_ai.get(status);
    auto target = c2_ai.run(map, node, next.data(), next.size(), limit).target;
    std::vector<char> ai_path;
    size_t size = 0;
    if (target != nullptr)
    {
        ai_path = c2_ai.make_path(node, target, map);
        node->open(map);
        for (char c : ai_path)
        {
            switch (c)
            {
            case 'L':
                while (node->move_left != nullptr && node->move_left->check(map))
                {
                    node = node->move_left;
                }
                break;
            case 'R':
                while (node->move_right != nullptr && node->move_right->check(map))
                {
                    node = node->move_right;
                }
                break;
            case 'D':
                node = node->drop(map);
                break;
            case 'l':
                if (node->move_left != nullptr && node->move_left->check(map))
                {
                    node = node->move_left;
                }
                break;
            case 'r':
                if (node->move_right != nullptr && node->move_right->check(map))
                {
                    node = node->move_right;
                }
                break;

            case 'z':
                for (auto wall_kick_node : node->wall_kick_counterclockwise)
                {
                    if (wall_kick_node)
                    {
                        if (wall_kick_node->check(map))
                        {
                            node = wall_kick_node;
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                break;
            case 'x':
                for (auto wall_kick_node : node->wall_kick_opposite)
                {
                    if (wall_kick_node)
                    {
                        if (wall_kick_node->check(map))
                        {
                            node = wall_kick_node;
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                break;
            case 'c':
                for (auto wall_kick_node : node->wall_kick_clockwise)
                {
                    if (wall_kick_node)
                    {
                        if (wall_kick_node->check(map))
                        {
                            node = wall_kick_node;
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                break;
            default:
                break;
            }
            path[size++] = { c, node->status.x, node->status.y, node->status.r };
        }
    }
    if (size == 0)
    {
        path[size++] = { 'V', int8_t(curX), int8_t(curY), uint8_t(curR) };
    }
    else
    {
        path[size] = path[size - 1];
        path[size++].move = 'V';
    }
    path[size++] = { '\0' };
    return target == nullptr ? 0 : target->attach(c2_ai.context().get(), map);
}
