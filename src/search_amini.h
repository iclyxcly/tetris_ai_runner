
#pragma once

#include "tetris_core.h"
#include <vector>
#include <cstddef>

namespace search_amini
{
    class Search
    {
    public:
        enum ASpinType
        {
            None,
            TSpin,
            AllSpin,
            TSpinMini,
            ASpinMini,
        };
        struct Config
        {
            bool allow_rotate_move = false;
            bool allow_180 = true;
            bool allow_d = false;
            bool allow_D = true;
            bool allow_LR = true;
            bool is_20g = false;
            bool last_rotate = false;
            bool is_amini = true;
            bool is_aspin = false;
            bool is_tspin = true;
            bool allow_immobile_t = true;
        };
        struct TetrisNodeWithASpinType
        {
            TetrisNodeWithASpinType()
            {
                std::memset(this, 0, sizeof(*this));
            }
            TetrisNodeWithASpinType(m_tetris::TetrisNode const *_node) : node(_node), last(), type(None), flags()
            {
            }
            m_tetris::TetrisNode const *node;
            m_tetris::TetrisNode const *last;
            ASpinType type;
            union
            {
                struct
                {
                    bool is_check;
                    bool is_last_rotate;
                    bool is_ready;
                    bool is_mini_ready;
                };
                uint32_t flags;
            };
            operator m_tetris::TetrisNode const *() const
            {
                return node;
            }
            m_tetris::TetrisNode const *operator->() const
            {
                return node;
            }
            bool operator==(TetrisNodeWithASpinType const &other)
            {
                return node == other.node && last == other.last && type == other.type && flags == other.flags;
            }
            bool operator==(std::nullptr_t)
            {
                return node == nullptr;
            }
            bool operator!=(std::nullptr_t)
            {
                return node != nullptr;
            }
        };
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::vector<char> make_path(m_tetris::TetrisNode const *node, TetrisNodeWithASpinType const &land_point, m_tetris::TetrisMap const &map);
        std::vector<TetrisNodeWithASpinType> const *search(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node, size_t depth);

    private:
        std::vector<char> make_path_20g(m_tetris::TetrisNode const *node, TetrisNodeWithASpinType const &land_point, m_tetris::TetrisMap const &map);
        std::vector<TetrisNodeWithASpinType> const *search_t(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node, size_t depth);
        bool check_ready(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node);
        bool check_mini_ready(TetrisNodeWithASpinType const& node, m_tetris::TetrisMap const& map);
        bool check_immobile(TetrisNodeWithASpinType const &node, m_tetris::TetrisMapSnap const &snap);
        std::vector<TetrisNodeWithASpinType> land_point_cache_;
        std::vector<m_tetris::TetrisNode const *> node_incomplete_;
        std::vector<m_tetris::TetrisNode const *> node_search_;
        m_tetris::TetrisNodeMark node_mark_;
        m_tetris::TetrisNodeMarkFiltered node_mark_filtered_;
        uint32_t *block_data_;
        uint32_t block_data_buffer_[52];
        int x_diff_, y_diff_;
        Config const *config_;
        m_tetris::TetrisContext const *context_;
    };
}
