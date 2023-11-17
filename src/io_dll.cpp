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
#include "search_tspin.h"
#include "ai_zzz.h"
#include "rule_io.h"
#include "rule_toj.h"
#include "random.h"
#include "ai_setting.h"

/*
 ***********************************************************************************************
 * 用于多next版本的ST...我自己MOD过的...参与比赛http://misakamm.com/blog/504请参照demo.cpp的AIPath
 ***********************************************************************************************
 * path 用于接收操作过程并返回，操作字符集：
 *      'l': 左移一格
 *      'r': 右移一格
 *      'd': 下移一格
 *      'L': 左移到头
 *      'R': 右移到头
 *      'D': 下移到底（但不粘上，可继续移动）
 *      'z': 逆时针旋转
 *      'c': 顺时针旋转
 * 字符串末尾要加'\0'，表示落地操作（或硬降落）
 *
 * 本函数支持任意路径操作，若不需要此函数只想使用上面一个的话，则删掉本函数即可
 *
 ***********************************************************************************************
 * 将此文件(ai.cpp)从工程排除,增加demo.cpp进来就可以了.如果直接使用标准的ST调用...会发生未定义的行为!
 ***********************************************************************************************
 */
#if !USE_MISAMINO
#if USE_THREAD
#if !USE_V08
m_tetris::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::IO, search_tspin::Search> srs_ai;
#else 
m_tetris::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::IO_v08, search_tspin::Search> srs_ai;
#endif
#else
#if !USE_V08
m_tetris::TetrisEngine<rule_io::TetrisRule, ai_zzz::IO, search_tspin::Search> srs_ai;
#else
m_tetris::TetrisEngine<rule_io::TetrisRule, ai_zzz::IO_v08, search_tspin::Search> srs_ai;
#endif
#endif
#if USE_PC
std::unique_ptr<m_tetris::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::TOJ_PC, search_tspin::Search>> srs_pc;
#endif
#else
#if USE_THREAD
#if !USE_V08
m_tetris::TetrisThreadEngine<rule_toj::TetrisRule, ai_zzz::IO, search_tspin::Search> srs_ai;
#else
m_tetris::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::IO_v08, search_tspin::Search> srs_ai;
#endif
#else
#if !USE_V08
m_tetris::TetrisEngine<rule_toj::TetrisRule, ai_zzz::IO, search_tspin::Search> srs_ai;
#else
m_tetris::TetrisEngine<rule_toj::TetrisRule, ai_zzz::IO_v08, search_tspin::Search> srs_ai;
#endif
#endif
#if USE_PC
std::unique_ptr<m_tetris::TetrisThreadEngine<rule_toj::TetrisRule, ai_zzz::TOJ_PC, search_tspin::Search>> srs_pc;
#endif
#endif
std::mutex srs_ai_lock;

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

clock_t now = 0, avg = 0, pieces = 0, influency = 0, start_count = 0, elapsed_time = 0, init_time = 0, a = 0;
extern "C" void attach_init()
{
    ege::mtsrand((unsigned int)(time(nullptr)));
}
#if USE_MISAMINO
extern "C" DECLSPEC_EXPORT int __cdecl AIDllVersion()
{
    return 2;
}

extern "C" DECLSPEC_EXPORT char* __cdecl AIName(int level)
{
    static char name[200];
    strcpy(name, srs_ai.ai_name().c_str());
    return name;
}
extern "C" DECLSPEC_EXPORT char* __cdecl TetrisAI(int overfield[], int field[], int field_w, int field_h, int b2b, int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, int level, int player)
#else
extern "C" DECLSPEC_EXPORT char* __cdecl Ping()
{
    char* ping = new char[100];
    strcpy(ping, "pong\0");
    return ping;
}
extern "C" DECLSPEC_EXPORT void __cdecl NewGame()
{
    pieces = time_t(0), now = clock(), a = 0, init_time = clock(), influency = time_t(0);
}
extern "C" DECLSPEC_EXPORT char* __cdecl TetrisAI(int field[], int b2b, int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, bool can180spin, int upcomeAtt, int maxDepth, double pps, bool burst, bool pc, bool isEnded, int player)
#endif
{
    clock_t tick = clock();
    static char result_buffer[8][1024];
    char* result = result_buffer[player];
    std::unique_lock<std::mutex> lock(srs_ai_lock);

    if (!srs_ai.prepare(10, 40))
    {
        *result = '\0';
        return result;
    }
#if USE_PC
    if (!srs_pc || srs_pc->context() != srs_ai.context())
    {
#if !USE_MISAMINO
        srs_pc.reset(new m_tetris::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::TOJ_PC, search_tspin::Search>(srs_ai.context()));
#else
        srs_pc.reset(new m_tetris::TetrisThreadEngine<rule_toj::TetrisRule, ai_zzz::TOJ_PC, search_tspin::Search>(srs_ai.context()));
#endif
        memset(srs_pc->status(), 0, sizeof * srs_pc->status());
    }
#endif
    m_tetris::TetrisMap map(10, 40);
#if USE_MISAMINO
    for (size_t d = 0, s = 22; d < 23; ++d, --s)
    {
        map.row[d] = field[s];
    }
#else
    for (size_t d = 0, s = 22; d < 23; ++d, --s)
    {
        map.row[d] = field[s];
    }
#endif
#if USE_MISAMINO
    for (size_t d = 23, s = 0; s < 8; ++d, ++s)
    {
        map.row[d] = overfield[s];
    }
#endif
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
    srs_ai.search_config()->allow_D = true;
    srs_ai.search_config()->allow_LR = true;
    srs_ai.search_config()->allow_d = true;
    srs_ai.search_config()->last_rotate = true;
    srs_ai.search_config()->is_20g = false;
#if !USE_V08
    srs_ai.status()->like = 0;
    srs_ai.ai_config()->safe = srs_ai.ai()->get_safe(map, active);
    ai_zzz::IO::Status::init_t_value(map, srs_ai.status()->t2_value, srs_ai.status()->t3_value);
    srs_ai.status()->b2b = !!b2b;
    srs_ai.status()->acc_value = 0;
    //NAME: init_24
    //GEN: 58
    //SCORE: 1596.95
    //DIFF: 93.30
    srs_ai.ai_config()->param = { 10.695327826494718692629248835, 9.261072057201415574922975793, 12.140335837873616142701393983, 15.382651766125240655469497142, 6.678204554058711828190553206, 5.897181596456438690267987113, 0.269416813375891961435115718, 0.391484929653500624002759878, 5.822108932702665384795182035, -5.201122502596883023784357647, 0.056575340003034650659952121, 1.645379752415937701925940928, 7.339142290119831102401803946, -0.013090860808305978743582010, -0.006922730671346907088681277, -2.038228942160984047404781450, -1.435855059887167994858714337, -0.100653160429101329320644709, -1.955148834179624817153353433, -0.740710718282400981671287354, -0.366645080200634798650582979, 0.062370184041030451993492534, -0.486575299365698377140887487, -0.807740828371587227252348384, -0.354631751564184027891712958, 0.496458131349682474375839547, -0.800817533473961895573722813, 1.338630117037470590446446295, 0.859673565668164108011239932 };
#else
    //NAME: init_16
    //GEN: 135
    //SCORE: 1601.47
    //DIFF: 99.28
    srs_ai.ai_config()->param = { 1.019339518, 0.949881975, 0.316346926, 1.670004485, 0.441530016, 0.829254221, 0.537486549, 0.052767246, 0.003979570, 0.282236974, 0.285383552, 8.163867067, 7.855438436, 8.382116605, 7.903386407, 8.122158599, 1.522100429, 0.667555193, -1.464319945, -0.890483606, -15.159808333, -16.455846053, -15.736087104, 15.959250750, 8.521046089, 0.478700698, 1.883695917, 3.211028413, 8.823854416, 3.982410719, 1.437406834 };
    srs_ai.status()->attack = 0;
    srs_ai.status()->b2b_attack = 0;
    srs_ai.status()->combo_attack = 0;
    srs_ai.status()->mul_attack = 0;
    srs_ai.status()->b2b = b2b;
    srs_ai.status()->margin_multiplier = ((clock() - srs_ai.status()->start_count) / 1000.0) * GARBAGE_INCREASE;
#endif
#if USE_PC
    * srs_pc->search_config() = *srs_ai.search_config();
#endif
#if !USE_MISAMINO
    // fix for lock delay (suicide bug not fixed)
    int i = 0;
    int j = 0;
    switch (active)
    {
    case 'S':
        i = (map.full(3, 20) || map.full(4, 20)) ? 1 : 0;
        j = (map.full(3, 19) || map.full(4, 19)) ? 2 : 1;
        break;
    case 'L':
    case 'J':
    case 'T':
        i = (map.full(3, 20) || map.full(4, 20) || map.full(5, 20)) ? 1 : 0;
        j = (map.full(3, 19) || map.full(4, 19) || map.full(5, 19)) ? 2 : 1;
        break;
    case 'O':
    case 'Z':
        i = (map.full(4, 20) || map.full(5, 20)) ? 1 : 0;
        j = (map.full(4, 19) || map.full(5, 19)) ? 2 : 1;
        break;
    case 'I':
        i = (map.full(3, 20) || map.full(4, 20) || map.full(5, 20) || map.full(6, 20)) ? 1 : 0;
        j = (map.full(3, 19) || map.full(4, 19) || map.full(5, 19) || map.full(6, 19)) ? 2 : 1;
        break;
    }
    struct ComboTable {
        int table[24] = { 0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,2,2,3 };
        int table_max = 18;
    }table;
#else
    struct ComboTable {
        int table[24] = { 0 };
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
#endif
#if USE_PC
    srs_pc->ai_config()->table = table.table;
    srs_pc->ai_config()->table_max = table.table_max;
#endif
#if !USE_V08
    srs_ai.ai_config()->table = table.table;
    srs_ai.ai_config()->table_max = table.table_max;
#endif
    srs_ai.memory_limit(1024ull << 20);
    srs_ai.status()->is_margin = elapsed_time > GARBAGE_MARGIN_TIME;
    srs_ai.status()->death = 0;
    srs_ai.status()->combo = combo;
    srs_ai.status()->under_attack = upcomeAtt;
    srs_ai.status()->map_rise = 0;
    srs_ai.status()->value = 0;
    srs_ai.status()->like = 0;
#if !USE_MISAMINO
    srs_ai.status()->pc = pc;
#else
    srs_ai.status()->pc = true;
#endif
#if USE_PC
    srs_pc->memory_limit(768ull << 20);
    srs_pc->status()->attack = 0;
    srs_pc->status()->b2b = b2b;
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

#if !USE_MISAMINO
    m_tetris::TetrisBlockStatus status(active, x, 22 - (y - i), (4 - spin) % 4);
    elapsed_time = clock() - init_time;
    ++pieces, avg = (clock() - now) / pieces;
    if (avg > time_t(1000) / pps)influency = time_t(0);
    else influency += time_t(20), influency = std::min(clock_t(300), influency);
    if (a == 0)
    {
        if (elapsed_time > GARBAGE_MARGIN_TIME)
        {
            srs_ai.status()->is_margin = true;
            srs_ai.status()->start_count = clock();
            a = 1;
        }
    }
    time_t f = 0;
    if (burst) {
        if (!canhold)
        {
            f = time_t(((1150 + influency) / pps) - std::min((map.count * 2) + (upcomeAtt * 10) + 0.0, ((1100 + influency) / pps) / 1.8));
        }
        else
        {
            f = time_t(((1100 + influency) / pps) - std::min((map.count * 2) + (upcomeAtt * 10) + 0.0, ((1100 + influency) / pps) / 1.8));
        }
    }
    else {
        if (!canhold)
        {
            if (avg > time_t(1000) / pps)influency -= time_t(1);
        }
        else
        {
            if (avg > time_t(1000) / pps)influency -= time_t(1);
        }
            f = time_t((900 + influency) / pps);
    }
    time_t think_limit = f > time_t(PERFORMANCE_MODE ? 700 : 100) ? time_t(PERFORMANCE_MODE ? 700 : 100) : f;
    if (j == 2 && f - think_limit > 300) {
        f = time_t(400);
    }
#else
    m_tetris::TetrisBlockStatus status(active, x, 22 - y, (4 - spin) % 4);
    time_t think_limit = (time_t)(std::pow(std::pow(100, 1.0 / 8), level));
#endif
    m_tetris::TetrisNode const* node = srs_ai.get(status);
    srs_ai.update();
    if (canhold)
    {
#if USE_PC
        srs_pc->run_hold(map, node, hold, curCanHold, next, maxDepth, time_t(0));
#endif
        auto run_result = srs_ai.run_hold(map, node, hold, curCanHold, next, maxDepth, think_limit);
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
#if !USE_MISAMINO
            result++[0] = 'd';
#endif
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
        auto run_result = srs_ai.run(map, node, next, maxDepth, think_limit);
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
#if !USE_MISAMINO
    if (f > think_limit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(f - think_limit));
    }
#endif
    return result_buffer[player];
}