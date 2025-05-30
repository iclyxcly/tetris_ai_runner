
#pragma once

#include "search_simple.h"

using namespace m_tetris;

namespace search_simple
{
    void Search::init(m_tetris::TetrisContext const *context)
    {
        node_mark_filtered_.init(context->node_max());
    }

    std::vector<char> Search::make_path(TetrisNode const *node, TetrisNode const *land_point, TetrisMap const &map)
    {
        std::vector<char> path;
        if(node == land_point || node->status.t != land_point->status.t || node->status.y < land_point->status.y)
        {
            return path;
        }
        while (node->status.r != land_point->status.r && node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
        {
            path.push_back('z');
            node = node->rotate_counterclockwise;
        }
        while (node->status.x < land_point->status.x && node->move_right && node->move_right->check(map))
        {
            path.push_back('r');
            node = node->move_right;
        }
        while (node->status.x > land_point->status.x && node->move_left && node->move_left->check(map))
        {
            path.push_back('l');
            node = node->move_left;
        }
        if(node->drop(map) == land_point)
        {
            path.push_back('D');
            return path;
        }
        while (node->status.y > land_point->status.y && node->move_down && node->move_down->check(map))
        {
            path.push_back('d');
            node = node->move_down;
        }
        if(node->drop(map) != land_point)
        {
            return std::vector<char>();
        }
        return path;
    }

    namespace
    {
        void push(m_tetris::TetrisNodeMarkFiltered &mark, std::vector<TetrisNode const *> &land_point_cache, TetrisNode const *land_point)
        {
            if(mark.mark(land_point))
            {
                land_point_cache.push_back(land_point);
            }
        }
    }

    std::vector<TetrisNode const *> const *Search::search(TetrisMap const &map, TetrisNode const *node, size_t depth)
    {
        land_point_cache_.clear();
        if (!node->check(map))
        {
            return &land_point_cache_;
        }
        node_mark_filtered_.clear();
        if(node->land_point != nullptr && node->low >= map.roof)
        {
            for(auto cit = node->land_point->begin(); cit != node->land_point->end(); ++cit)
            {
                push(node_mark_filtered_, land_point_cache_, (*cit)->drop(map));
            }
        }
        else
        {
            TetrisNode const *rotate = node;
            do
            {
                push(node_mark_filtered_, land_point_cache_, rotate->drop(map));
                TetrisNode const *left = rotate->move_left;
                while (left != nullptr && left->check(map))
                {
                    push(node_mark_filtered_, land_point_cache_, left->drop(map));
                    left = left->move_left;
                }
                TetrisNode const *right = rotate->move_right;
                while (right != nullptr && right->check(map))
                {
                    push(node_mark_filtered_, land_point_cache_, right->drop(map));
                    right = right->move_right;
                }
                rotate = rotate->rotate_counterclockwise;
            } while (rotate != nullptr && rotate != node && rotate->check(map));
        }
        return &land_point_cache_;
    }
}
