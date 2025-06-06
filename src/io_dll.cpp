#ifdef _WIN32
#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall
#else
#define DECLSPEC_EXPORT
#define WINAPI
#define __cdecl
#endif

#include <ctime>
#include <numeric>
#include "tetris_core.h"
#include "search_amini.h"
#include "ai_zzz.h"
#include "rule_io.h"
#include "random.h"
#include "ai_setting.h"
#include "io_dll_specs.h"

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
m_tetris::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::IO, search_amini::Search> srs_ai;
#else
m_tetris::TetrisEngine<rule_io::TetrisRule, ai_zzz::IO, search_amini::Search> srs_ai;
#endif
struct {
    int level;
    bool canHold;
} c;

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
see tetris_gem.cpp for the bitmapstatus->
curCanHold: indicates whether you can use hold on current move.
might be caused by re-think after a hold move.
canhold: false if hold is completely disabled.
comboTable: -1 is the end of the table.
*/

using namespace AIDLL;

extern "C" void attach_init()
{
    ege::mtsrand((unsigned int)(time(nullptr)));
}

extern "C" DECLSPEC_EXPORT int __cdecl AIDllVersion()
{
    return DLL_VER;
}

extern "C" DECLSPEC_EXPORT const char* __cdecl AIName(int level)
{
    static std::string name;
    name = srs_ai.ai_name() + " LV" + std::to_string(level);
    return name.c_str();
}

extern "C" DECLSPEC_EXPORT void __cdecl InitAI(const Config* config) {
    if (config->season == 2) {
        srs_ai.search_config()->allow_immobile_t = true;
        srs_ai.search_config()->is_amini = true;
        srs_ai.search_config()->is_aspin = false;
        srs_ai.search_config()->is_tspin = true;
        srs_ai.ai_config()->season_2 = true;
    }
    else {
        srs_ai.search_config()->allow_immobile_t = false;
        srs_ai.search_config()->is_amini = false;
        srs_ai.search_config()->is_aspin = false;
        srs_ai.search_config()->is_tspin = true;
        srs_ai.ai_config()->season_2 = false;
    }
    srs_ai.ai_config()->multiplier = config->multiplier;
    srs_ai.ai_config()->garbage_cap = config->garbageCap;
    srs_ai.search_config()->allow_180 = config->allow180;
    srs_ai.ai_config()->lockout = config->lockout;
    c.level = config->level;
    c.canHold = config->canHold;
}

extern "C" DECLSPEC_EXPORT const char* __cdecl TetrisAI(const Field* field, const Queue* queue, const Status* status)
{
    static std::string result;

	//{
	//	FILE* file = fopen("debug.txt", "w");
	//	if (file) {
	//		for (int h = 24; h >= 0; --h) {
	//			for (int w = 0; w < 10; ++w) {
	//				if ((field->field[h] >> w) & 1) {
	//					fprintf(file, "[]");
	//				}
	//				else {
	//					fprintf(file, "  ");
	//				}
	//			}
	//			fprintf(file, "\n");
	//		}
	//		fprintf(file, "Queue:\n");
	//		fprintf(file, " next: ");
	//		for (int i = 0; i < NEXT_SIZE; ++i) fprintf(file, "%d ", (int)queue->next[i]);
	//		fprintf(file, "\n hold: %d\n active: %d\n canHold: %d\n curCanHold: %d\n x: %d\n y: %d\n r: %d\n\n",
	//			(int)queue->hold, (int)queue->active, queue->canHold, queue->curCanHold, queue->x, queue->y, queue->r);

	//		// Print Status
	//		fprintf(file, "Status:\n b2b: %d\n combo: %d\n upcomeAtt: ", status->b2b, status->combo);
	//		for (int i = 0; i < UPCOMEATT_SIZE; ++i) fprintf(file, "%d ", status->upcomeAtt[i]);
	//		fprintf(file, "\n\n");
	//		fclose(file);
	//	}
	//}

    if (!srs_ai.prepare(10, 40))
    {
        result.clear();
        return result.c_str();
    }
    m_tetris::TetrisMap map(10, 40);
    for (size_t d = 0; d < FIELD_SIZE; ++d)
    {
        map.row[d] = field->field[d];
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
    srs_ai.search_config()->allow_D = true;
    srs_ai.search_config()->allow_LR = true;
    srs_ai.search_config()->allow_d = true;
    srs_ai.search_config()->last_rotate = true;
    srs_ai.search_config()->is_20g = false;
    //NAME: init_21
    //GEN: 72
    //SCORE: 1590.09
    //DIFF: 83.88
    srs_ai.ai_config()->param = { 128.848632018967037993206758983, 159.486229165944052965642185882, 161.917442316092603959987172857, 81.770591639349177626172604505, 381.778776257560934936918783933, 98.094088345045122423471184447, 34.677952239613162532805290539, 129.220619858914346878009382635, 0.911925860653483022488785537, 3.743571313305298797757814100, 3.153364454826400375964112754, 0.007065131195186014588516255, -0.081683675915617898199982960, -0.954530616937390941068031225, 1.612455139641955748075474730, 0.570015487183247460123425299, 1.093367709554965427898309827, 1.511144844202827464130223234, 1.007928243238619847588211087, -0.740554584228065859718981301, 0.104364933113540087061821282, 8.660904648990943144326593028, 12.172353417045528090056905057, 30.511480066561279755887881038, 1.585887060974324525020051624 };
    srs_ai.ai_config()->is_margin = false;

    srs_ai.status()->max_combo = 0;
    srs_ai.status()->attack = 0;
    srs_ai.status()->b2bcnt = status->b2b;
    srs_ai.status()->board_fill = map.count;
    srs_ai.memory_limit(1024ull << 20);
    srs_ai.status()->death = 0;
    srs_ai.status()->combo = status->combo;
    int upcomeAtt = std::accumulate(status->upcomeAtt, status->upcomeAtt + UPCOMEATT_SIZE, 0, [](int a, int b) {
        return a + b;
        });
    if (srs_ai.status()->under_attack != upcomeAtt)
    {
        srs_ai.update();
    }
    srs_ai.status()->under_attack = upcomeAtt;
    srs_ai.status()->map_rise = 0;
    srs_ai.status()->like = 0;
    srs_ai.status()->value = 0;
    srs_ai.status()->pc = true;
    m_tetris::TetrisBlockStatus block_status(queue->active, queue->x, 22 - queue->y, (4 - queue->r) % 4);
    time_t think_limit = (time_t)(std::pow(std::pow(100, 1.0 / 8), c.level));
    m_tetris::TetrisNode const* node = srs_ai.get(block_status);
    srs_ai.update();
    int maxDepth = 0;
    for (maxDepth = 0; maxDepth < NEXT_SIZE && queue->next[maxDepth]; ++maxDepth);
    if (c.canHold)
    {
        auto run_result = srs_ai.run_hold(map, node, queue->hold, queue->curCanHold, queue->next, maxDepth, think_limit);
        if (run_result.change_hold)
        {
            if (run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai.make_path(srs_ai.context()->generate(run_result.target->status.t), run_result.target, map);
                result = std::string(ai_path.begin(), ai_path.end());
            }
            result.insert(result.begin(), 'v');
        }
        else
        {
            if (run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai.make_path(node, run_result.target, map);
                result = std::string(ai_path.begin(), ai_path.end());
            }
        }
    }
    else
    {
        auto run_result = srs_ai.run(map, node, queue->next, maxDepth, think_limit);
        if (run_result.target != nullptr)
        {
            std::vector<char> ai_path = srs_ai.make_path(node, run_result.target, map);
            result = std::string(ai_path.begin(), ai_path.end());
        }
    }
    result.push_back('V');
    return result.c_str();
}