#include <iostream>
#include "tetris_core.h"
#include "search_amini.h"
#include "ai_zzz.h"
#include "rule_io.h"
#include "random.h"
#include "ai_setting.h"
#include <vector>
#include <nlohmann/json.hpp>
#include <mutex>
#include <algorithm>
#include <string>
#include <future>
#include <chrono>
#include <cstring>

using JSON = nlohmann::json;
typedef m_tetris::TetrisEngine<rule_io::TetrisRule, ai_zzz::IO_v08, search_amini::Search> Bot;
struct BotInstance
{
    uint64_t bot_id;
    std::unique_ptr<Bot> bot;
    clock_t margin_start_time = 0, elapsed_time = 0, start_time = 0;
    BotInstance(const unsigned long long &id) : bot_id(id)
    {
        bot = std::make_unique<Bot>();
        bot->prepare(10, 40);
    }
    void new_game()
    {
        start_time = clock();
        margin_start_time = 0;
        elapsed_time = 0;
        bot->status()->is_margin = false;
    }
};

std::recursive_mutex srs_ai_lock;
std::recursive_mutex async_lock;
std::vector<BotInstance> bots;

void print_board(const m_tetris::TetrisMap &map)
{
    printf("Board:\n");
    printf("----");
    for (int i = 0; i < map.width; ++i)
    {
        printf("--");
    }
    printf("\n");
    for (int my = 20; my >= 0; --my)
    {
        printf("|");
        for (int mx = 0; mx < map.width; ++mx)
        {
            if (map.full(mx, my))
            {
                printf("[]");
            }
            else
            {
                printf("  ");
            }
        }
        printf("|\n");
    }
    printf("----");
    for (int i = 0; i < map.width; ++i)
    {
        printf("--");
    }
}

std::string ai_run_thread(const int &bot_id, const JSON &data)
{
    std::string result;
    srs_ai_lock.lock();
    auto it = std::find_if(bots.begin(), bots.end(), [&](const BotInstance &bot)
                           { return bot.bot_id == bot_id; });
    if (it == bots.end())
    {
        srs_ai_lock.unlock();
        return "";
    }
    srs_ai_lock.unlock();
    BotInstance &bot = *it;

    auto &srs_ai = bot.bot;

    bool can_hold = data["can_hold"].get<bool>();
    bool can_pc = data["can_pc"].get<bool>();
    char active = data["active"].get<std::string>()[0];
    char hold = data["hold"].get<std::string>()[0];
    int y = data["y"].get<int>();
    char next[32];
    int maxDepth = std::min<int>(data["next_size"].get<int>(), data["next"].get<std::string>().size());
    for (int i = 0; i < maxDepth; ++i)
    {
        next[i] = data["next"].get<std::string>()[i];
    }
    int b2b = data["b2b"].get<int>();
    int combo = data["combo"].get<int>();
    int upcomeAtt = data["pending"].get<int>();

    m_tetris::TetrisMap map(10, 40);
    for (size_t d = 0; d < 23; ++d)
    {
        map.row[d] = data["field"][d].get<uint32_t>();
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
    srs_ai->search_config()->allow_rotate_move = false;
    srs_ai->search_config()->allow_180 = true;
    srs_ai->search_config()->allow_D = true;
    srs_ai->search_config()->allow_LR = false;
    srs_ai->search_config()->allow_d = false;
    srs_ai->search_config()->last_rotate = false;
    srs_ai->search_config()->is_20g = false;

    srs_ai->ai_config()->param = {128.848632018967037993206758983, 159.486229165944052965642185882, 161.917442316092603959987172857, 81.770591639349177626172604505, 381.778776257560934936918783933, 98.094088345045122423471184447, 34.677952239613162532805290539, -0.435441341531714487533832880, 129.220619858914346878009382635, 0.911925860653483022488785537, 3.743571313305298797757814100, 3.153364454826400375964112754, 0.007065131195186014588516255, -0.081683675915617898199982960, -0.954530616937390941068031225, 1.612455139641955748075474730, 0.570015487183247460123425299, 1.093367709554965427898309827, 1.511144844202827464130223234, 1.007928243238619847588211087, -0.740554584228065859718981301, 0.104364933113540087061821282, 8.660904648990943144326593028, 12.172353417045528090056905057, 6.888583310333941334135943180, 30.511480066561279755887881038, 1.585887060974324525020051624};
    srs_ai->status()->max_combo = 0;
    srs_ai->status()->attack = 0;
    srs_ai->status()->b2bcnt = b2b;
    srs_ai->status()->board_fill = map.count;

    struct ComboTable
    {
        int table[24] = {0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3};
        int table_max = 18;
    } table;

    srs_ai->ai_config()->table = table.table;
    srs_ai->ai_config()->table_max = table.table_max;

    srs_ai->memory_limit(32ull << 20);

    bot.elapsed_time = (clock() - bot.start_time) / CLOCKS_PER_SEC * 1000;
    if (!srs_ai->status()->is_margin)
    {
        if (bot.elapsed_time > GARBAGE_MARGIN_TIME)
        {
            srs_ai->status()->is_margin = true;
            srs_ai->status()->start_count = clock();
        }
    }
    srs_ai->status()->death = 0;
    srs_ai->status()->combo = combo;
    if (srs_ai->status()->under_attack != upcomeAtt)
    {
        srs_ai->update();
    }
    srs_ai->status()->under_attack = upcomeAtt;
    srs_ai->status()->map_rise = 0;
    srs_ai->status()->like = 0;
    srs_ai->status()->value = 0;
    srs_ai->status()->pc = can_pc;

    m_tetris::TetrisBlockStatus status(active, 3, y, 0);
    m_tetris::TetrisNode const *node = srs_ai->get(status);
    if (can_hold)
    {
        auto run_result = srs_ai->run_hold(map, node, hold, true, next, maxDepth, time_t(20));
        if (run_result.change_hold)
        {
            result += "v";
            if (run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai->make_path(srs_ai->context()->generate(run_result.target->status.t), run_result.target, map);
                for (auto &c : ai_path)
                {
                    result += c;
                }
            }
        }
        else
        {
            if (run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai->make_path(node, run_result.target, map);
                for (auto &c : ai_path)
                {
                    result += c;
                }
            }
        }
    }
    else
    {
        auto run_result = srs_ai->run(map, node, next, maxDepth, time_t(20));
        if (run_result.target != nullptr)
        {
            std::vector<char> ai_path = srs_ai->make_path(node, run_result.target, map);
            for (auto &c : ai_path)
            {
                result += c;
            }
        }
    }
    result += 'V';
    return result;
}

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

thread_local std::unordered_map<int, std::string> buffer_map;

extern "C" EXPORT void new_bot(int bot_id)
{
    srs_ai_lock.lock();
    bots.emplace_back(BotInstance(bot_id));
    srs_ai_lock.unlock();
}

extern "C" EXPORT void end_bot(int bot_id)
{
    srs_ai_lock.lock();
    auto it = std::remove_if(bots.begin(), bots.end(), [&](const BotInstance &bot)
                             { return bot.bot_id == bot_id; });
    if (it != bots.end())
    {
        bots.erase(it, bots.end());
    }
    srs_ai_lock.unlock();
}

extern "C" EXPORT void new_game(int bot_id)
{
    srs_ai_lock.lock();
    auto it = std::find_if(bots.begin(), bots.end(), [&](const BotInstance &bot)
                           { return bot.bot_id == bot_id; });
    if (it == bots.end())
    {
        srs_ai_lock.unlock();
        return;
    }
    srs_ai_lock.unlock();
    it->new_game();
}

extern "C" EXPORT const char* TetrisAI(int bot_id, const char *param)
{
    JSON data = JSON::parse(param);
    buffer_map[bot_id] = ai_run_thread(bot_id, data);
    // printf("Move: %s\n", buffer_map[bot_id].c_str());
    return buffer_map[bot_id].c_str();
}