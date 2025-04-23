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
#include "search_amini.h"
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
#if USE_THREAD
m_tetris::TetrisThreadEngine<rule_toj::TetrisRule, ai_zzz::IO, search_amini::Search> srs_ai;
#else
m_tetris::TetrisEngine<rule_toj::TetrisRule, ai_zzz::IO, search_tspin::Search> srs_ai;
#endif
#if USE_PC
std::unique_ptr<m_tetris::TetrisThreadEngine<rule_toj::TetrisRule, ai_zzz::TOJ_PC, search_tspin::Search>> srs_pc;
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

extern "C" void attach_init()
{
    ege::mtsrand((unsigned int)(time(nullptr)));
}
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
        srs_pc.reset(new m_tetris::TetrisThreadEngine<rule_toj::TetrisRule, ai_zzz::TOJ_PC, search_tspin::Search>(srs_ai.context()));
        memset(srs_pc->status(), 0, sizeof * srs_pc->status());
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
    srs_ai.search_config()->allow_D = true;
    srs_ai.search_config()->allow_LR = true;
    srs_ai.search_config()->allow_d = true;
    srs_ai.search_config()->last_rotate = true;
    srs_ai.search_config()->is_20g = false;
    srs_ai.search_config()->allow_immobile_t = false;
    srs_ai.search_config()->is_aspin = false;
    srs_ai.search_config()->is_amini = false;
    srs_ai.search_config()->is_tspin = true;
    //NAME: init_21
    //GEN: 72
    //SCORE: 1590.09
    //DIFF: 83.88
    srs_ai.ai_config()->param = { 128.848632018967037993206758983, 159.486229165944052965642185882, 161.917442316092603959987172857, 81.770591639349177626172604505, 381.778776257560934936918783933, 98.094088345045122423471184447, 34.677952239613162532805290539, -0.435441341531714487533832880, 129.220619858914346878009382635, 0.911925860653483022488785537, 3.743571313305298797757814100, 3.153364454826400375964112754, 0.007065131195186014588516255, -0.081683675915617898199982960, -0.954530616937390941068031225, 1.612455139641955748075474730, 0.570015487183247460123425299, 1.093367709554965427898309827, 1.511144844202827464130223234, 1.007928243238619847588211087, -0.740554584228065859718981301, 0.104364933113540087061821282, 8.660904648990943144326593028, 12.172353417045528090056905057, 6.888583310333941334135943180, 30.511480066561279755887881038, 1.585887060974324525020051624 };
    srs_ai.ai_config()->is_margin = false;
    srs_ai.ai_config()->season_2 = false;

    srs_ai.status()->max_combo = 0;
    srs_ai.status()->attack = 0;
    srs_ai.status()->b2bcnt = b2b;
    srs_ai.status()->board_fill = map.count;
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
    srs_ai.status()->death = 0;
    srs_ai.status()->combo = combo;
    if (srs_ai.status()->under_attack != upcomeAtt)
    {
        srs_ai.update();
    }
    srs_ai.status()->under_attack = upcomeAtt;
    srs_ai.status()->map_rise = 0;
    srs_ai.status()->like = 0;
    srs_ai.status()->value = 0;
    srs_ai.status()->pc = true;
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
    m_tetris::TetrisBlockStatus status(active, x, 22 - y, (4 - spin) % 4);
    time_t think_limit = (time_t)(std::pow(std::pow(100, 1.0 / 8), level));
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
    return result_buffer[player];
}