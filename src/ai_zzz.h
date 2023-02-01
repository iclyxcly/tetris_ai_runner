﻿
#include "tetris_core.h"
#include "search_tspin.h"
#include <array>

namespace ai_zzz
{
    namespace qq
    {
        class Attack
        {
        public:
            struct Config
            {
                size_t level;
                int mode;
            };
            struct Result
            {
                double land_point, map;
                size_t clear;
                int danger;
            };
            struct Status
            {
                double land_point;
                double attack;
                double rubbish;
                double value;
                bool operator < (Status const &) const;
            };
        public:
            void init(m_tetris::TetrisContext const *context, Config const *config);
            std::string ai_name() const;
            Result eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
            Status get(m_tetris::TetrisNode const *node, Result const &eval_result, size_t depth, Status const &status) const;

        private:
            uint32_t check_line_1_[32];
            uint32_t check_line_2_[32];
            uint32_t *check_line_1_end_;
            uint32_t *check_line_2_end_;
            Config const *config_;
            m_tetris::TetrisContext const *context_;
            int col_mask_, row_mask_;
            struct MapInDangerData
            {
                uint32_t data[4];
            };
            std::vector<MapInDangerData> map_danger_data_;
            size_t map_in_danger_(m_tetris::TetrisMap const &map) const;
        };
    }

    class Dig
    {
    public:
        struct Config
        {
            std::array<double, 100> p =
            {
                0 ,     1,
                0 ,     1,
                0 ,     1,
                0 ,    96,
                0 ,   160,
                0 ,   128,
                0 ,    60,
                0 ,   380,
                0 ,   100,
                0 ,    40,
                0 , 50000,
                32,  0.25,
            };
        };
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::string ai_name() const;
        double eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        double get(m_tetris::TetrisNode const *node, double const &eval_result) const;
    private:
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        m_tetris::TetrisContext const *context_;
        Config const *config_;
        size_t map_in_danger_(m_tetris::TetrisMap const &map) const;
        int col_mask_, row_mask_;
    };

    class TOJ_PC
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        typedef search_tspin::Search::TetrisNodeWithTSpinType TetrisNodeEx;
        struct Config
        {
            int const *table;
            int table_max;
        };
        struct Result
        {
            double value;
            int clear;
            int roof;
        };
        struct Status
        {
            int under_attack;
            int recv_attack;
            int attack;
            int like;
            int combo;
            bool b2b;
            bool pc;
            double value;
            bool operator < (Status const &) const;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return 0.5;
        }
        Result eval(TetrisNodeEx const &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(TetrisNodeEx &node, Result const &eval_result, size_t depth, Status const & status) const;

    private:
        m_tetris::TetrisContext const *context_;
        Config const *config_;
        int col_mask_, row_mask_;
    };

    class TOJ_v08
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        typedef search_tspin::Search::TetrisNodeWithTSpinType TetrisNodeEx;
        struct Param {
            double roof = 160;
            double col_trans = 160;
            double row_trans = 160;
            double hole_count = 160;
            double hole_line = 160;
            double well_depth = 160;
            double hole_depth = 160;
            double b2b = 160;
            double attack = 256;
            double max_attack = 40;
            double hold_t = 4;
            double hold_i = 2;
            double waste_t = -0;
            double waste_i = -0;
            double clear_1 = -0;
            double clear_2 = -0;
            double clear_3 = -0;
            double clear_4 = 0;
            double t2_slot = 1.5;
            double t3_slot = 1;
            double tspin_mini = -0;
            double tspin_1 = 0;
            double tspin_2 = 8;
            double tspin_3 = 12;
            double combo = 40;
            double ratio = 1.5;
        };
        struct Config
        {
            Param param;
            int const *table;
            int table_max;
        };
        struct Result
        {
            double value;
            int clear;
            int count;
            int t2_value;
            int t3_value;
            m_tetris::TetrisMap const* map;
        };
        struct Status
        {
            int max_combo;
            int max_attack;
            int death;
            int combo;
            int attack;
            int combo_attack;
            int under_attack;
            int map_rise;
            bool b2b;
            double like;
            double value;
            bool operator < (Status const &) const;
        };
    public:
        int8_t get_safe(m_tetris::TetrisMap const &m, char t) const;
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return config_->param.ratio;
        }
        Result eval(TetrisNodeEx const &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(TetrisNodeEx &node, Result const &eval_result, size_t depth, Status const & status, m_tetris::TetrisContext::Env const &env) const;
    private:
        m_tetris::TetrisContext const *context_;
        Config const *config_;
        int col_mask_, row_mask_;
        int full_count_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        size_t map_in_danger_(m_tetris::TetrisMap const &map, size_t t, size_t up) const;
    };

    class IO_v08
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        typedef search_tspin::Search::TetrisNodeWithTSpinType TetrisNodeEx;
        struct Param
        {
            double roof = 128;
            double col_trans = 256;
            double row_trans = 180;
            double clear_width = 1;
            double b2b = 32;
            double attack = 1.5;
            double hold_t = 4;
            double hold_i = 2;
            double waste_t = -22;
            double waste_i = -7;
            double clear_1 = 15;
            double clear_2 = -32;
            double clear_3 = -32;
            double clear_4 = 15;
            double t2_slot = 1.5;
            double t3_slot = 0.5;
            double tspin_mini = -5;
            double tspin_1 = 6;
            double tspin_2 = 8;
            double tspin_3 = 12;
            double combo = 0;
            double ratio = 1.5;
        };
        struct Config
        {
            int const* table;
            int table_max;
            Param param;
        };
        struct Result
        {
            double value;
            int clear;
            int count;
            int t2_value;
            int t3_value;
            m_tetris::TetrisMap const* map;
        };
        struct Status
        {
            int max_combo;
            int max_attack;
            int death;
            int combo;
            int attack;
            int under_attack;
            int under_attack_PRE; //soon
            int map_rise;
            int b2bcnt;
            int combo_attack;
            int b2b_attack;
            bool pc;
            int board_fill;
            bool is_margin;
            double like;
            double value;
            clock_t start_count;
            bool operator < (Status const&) const;
        };
    public:
        int8_t get_safe(m_tetris::TetrisMap const& m, char t) const;
        void init(m_tetris::TetrisContext const* context, Config const* config);
        std::string ai_name() const;
        double ratio() const
        {
            return config_->param.ratio;
        }
        Result eval(TetrisNodeEx const& node, m_tetris::TetrisMap const& map, m_tetris::TetrisMap const& src_map, size_t clear) const;
        Status get(TetrisNodeEx& node, Result const& eval_result, size_t depth, Status const& status, m_tetris::TetrisContext::Env const& env) const;
    private:
        m_tetris::TetrisContext const* context_;
        Config const* config_;
        int col_mask_, row_mask_;
        int full_count_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        size_t map_in_danger_(m_tetris::TetrisMap const& map, size_t t, size_t up) const;
    };

    class TOJ
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        typedef search_tspin::Search::TetrisNodeWithTSpinType TetrisNodeEx;
        struct Param {
            double base = 30;
            double roof = 300;
            double col_trans = 380;
            double row_trans = 200;
            double hole_count = 380;
            double hole_line = 60;
            double clear_width = -1;
            double wide_2 = -256;
            double wide_3 = -128;
            double wide_4 = -64;
            double safe = 4;
            double b2b = 512;
            double attack = 160;
            double hold_t = 0.15;
            double hold_i = -0.25;
            double waste_t = -28;
            double waste_i = -14;
            double clear_1 = -26;
            double clear_2 = -64;
            double clear_3 = -56;
            double clear_4 = 14;
            double t2_slot = 1.1;
            double t3_slot = 0.3;
            double tspin_mini = -4;
            double tspin_1 = 7;
            double tspin_2 = 3;
            double tspin_3 = 1;
            double combo = 92;
            double ratio = 0.3;
        };
        struct Config
        {
            int const *table;
            int table_max;
            int safe;
            Param param;
        };
        struct Result
        {
            double value;
            int8_t clear;
            int8_t top_out;
            int16_t count;
            int16_t t2_value;
            int16_t t3_value;
            TSpinType t_spin;
            m_tetris::TetrisMap const* map;
        };
        struct Status
        {
            int8_t death;
            int8_t combo;
            int8_t under_attack;
            int8_t map_rise;
            int8_t b2b;
            int16_t t2_value;
            int16_t t3_value;
            double acc_value;
            double like;
            double value;
            bool operator < (Status const &) const;

            static void init_t_value(m_tetris::TetrisMap const &m, int16_t &t2_value_ref, int16_t &t3_value_ref, m_tetris::TetrisMap *out_map = nullptr);
        };
    public:
        int8_t get_safe(m_tetris::TetrisMap const &m, char t) const;
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return config_->param.ratio;
        }
        Result eval(TetrisNodeEx const &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(TetrisNodeEx &node, Result const &eval_result, size_t depth, Status const & status, m_tetris::TetrisContext::Env const &env) const;
    private:
        m_tetris::TetrisContext const *context_;
        Config const *config_;
        int col_mask_, row_mask_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        size_t map_in_danger_(m_tetris::TetrisMap const &map, size_t t, size_t up) const;
    };

    class C2
    {
    public:
        struct Config
        {
            std::array<double, 100> p;
            double p_rate;
            int safe;
            int mode;
            int danger;
            int soft_drop;
        };
        struct Status
        {
            double attack;
            double map;
            size_t combo;
            size_t combo_limit;
            double value;
            bool operator < (Status const &) const;
        };
        struct Result
        {
            double attack;
            double map;
            size_t clear;
            double fill;
            double hole;
            double new_hole;
            bool soft_drop;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::string ai_name() const;
        Result eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(m_tetris::TetrisNode const *node, Result const &eval_result, size_t depth, Status const &status, m_tetris::TetrisContext::Env const &env) const;
        Status iterate(Status const **status, size_t status_length) const;

    private:
        m_tetris::TetrisContext const *context_;
        Config const *config_;
        int col_mask_, row_mask_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        size_t map_in_danger_(m_tetris::TetrisMap const &map) const;
    };

}