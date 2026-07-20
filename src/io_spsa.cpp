#define _CRT_SECURE_NO_WARNINGS
#include <ctime>
#include <fstream>
#include <thread>
#include <random>
#include <iostream>
#include <chrono>
#include <deque>
#include <atomic>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <functional>

#include "tetris_core.h"
#include "search_amini.h"
#include "ai_zzz.h"
#include "rule_io.h"
#include "ai_setting.h"
#include "io_param.h"

static double const param_step[] = {
    10.0, // roof      
    10.0, // col_trans 
    10.0, // row_trans 
    10.0, // hole_count
    10.0, // hole_line 
    10.0, // well_depth
    10.0, // hole_depth
    50.0, // b2b       
    50.0, // attack    
     5.0, // hold_t    
     5.0, // hold_i    
     5.0, // waste_t   
     5.0, // waste_i   
    10.0, // clear_1   
    10.0, // clear_2   
    10.0, // clear_3   
    10.0, // clear_4   
    10.0, // t2_slot   
    10.0, // t3_slot   
    10.0, // tspin_mini
    10.0, // tspin_1   
    10.0, // tspin_2   
    10.0, // tspin_3   
    10.0, // combo     
     0.1, // ratio     
    10.0, // spin_combo
    10.0, // surge_utilization
};

size_t const NUM_PARAMS = sizeof(param_step) / sizeof(param_step[0]);

static double const param_rates[NUM_PARAMS] = {
    0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 
    0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,
    0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,
    0.1, 0.1, 0.1, 0.1, 0.1, 0.1,     
};

// ---------------------------------------------------------------------------
// SPSA gain schedule
// ---------------------------------------------------------------------------
struct SpsaSchedule {
    double A;        // a_k = A / (k+1+A)^alpha
    double alpha;    // typically 0.602
    double C;        // c_k = C / (k+1)^gamma
    double gamma;    // typically 0.101

    double a_k(int k) const { return A / std::pow(k + 1.0 + A, alpha); }
    double c_k(int k) const { return C / std::pow(k + 1.0, gamma); }
};

// Larger A = slower decay of learning rate, so params actually move
static SpsaSchedule const default_schedule = { 500.0, 0.602, 1.0, 0.101 };

// ---------------------------------------------------------------------------
// Engine type alias
// ---------------------------------------------------------------------------
using IOEngine = m_tetris::TetrisEngine<rule_io::TetrisRule, ai_zzz::IO, search_amini::Search>;

// ---------------------------------------------------------------------------
// Param <-> double array conversion
// ---------------------------------------------------------------------------
static void param_to_array(ai_zzz::IO::Param const &p, double *out) {
    for (int i = 0; i < NUM_PARAMS; ++i) out[i] = p.data[i];
}
static void array_to_param(double const *in, ai_zzz::IO::Param &p) {
    for (int i = 0; i < NUM_PARAMS; ++i) p.data[i] = in[i];
}

// ---------------------------------------------------------------------------
// Load IO::Param values — try the tagged binary file first, fall back to
// the hardcoded defaults in ai_zzz.h.
// ---------------------------------------------------------------------------
static void load_params(double *out, const std::string &tag = "") {
    if (IOParam::read(out, NUM_PARAMS, tag)) {
        printf("[SPSA] Loaded best parameters from %s\n", IOParam::filename(tag).c_str());
        return;
    }
    // Hardcoded defaults from ai_zzz.h
    double const dflt[NUM_PARAMS] = {
        128.0,   // roof
        160.0,   // col_trans
        160.0,   // row_trans
         80.0,   // hole_count
        380.0,   // hole_line
        100.0,   // well_depth
         40.0,   // hole_depth
       2048.0,   // b2b         
        256.0,   // attack      
        128.0,   // hold_t      
         64.0,   // hold_i      
          0.0,   // waste_t
          0.0,   // waste_i
          0.0,   // clear_1
          0.0,   // clear_2
          0.0,   // clear_3
         32.0,   // clear_4     
        384.0,   // t2_slot     
         64.0,   // t3_slot     
          0.0,   // tspin_mini
          0.0,   // tspin_1
        256.0,   // tspin_2     
        384.0,   // tspin_3     
         30.0,   // combo
          1.5,   // ratio
         64.0,   // spin_combo  
        128.0,   // surge_utilization
    };
    memcpy(out, dflt, sizeof(dflt[0]) * NUM_PARAMS);
}

struct BotInstance {
    IOEngine ai;
    m_tetris::TetrisMap map;
    std::mt19937 r_next;
    std::mt19937 r_garbage;

    int next_length = 5;
    int search_ms = 20;
    bool season_2 = false;
    std::vector<char> next;
    ai_zzz::IO::GarbageQueue recv_attack;
    ai_zzz::IO::GarbageQueue network_recv_attack;
    int send_attack = 0;
    int combo = 0;
    int b2bcnt = 0;
    char hold = ' ';
    bool dead = false;

    int total_block = 0;
    int total_clear = 0;
    int total_attack = 0;
    int total_receive = 0;

    explicit BotInstance(IOEngine &global_ai) : ai(global_ai.context()) {
        ai.prepare(10, 40);
    }

    void init(double const *params) {
        map = m_tetris::TetrisMap(10, 40);
        array_to_param(params, ai.ai_config()->param);
        r_next.seed(std::random_device{}());
        r_garbage.seed(r_next());
        next.clear();
        recv_attack.clear();
        network_recv_attack.clear();
        send_attack = 0;
        combo = 0;
        b2bcnt = 0;
        hold = ' ';
        dead = false;
        total_block = 0;
        total_clear = 0;
        total_attack = 0;
        total_receive = 0;
    }

    void init_status() {
        ai.search_config()->allow_rotate_move = false;
        ai.search_config()->allow_180 = true;
        ai.search_config()->allow_d = true;
        ai.search_config()->allow_D = true;
        ai.search_config()->allow_LR = false;
        ai.search_config()->is_20g = false;
        ai.search_config()->last_rotate = false;

        ai.search_config()->is_amini = season_2;
        ai.search_config()->is_aspin = false;
        ai.search_config()->is_tspin = true;
        ai.search_config()->allow_immobile_t = season_2;

        ai.ai_config()->season_2 = season_2;
        ai.ai_config()->pc = true;
        ai.ai_config()->lockout = false;
        ai.ai_config()->multiplier = 1;
        ai.ai_config()->garbage_cap = GARBAGE_CAP;
        ai.ai_config()->pc = false;

        ai.status()->max_combo = 0;
        ai.status()->death = 0;
        ai.status()->combo = combo;
        ai.status()->attack = 0;
        ai.status()->acc_attack = 0;
        ai.status()->acc_surge_attack = 0;
        ai.status()->b2b_move_cnt = 0;
        ai.status()->under_attack = recv_attack;
        ai.status()->map_rise = 0;
        ai.status()->b2bcnt = b2bcnt;
        ai.status()->like = 0;
        ai.status()->value = 0;
    }

    void prepare() {
        if (!next.empty()) next.erase(next.begin());
        while (next.size() <= (size_t)next_length) {
            for (size_t i = 0; i < ai.context()->type_max(); ++i)
                next.push_back(ai.context()->convert(i));
            std::shuffle(next.end() - ai.context()->type_max(), next.end(), r_next);
        }
    }

    void run() {
        init_status();

        char current = next.front();
        auto result = ai.run_hold(map, ai.context()->generate(current), hold, true,
                                  next.data() + 1, next_length, search_ms);
        if (result.target == nullptr) {
            dead = true;
            return;
        }
        if (result.change_hold) {
            if (hold == ' ') next.erase(next.begin());
            hold = current;
        }

        using ASpinType = ai_zzz::IO::ASpinType;
        ASpinType spin = result.target.type;

        int clear = result.target->attach(ai.context().get(), map);
        total_clear += clear;

        int cur_atk = 0;
        int base = 0;
        int surge_atk = 0;

        auto is_b2b_move = [&]() {
            return clear == 4 || (clear && spin != ASpinType::None) || (season_2 && map.count == 0);
        };

        switch (clear) {
        case 0:
            combo = 0;
            break;
        case 1:
            if (spin == ASpinType::None || spin == ASpinType::ASpinMini || spin == ASpinType::TSpinMini)
            {
                base = 0;
            }
            else
                base = 2;
            goto attack;
        case 2:
            if (spin == ASpinType::None || spin == ASpinType::ASpinMini || spin == ASpinType::TSpinMini)
            {
                base = 1;
            }
            else
                base = 4;
            goto attack;
        case 3:
            if (spin == ASpinType::None || spin == ASpinType::ASpinMini || spin == ASpinType::TSpinMini)
            {
                base = 2;
            }
            else
                base = 6;
            goto attack;
        case 4:
            base = 4;
            goto attack;

        attack:
            ++combo;
            if (is_b2b_move()) {
                ++b2bcnt;
            } else {
                if (season_2 && b2bcnt > 4) surge_atk += b2bcnt - 1;
                b2bcnt = 0;
            }
            cur_atk = base;
            if (season_2) {
                if (clear && b2bcnt > 1) ++cur_atk;
            } else {
                if (b2bcnt > 1) {
                    int b2b_copy = b2bcnt - 1;
                    double f = log1p(b2b_copy * 0.8);
                    while (f > 1) --f;
                    cur_atk += (int)std::floor(1 + log1p(b2b_copy * 0.8)) + (b2b_copy == 1 ? 0 : (1 + f) / 3);
                }
            }
            cur_atk = (int)std::floor(cur_atk * (1.0 + 0.25 * (combo - 1)));
            if (combo > 2)
                cur_atk = std::max((int)std::floor(std::log1p(1.25 * (combo - 1))), cur_atk);
            break;
        }

        if (map.count == 0) cur_atk += season_2 ? 5 : 10;

        ++total_block;
        total_attack += cur_atk;
        send_attack = cur_atk + surge_atk;
        send_attack = recv_attack.reduce(send_attack);
        send_attack = network_recv_attack.reduce(send_attack);

        int cap = GARBAGE_CAP;
        while (!recv_attack.empty() && recv_attack.queue[0].steps == 0 && cap > 0) {
            if (combo > 0) break;

            int line = 0;
            if (cap < recv_attack.queue[0].lines) {
                line = cap;
                recv_attack.queue[0].lines -= cap;
                cap = 0;
            } else {
                cap -= recv_attack.queue[0].lines;
                line = recv_attack.queue[0].lines;
                recv_attack.pop_front();
            }
            total_receive += line;

            for (int y = map.height - 1; y >= line; --y)
                map.row[y] = map.row[y - line];
            uint32_t hole = 1u << std::uniform_int_distribution<int>(0, map.width - 1)(r_garbage);
            uint32_t garbage_row = ai.context()->full() & ~hole;
            for (int y = 0; y < line; ++y)
                map.row[y] = garbage_row;
            map.count = 0;
            map.roof = 0;
            for (int my = 0; my < map.height; ++my) {
                for (int mx = 0; mx < map.width; ++mx) {
                    if (map.full(mx, my)) {
                        map.top[mx] = map.roof = my + 1;
                        ++map.count;
                    }
                }
            }
        }

        while(!network_recv_attack.empty() && network_recv_attack.queue[0].steps == 0)
        {
            recv_attack.push({network_recv_attack.queue[0].lines, 1});
            network_recv_attack.pop_front();
        }

        recv_attack.tick();
        network_recv_attack.tick();
    }

    void under_attack(int line) {
        if (line > 0) network_recv_attack.push({static_cast<uint8_t>(line), 0});
    }
};

static void render_view(BotInstance &b1, BotInstance &b2) {
    m_tetris::TetrisMap m1 = b1.map;
    m_tetris::TetrisMap m2 = b2.map;
    if (!b1.next.empty()) {
        auto n1 = b1.ai.context()->generate(b1.next.front());
        if (n1) { m_tetris::TetrisMap tmp = b1.map; n1->attach(b1.ai.context().get(), tmp); m1 = tmp; }
    }
    if (!b2.next.empty()) {
        auto n2 = b2.ai.context()->generate(b2.next.front());
        if (n2) { m_tetris::TetrisMap tmp = b2.map; n2->attach(b2.ai.context().get(), tmp); m2 = tmp; }
    }

    int up1 = b1.recv_attack.sum() + b1.network_recv_attack.sum();
    int up2 = b2.recv_attack.sum() + b2.network_recv_attack.sum();

    printf("\033[H"); // move cursor home
    printf("HOLD=%c NXT=%c%c%c%c%c CMB=%d B2B=%d UP=%2d ATK=%4d BLK=%4d\n",
        b1.hold ? b1.hold : ' ', b1.next.size()>1?b1.next[1]:' ', b1.next.size()>2?b1.next[2]:' ',
        b1.next.size()>3?b1.next[3]:' ', b1.next.size()>4?b1.next[4]:' ', b1.next.size()>5?b1.next[5]:' ',
        b1.combo, b1.b2bcnt, up1, b1.total_attack, b1.total_block);
    printf("HOLD=%c NXT=%c%c%c%c%c CMB=%d B2B=%d UP=%2d ATK=%4d BLK=%4d\n",
        b2.hold ? b2.hold : ' ', b2.next.size()>1?b2.next[1]:' ', b2.next.size()>2?b2.next[2]:' ',
        b2.next.size()>3?b2.next[3]:' ', b2.next.size()>4?b2.next[4]:' ', b2.next.size()>5?b2.next[5]:' ',
        b2.combo, b2.b2bcnt, up2, b2.total_attack, b2.total_block);
    for (int y = 21; y >= 0; --y) {
        for (int x = 0; x < 10; ++x) printf("%s", m1.full(x, y) ? "[]" : "  ");
        printf("  ");
        for (int x = 0; x < 10; ++x) printf("%s", m2.full(x, y) ? "[]" : "  ");
        printf("\n");
    }
    fflush(stdout);
}

static std::pair<double,double> play_match(BotInstance &b1, BotInstance &b2,
                                            int max_rounds = 1000,
                                            std::function<void()> view_cb = nullptr) {
    for (int round = 1; round <= max_rounds; ++round) {
        b1.prepare(); b2.prepare();
        if (view_cb) view_cb();
        b1.run();     b2.run();
        if (b1.dead || b2.dead) break;

        int atk1 = b1.send_attack;
        int atk2 = b2.send_attack;
        if (atk1 > atk2)      b2.under_attack(atk1 - atk2);
        else if (atk2 > atk1) b1.under_attack(atk2 - atk1);
    }

    double app1 = b1.total_block > 0 ? (double)b1.total_attack / b1.total_block : 0.0;
    double app2 = b2.total_block > 0 ? (double)b2.total_attack / b2.total_block : 0.0;
    return {app1, app2};
}

static double evaluate(IOEngine &global_ai, double const *theta,
                       int num_matches, std::mt19937 &rng, bool season_2,
                       std::function<void()> view_cb = nullptr) {
    double perturb[NUM_PARAMS];
    memcpy(perturb, theta, sizeof(perturb[0]) * NUM_PARAMS);
    for (int i = 0; i < NUM_PARAMS; ++i) {
        perturb[i] += (rng() & 1 ? 1 : -1) * param_step[i] * 0.5;
    }
    int win_threshold = (num_matches + 1) / 2;
    int wins_theta = 0, wins_perturb = 0;
    int matches_played = 0;
    for (int m = 0; m < num_matches; ++m) {
        BotInstance b1(global_ai), b2(global_ai);
        b1.season_2 = b2.season_2 = season_2;
        b1.init(theta);
        b2.init(perturb);
        auto [app1, app2] = play_match(b1, b2, 1000, view_cb);
        if (app1 > app2) ++wins_theta;
        else if (app2 > app1) ++wins_perturb;
        ++matches_played;
        if (wins_theta >= win_threshold || wins_perturb >= win_threshold)
            break;
    }
    return matches_played > 0 ? (double)(wins_theta - wins_perturb) / matches_played : 0.0;
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    int season       = 1;
    int num_iters    = 10000;
    int eval_matches = 1;

    if (argc > 1) season       = std::stoi(argv[1]);
    if (argc > 2) num_iters    = std::stoi(argv[2]);
    if (argc > 3) eval_matches = std::stoi(argv[3]);

    std::string tag = "s" + std::to_string(season);
    std::string data_file = IOParam::tag_filename("spsa_data.bin", tag);

    bool season_2 = (season == 2);

    std::mt19937 rng((unsigned)std::random_device{}());

    IOEngine global_ai;
    global_ai.prepare(10, 40);

    double theta[NUM_PARAMS];

    load_params(theta, tag);

    int resume_k = 0;
    {
        std::ifstream ifs(data_file, std::ios::binary);
        if (ifs.good()) {
            ifs.read(reinterpret_cast<char*>(&resume_k), sizeof(resume_k));
            ifs.read(reinterpret_cast<char*>(theta), sizeof(theta));
            ifs.close();
            printf("[SPSA] Resumed from iteration %d (overrides best_io_param)\n", resume_k);
        } else {
            printf("[SPSA] Starting fresh\n");
        }
    }

    SpsaSchedule sched = default_schedule;
    printf("[SPSA] %d iters, %d matches/eval, 1000 rounds/match, 20ms search, season %d\n",
           num_iters, eval_matches, season);
    fflush(stdout);

    std::atomic<bool> view{false};

    // Stdin listener
    std::thread stdin_thread([&]() {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "view") { view = true; printf("\033[2J"); }
            else if (line.empty()) { view = false; }
        }
    });
    stdin_thread.detach();

    auto start_time = std::chrono::steady_clock::now();

    for (int k = resume_k; k < num_iters; ++k) {
        double ck = sched.c_k(k);
        double ak = sched.a_k(k);

        double delta[NUM_PARAMS];
        for (int i = 0; i < NUM_PARAMS; ++i)
            delta[i] = (rng() & 1) ? 1.0 : -1.0;

        double theta_plus[NUM_PARAMS], theta_minus[NUM_PARAMS];
        for (int i = 0; i < NUM_PARAMS; ++i) {
            double step = ck * param_step[i] * delta[i];
            theta_plus[i]  = theta[i] + step;
            theta_minus[i] = theta[i] - step;
        }

        int m = std::max(1, eval_matches);
        int win_threshold = (m + 1) / 2;
        int wins_plus = 0, wins_minus = 0;
        int matches_played = 0;
        for (int i = 0; i < m; ++i) {
            BotInstance plus(global_ai), minus(global_ai);
            plus.season_2 = minus.season_2 = season_2;
            plus.init(theta_plus);
            minus.init(theta_minus);
            auto view_cb = [&]() { if (view.load()) render_view(plus, minus); };
            auto [app_plus, app_minus] = play_match(plus, minus, 1000, view_cb);
            if (app_plus > app_minus) ++wins_plus;
            else if (app_minus > app_plus) ++wins_minus;
            ++matches_played;
            if (wins_plus >= win_threshold || wins_minus >= win_threshold)
                break;
        }
        double score_diff = matches_played > 0 ? (double)(wins_plus - wins_minus) / matches_played : 0.0;

        double grad[NUM_PARAMS];
        for (int i = 0; i < NUM_PARAMS; ++i) {
            double denom = 2.0 * ck * param_step[i] * delta[i];
            if (std::fabs(denom) < 1e-15) denom = 1e-15;
            grad[i] = score_diff / denom;
        }

        for (int i = 0; i < NUM_PARAMS; ++i)
            theta[i] += ak * param_rates[i] * param_step[i] * param_step[i] * grad[i];

        {
            std::ofstream ofs(data_file, std::ios::binary);
            int sk = k + 1;
            ofs.write((char const*)&sk, sizeof(sk));
            ofs.write((char const*)theta, sizeof(theta));
            ofs.close();
        }

        // Save best params every iteration
        IOParam::write(theta, NUM_PARAMS, tag);
        if ((k + 1) % 10 == 0 || k == resume_k) {
            auto now = std::chrono::steady_clock::now();
            double sec = std::chrono::duration<double>(now - start_time).count();
            printf("[SPSA] iter %5d | ∇=%+.6f | ak=%.6f ck=%.6f | %.1fs\n",
                    k, score_diff, ak, ck, sec);
        }
        fflush(stdout);
    }

    // Final save
    IOParam::write(theta, NUM_PARAMS, tag);
    {
        std::ofstream ofs(data_file, std::ios::binary);
        int sk = num_iters;
        ofs.write((char const*)&sk, sizeof(sk));
        ofs.write((char const*)theta, sizeof(theta));
        ofs.close();
    }

    printf("\n[SPSA] Done. %d iterations completed\n", num_iters);
    printf("[SPSA] Params saved to %s\n", IOParam::filename(tag).c_str());
    return 0;
}
