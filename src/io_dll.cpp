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

#if USE_THREAD
m_tetris::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::IO_v08, search_tspin::Search> srs_ai;
#else
m_tetris::TetrisEngine<rule_io::TetrisRule, ai_zzz::IO_v08, search_tspin::Search> srs_ai;
#endif
#if USE_PC
std::unique_ptr<m_tetris::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::TOJ_PC, search_tspin::Search>> srs_pc;
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

extern "C" DECLSPEC_EXPORT char *__cdecl AIName(int level)
{
    static char name[200];
    strcpy(name, srs_ai.ai_name().c_str());
    return name;
}
extern "C" DECLSPEC_EXPORT char* __cdecl TetrisAI(int overfield[], int field[], int field_w, int field_h, int b2b, int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, int level, int player)
#else
extern "C" DECLSPEC_EXPORT char* __cdecl TetrisAI(int field[], int field_w, int field_h, int b2b, int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, double pps, bool isEnded, int boardfill, int player)
#endif
{
    static char result_buffer[8][1024];
    char* result = result_buffer[player];
    std::unique_lock<std::mutex> lock(srs_ai_lock);

    if (field_w != 10 || field_h != 22 || !srs_ai.prepare(10, 40))
    {
        *result = '\0';
        return result;
    }
#if USE_PC
    if (!srs_pc || srs_pc->context() != srs_ai.context())
    {
        srs_pc.reset(new m_tetris::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::TOJ_PC, search_tspin::Search>(srs_ai.context()));
        memset(srs_pc->status(), 0, sizeof * srs_pc->status());
    }
#endif
    m_tetris::TetrisMap map(10, 40);
    for (size_t d = 0, s = 22; d < 23; ++d, --s)
    {
        map.row[d] = field[s];
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
    //int i = 0;
    //switch (active)
    //{
    //case 'S':
    //    i = map.full(4, 21) || map.full(5, 21) ? 1 : 0;
    //    break;
    //case 'L':
    //case 'J':
    //case 'T':
    //    i = map.full(4, 21) || map.full(5, 21) || map.full(6, 21) ? 1 : 0;
    //    break;
    //case 'O':
    //case 'Z':
    //    i = map.full(5, 21) || map.full(6, 21) ? 1 : 0;
    //    break;
    //case 'I':
    //    i = map.full(4, 21) || map.full(5, 21) || map.full(6, 21) || map.full(7, 21) ? 1 : 0;
    //    break;
    //}
    srs_ai.search_config()->allow_rotate_move = false;
    srs_ai.search_config()->allow_180 = can180spin;
    srs_ai.search_config()->last_rotate = true;
    srs_ai.search_config()->is_20g = false;
#if USE_PC
    * srs_pc->search_config() = *srs_ai.search_config();
#endif
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
    srs_ai.ai_config()->table = table.table;
    srs_ai.ai_config()->table_max = table.table_max;
#if USE_PC
    srs_pc->ai_config()->table = table.table;
    srs_pc->ai_config()->table_max = table.table_max;
#endif
    srs_ai.memory_limit(1024ull << 20);
    // GEN 19
    srs_ai.ai_config()->param = { 129.271797681, 157.654934407, 159.339194814, 78.880984075, 376.691632445, 100.573124428, 40.050481765, 1.346822692, 129.027085819, 0.510609201, 4.040369705, 2.197667163, 0.253586898, 0.048747843, -5.446558259, 1.103539195, -0.088376950, 1.033262405, 1.414885361, 0.958547792, -0.847350442, 0.317920170, 7.530992201, 12.369924497, 5.870477116, 29.245939927, 1.529551513 };
    srs_ai.status()->max_combo = 0;
    srs_ai.status()->death = 0;
    srs_ai.status()->is_margin = elapsed_time > GARBAGE_MARGIN_TIME;
    srs_ai.status()->combo = combo;
    srs_ai.status()->attack = 0;
    srs_ai.update();
    srs_ai.status()->under_attack = upcomeAtt;
    srs_ai.status()->map_rise = 0;
    srs_ai.status()->b2b = !!b2b;
    srs_ai.status()->b2bcnt = b2b;
#if !USE_MISAMINO
    srs_ai.status()->board_fill = boardfill;
#endif
    srs_ai.status()->like = 0;
    srs_ai.status()->value = 0;
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

    //m_tetris::TetrisBlockStatus status(active, x, 22 - (y - i), (4 - spin) % 4);
    m_tetris::TetrisBlockStatus status(active, x, 22 - y, (4 - spin) % 4);
    m_tetris::TetrisNode const* node = srs_ai.get(status);
#if !USE_MISAMINO
    if (isEnded) pieces = 0, now = clock(), a = 0, init_time = clock(), influency = 0;
    elapsed_time = clock() - init_time;
    ++pieces, avg = (clock() - now) / pieces;
    if (avg > 1000 / pps)influency = 0;
    else influency += 20, influency = std::min((clock_t)300, influency);
    if (a == 0)
    {
        if (elapsed_time > GARBAGE_MARGIN_TIME) 
        {
            srs_ai.status()->is_margin = true;
            srs_ai.status()->start_count = clock();
            a = 1;
        }
    }
    time_t f = (time_t)(((1000 + influency) / pps) - std::min(2 * ((pps * (upcomeAtt * 3 + (combo * 2))) + boardfill), ((1000 + influency) / pps) / 2));
#else
    time_t f = (time_t)(std::pow(std::pow(100, 1.0 / 8), level));
#endif
    if (canhold)
    {
#if USE_PC
        srs_pc->run_hold(map, node, hold, curCanHold, next, maxDepth, time_t(0));
#endif
        auto run_result = srs_ai.run_hold(map, node, hold, curCanHold, next, maxDepth, f);
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
        auto run_result = srs_ai.run(map, node, next, maxDepth, f);
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