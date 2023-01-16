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
m_tetris::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::IO_v08, search_tspin::Search> srs_ai;
#else
m_tetris::TetrisEngine<rule_io::TetrisRule, ai_zzz::IO_v08, search_tspin::Search> srs_ai;
#endif
#if USE_PC
std::unique_ptr<m_tetris::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::TOJ_PC, search_tspin::Search>> srs_pc;
#endif
#else
#if USE_THREAD
m_tetris::TetrisThreadEngine<rule_toj::TetrisRule, ai_zzz::IO_v08, search_tspin::Search> srs_ai;
#else
m_tetris::TetrisEngine<rule_toj::TetrisRule, ai_zzz::IO_v08, search_tspin::Search> srs_ai;
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
extern "C" DECLSPEC_EXPORT char* __cdecl TetrisAI(int field[], int b2b, int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, bool can180spin, int upcomeAtt, int maxDepth, double pps, bool burst, bool pc, bool isEnded, int player)
#endif
{
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
    for (size_t d = 0, s = 22; d < 23; ++d, --s)
    {
        map.row[d] = field[s];
    }
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
    srs_ai.ai_config()->table = table.table;
    srs_ai.ai_config()->table_max = table.table_max;
#if USE_PC
    srs_pc->ai_config()->table = table.table;
    srs_pc->ai_config()->table_max = table.table_max;
#endif
#endif
    srs_ai.memory_limit(1024ull << 20);
    // GEN 355
    srs_ai.ai_config()->param = { -0.367524328, 6.049256501, 27.627711300, -1.637806418, 6.765923337, 2.809611337, 7.193374429, 8.086589257, 8.973413111, -3.032756246, -4.544260860, -0.898290778, 1.568195669, -3.277873776, 0.332123532, -0.119231076, 1.119955012, -0.248719773, -0.432295577, -1.591718588, -3.621631207, -3.369561764, 4.291762455, 1.025722959, -2.468272212, 2.682308128, -0.559509211, 0.106651065, -0.895323708, -0.025436488, 0.176000765, -8.769156396, -1.084310283, 0.333485563 };
    srs_ai.status()->max_combo = 0;
    srs_ai.status()->max_attack = 0;
    srs_ai.status()->death = 0;
    srs_ai.status()->is_margin = elapsed_time > GARBAGE_MARGIN_TIME;
    srs_ai.status()->combo = combo;
    srs_ai.status()->attack = 0;
    srs_ai.status()->under_attack = upcomeAtt;
    srs_ai.status()->map_rise = 0;
    srs_ai.status()->b2bcnt = b2b;
    srs_ai.status()->board_fill = map.count;
#if !USE_MISAMINO
    srs_ai.status()->pc = pc;
#else
    srs_ai.status()->pc = true;
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

#if !USE_MISAMINO
    m_tetris::TetrisBlockStatus status(active, x, 22 - (y - i), (4 - spin) % 4);
    if (isEnded) pieces = time_t(0), now = clock(), a = 0, init_time = clock(), influency = time_t(0);
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
            f = time_t((900 + influency) / pps);
        }
        else
        {
            if (avg > time_t(1000) / pps)influency -= time_t(1);
            f = time_t((900 + influency) / pps);
        }
    }
    time_t think_limit = f > time_t(600) ? time_t(600) : f;
    if (j == 2 && think_limit > 400) {
        think_limit = f = time_t(400);
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
    srs_ai.status()->board_fill_prev = map.count;
    return result_buffer[player];
}