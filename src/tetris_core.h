﻿//F:\git\tetris_ai_runner\the_ai_games\src>g++ -std=c++1y -static -Isrc -o bin/run_ai -O ai_ax.cpp 2> error.txt
#pragma once

#include <unordered_map>
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <functional>
#include <cassert>
#include <ctime>
#include <cstring>
#include "rb_tree.h"

namespace m_tetris
{
    const int max_height = 40;
    const int max_wall_kick = 16;

    struct TetrisNode;
    struct TetrisWallKickOpertion;
    struct TetrisOpertion;
    struct TetrisMap;
    union TetrisBlockStatus;
    class TetrisContext;

    //游戏场景,下标从0开始,左下角为原点,最大支持[高度=40,宽度=32]
    struct TetrisMap
    {
        //行数据,具体用法看full函数吧...
        uint32_t row[max_height];
        //每一列的高度
		int32_t top[32];
        //场景宽
        int32_t width;
        //场景高
        int32_t height;
        //场景目前最大高度
        int32_t roof;
        //场景的方块数
        int32_t count;
        //判定[x,y]坐标是否有方块
        inline bool full(size_t x, size_t y) const
        {
            return (row[y] >> x) & 1;
        }
        TetrisMap()
        {
        }
        TetrisMap(size_t w, size_t h)
        {
            memset(this, 0, sizeof *this);
            width = w;
            height = h;
        }
        TetrisMap(TetrisMap const &other)
        {
            memcpy(this, &other, sizeof *this);
        }
        TetrisMap &operator = (TetrisMap const &other)
        {
            if(this != &other)
            {
                memcpy(this, &other, sizeof *this);
            }
            return *this;
        }
        bool operator == (TetrisMap const &other)
        {
            return std::memcmp(this, &other, sizeof *this) == 0;
        }
    };

    //方块状态
    //t:OISZLJT字符
    //[x,y]坐标,y越大高度越大
    //r:旋转状态(0-3)
    union TetrisBlockStatus
    {
        struct
        {
            char t;
            int8_t x, y;
            uint8_t r;
        };
        uint32_t status;
        TetrisBlockStatus() = default;
        TetrisBlockStatus(TetrisBlockStatus const &) = default;
        TetrisBlockStatus(char _t, int8_t _x, int8_t _y, uint8_t _r) : t(_t), x(_x), y(_y), r(_r)
        {
        }
    };

    struct TetrisBlockStatusHash
    {
		size_t operator()(TetrisBlockStatus const &block) const
        {
            return std::hash<uint32_t>()(block.status);
        };
    };

    struct TetrisBlockStatusEqual
    {
        bool operator()(TetrisBlockStatus const &left, TetrisBlockStatus const &right) const
        {
            return left.status == right.status;
        };
    };

    struct TetrisBlockStatusCompare
    {
        bool operator()(TetrisBlockStatus const &left, TetrisBlockStatus const &right) const
        {
            return left.status < right.status;
        };
    };

    //踢墙表
    struct TetrisWallKickOpertion
    {
        struct WallKickNode
        {
            int16_t x, y;
        };
        uint32_t length;
        WallKickNode data[max_wall_kick];
    };

    //方块操作
    struct TetrisOpertion
    {
        //创建一个方块
        TetrisNode(*create)(size_t w, size_t h, TetrisOpertion const &op);
        //顺时针旋转(右旋)
        bool(*rotate_clockwise)(TetrisNode &node, TetrisContext const *context);
        //逆时针旋转(左旋)
        bool(*rotate_counterclockwise)(TetrisNode &node, TetrisContext const *context);
        //转动180°
        bool(*rotate_opposite)(TetrisNode &node, TetrisContext const *context);
        //顺时针旋转踢墙
        TetrisWallKickOpertion wall_kick_clockwise;
        //逆时针旋转踢墙
        TetrisWallKickOpertion wall_kick_counterclockwise;
        //转动180°踢墙
        TetrisWallKickOpertion wall_kick_opposite;
    };

    //指针网节点
    struct TetrisNode
    {
        //方块状态
        TetrisBlockStatus status;
        //方块操作函数
        TetrisOpertion op;
        //方块每行的数据
        uint32_t data[4];
        //方块每列的上沿高度
		int32_t top[4];
        //方块每列的下沿高度
		int32_t bottom[4];
        //方块在场景中的矩形位置
        int8_t row, height, col, width;
        //各种变形会触及到的最低高度
        int32_t low;

        //指针网索引
        //用于取代哈希表的hash

        size_t index;
        size_t index_filtered;

        //用于落点搜索优化
        std::vector<TetrisNode const *> const *land_point;

        //以下是指针网的数据
        //对应操作所造成的数据改变全都预置好,不需要再计算
        //如果为空,表示已经到达场景边界或者不支持该操作

        TetrisNode const *rotate_clockwise;
        TetrisNode const *rotate_counterclockwise;
        TetrisNode const *rotate_opposite;
        TetrisNode const *move_left;
        TetrisNode const *move_right;
        TetrisNode const *move_down;
        TetrisNode const *move_up;
        TetrisNode const *move_down_multi[max_height];

        //踢墙序列,依次尝试
        //遇到nullptr,表示序列结束

        TetrisNode const *wall_kick_clockwise[max_wall_kick];
        TetrisNode const *wall_kick_counterclockwise[max_wall_kick];
        TetrisNode const *wall_kick_opposite[max_wall_kick];

        //上下文...这个需要解释么?
        TetrisContext const *context;

        //检查当前块是否能够合并入场景
        bool check(TetrisMap const &map) const;
        //检查当前块是否是露天的
        bool open(TetrisMap const &map) const;
        //当前块合并入场景,同时更新场景数据
        size_t attach(TetrisMap &map) const;
        //探测合并后消的最低行
        int clear_low(TetrisMap &map) const;
        //探测合并后消的最低行
        int clear_high(TetrisMap &map) const;
        //计算当前块软降位置
        TetrisNode const *drop(TetrisMap const &map) const;
    };

    //节点标记.广搜的时候使用
    class TetrisNodeMark
    {
    private:
        struct Mark
        {
            Mark() : version(0)
            {
            }
            size_t version;
            std::pair<TetrisNode const *, char> data;
        };
        size_t version_;
        std::vector<Mark> data_;

    public:
        void init(size_t size);
        void clear();
        std::pair<TetrisNode const *, char> get(size_t index);
        std::pair<TetrisNode const *, char> get(TetrisNode const *key);
        bool set(TetrisNode const *key, TetrisNode const *node, char op);
        bool cover(TetrisNode const *key, TetrisNode const *node, char op);
        bool mark(TetrisNode const *key);
    };

    //节点标记.过滤了位置相同的节点
    class TetrisNodeMarkFiltered
    {
    private:
        struct Mark
        {
            Mark() : version(0)
            {
            }
            size_t version;
            std::pair<TetrisNode const *, char> data;
        };
        size_t version_;
        std::vector<Mark> data_;

    public:
        void init(size_t size);
        void clear();
        std::pair<TetrisNode const *, char> get(size_t index);
        std::pair<TetrisNode const *, char> get(TetrisNode const *key);
        bool set(TetrisNode const *key, TetrisNode const *node, char op);
        bool cover(TetrisNode const *key, TetrisNode const *node, char op);
        bool mark(TetrisNode const *key);
    };

    template<class TetrisRule, class AI, class Search>
    struct TetrisContextBuilder;

    //上下文对象.场景大小改变了需要重新初始化上下文
    class TetrisContext
    {
        template<class TetrisRule, class AI, class Search>
        friend struct TetrisContextBuilder;
    private:
        TetrisContext()
        {
        }
        //指针网数据
        std::unordered_map<TetrisBlockStatus, TetrisNode, TetrisBlockStatusHash, TetrisBlockStatusEqual> node_cache_;

        //规则信息

        std::map<std::pair<char, unsigned char>, TetrisOpertion> opertion_;
        std::map<char, TetrisBlockStatus(*)(TetrisContext const *)> generate_;

        //宽,高什么的...
        size_t width_, height_;
        //满行
        size_t full_;

        //一些用于加速的数据...
        std::map<char, std::vector<TetrisNode const *>> place_cache_;
        size_t type_max_;
        TetrisNode const *generate_cache_[256];
        char index_to_type_[256];
        size_t type_to_index_[256];

    public:
        enum PrepareResult : int
        {
            fail = 0, ok = 1, rebuild = 2,
        };
        //准备好上下文,返回fail表示上下错误
        PrepareResult prepare(int width, int height);

        int width() const;
        int height() const;
        size_t full() const;
        size_t type_max() const;
        size_t node_max() const;
        TetrisOpertion get_opertion(unsigned char t, unsigned char r) const;
        TetrisNode const *get(TetrisBlockStatus const &status) const;
        TetrisNode const *get(char t, int8_t x, int8_t y, uint8_t r) const;
        TetrisNode const *generate(char type) const;
        TetrisNode const *generate(size_t index) const;
        TetrisNode const *generate() const;
        bool create(TetrisBlockStatus const &status, TetrisNode &node) const;
    };

    template<class Type>
    struct TetrisCallInit
    {
        template<class CallType, class T>
        struct CallInit
        {
            template<class... Params>
            CallInit(CallType &type, Params const &... params)
            {
            }
        };
        template<class CallType>
        struct CallInit<CallType, std::true_type>
        {
            template<class... Params>
            CallInit(CallType &type, Params const &... params)
            {
                type.init(params...);
            }
        };
        struct Fallback
        {
            int init;
        };
        struct Derived : Type, Fallback
        {
        };
        template<typename U, U> struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::init> *);
        template<typename U>
        static std::true_type func(...);
    public:
        template<class... Params>
        TetrisCallInit(Type &type, Params const &... params)
        {
            CallInit<Type, decltype(func<Derived>(nullptr))>(type, params...);
        }
    };

    template<class AI, class Node>
    struct TetrisCallEval
    {
        template <typename T>
        struct function_traits : public function_traits<decltype(&AI::eval)>
        {
        };
        template <typename ClassType, typename ReturnType, typename... Args>
        struct function_traits<ReturnType(ClassType::*)(Args...) const>
        {
            enum
            {
                arity = sizeof...(Args)
            };

            typedef ReturnType result_type;
            template<unsigned int i>
            struct arg
            {
                typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
            };
        };
        typedef typename function_traits<AI>::template arg<0u>::type OtherNode;
        typedef typename function_traits<AI>::result_type result_type;
        template<class CallAI, class A, class B>
        struct CallEval
        {
            template<class Return, class TetrisNodeEx, class... Params>
            static Return eval(CallAI const &ai, TetrisNode const *node, Params const &... params)
            {
                typename std::remove_reference<OtherNode>::type node_ex(node);
                return ai.eval(node_ex, params...);
            }
        };
        template<class CallAI, class T>
        struct CallEval<CallAI, T, T>
        {
            template<class Return, class TetrisNodeEx, class... Params>
            static Return eval(CallAI const &ai, TetrisNodeEx &node, Params const &... params)
            {
                return ai.eval(node, params...);
            }
        };
    public:
        template<class... Params>
        static auto eval(AI const &ai, Node &node, Params const &... params)->result_type
        {
            return CallEval<AI, Node &, OtherNode &>::template eval<result_type, Node>(ai, node, params...);
        }
    };

    template<class Type>
    struct TetrisRuleInit
    {
        template<class CallType, class T>
        struct RuleInit
        {
            static bool init(int w, int h)
            {
                return true;
            }
        };
        template<class CallType>
        struct RuleInit<CallType, std::true_type>
        {
            static bool init(int w, int h)
            {
                return CallType().init(w, h);
            }
        };
        struct Fallback
        {
            int init;
        };
        struct Derived : Type, Fallback
        {
        };
        template<typename U, U> struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::init> *);
        template<typename U>
        static std::true_type func(...);
    public:
        static bool init(int w, int h)
        {
            return RuleInit<Type, decltype(func<Derived>(nullptr))>::init(w, h);
        }
    };

    template<class TetrisAI>
    struct TetrisAIHasIterated
    {
        struct Fallback
        {
            int iterated;
        };
        struct Derived : TetrisAI, Fallback
        {
        };
        template<typename U, U> struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::iterated> *);
        template<typename U>
        static std::true_type func(...);
    public:
        typedef decltype(func<Derived>(nullptr)) type;
    };

    template<class TetrisAI>
    struct TetrisHasConfig
    {
        struct Fallback
        {
            int Config;
        };
        struct Derived : TetrisAI, Fallback
        {
        };
        template<typename U, U> struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::Config> *);
        template<typename U>
        static std::true_type func(...);
    public:
        typedef decltype(func<Derived>(nullptr)) type;
    };

    template<class TetrisRule, class TetrisAI, class TetrisSearch>
    struct TetrisContextBuilder
    {
    private:
        template<class CallAI, class>
        struct AIConfig
        {
            class AIConfigHolder
            {
            public:
                typedef void AIConfigType;
                void const *ai_config() const
                {
                    return nullptr;
                }
                void *ai_config()
                {
                    return nullptr;
                }
            };
        };
        template<class CallAI>
        struct AIConfig<CallAI, std::true_type>
        {
            class AIConfigHolder
            {
            public:
                typedef typename CallAI::Config AIConfigType;
                AIConfigType const *ai_config() const
                {
                    return &ai_config_;
                }
                AIConfigType *ai_config()
                {
                    return &ai_config_;
                }
            private:
                AIConfigType ai_config_;
            };
        };
        template<class CallSearch, class>
        struct SearchConfig
        {
            class SearchConfigHolder
            {
            public:
                typedef void SearchConfigType;
                void const *search_config() const
                {
                    return nullptr;
                }
                void *search_config()
                {
                    return nullptr;
                }
            };
        };
        template<class CallSearch>
        struct SearchConfig<CallSearch, std::true_type>
        {
            class SearchConfigHolder
            {
            public:
                typedef typename CallSearch::Config SearchConfigType;
                SearchConfigType const *search_config() const
                {
                    return &status_config_;
                }
                SearchConfigType *search_config()
                {
                    return &status_config_;
                }
            private:
                SearchConfigType status_config_;
            };
        };
    public:
        class TetrisContextEx : public TetrisContext, public AIConfig<TetrisAI, typename TetrisHasConfig<TetrisAI>::type>::AIConfigHolder, public SearchConfig<TetrisSearch, typename TetrisHasConfig<TetrisSearch>::type>::SearchConfigHolder
        {
        };
        static TetrisContextEx *build_context()
        {
            TetrisContextEx *context = new TetrisContextEx();
            context->opertion_ = TetrisRule::get_opertion();
            context->generate_ = TetrisRule::get_generate();
            return context;
        }
    private:
        template<class, class>
        struct CallInit
        {
            static void call(TetrisAI &ai, TetrisContextEx const *context)
            {
                TetrisCallInit<TetrisAI>(ai, context, context->ai_config());
            }
            static void call(TetrisSearch &search, TetrisContextEx const *context)
            {
                TetrisCallInit<TetrisSearch>(search, context, context->search_config());
            }
        };
        template<class Unuse>
        struct CallInit<Unuse, void>
        {
            static void call(TetrisAI &ai, TetrisContextEx const *context)
            {
                TetrisCallInit<TetrisAI>(ai, context);
            }
            static void call(TetrisSearch &search, TetrisContextEx const *context)
            {
                TetrisCallInit<TetrisSearch>(search, context);
            }
        };
    public:
        static void init_ai(TetrisAI &ai, TetrisContextEx const *context)
        {
            CallInit<TetrisContextEx, typename TetrisContextEx::AIConfigType>::call(ai, context);
        }
        static void init_search(TetrisSearch &search, TetrisContextEx const *context)
        {
            CallInit<TetrisContextEx, typename TetrisContextEx::SearchConfigType>::call(search, context);
        }
    };

    template<class TetrisAI, class TetrisSearch>
    struct TetrisCore
    {
    private:
        template <typename TemplateElement>
        struct element_traits
        {
            typedef void Element;
        };
        template <typename TemplateElement>
        struct element_traits<std::vector<TemplateElement> const *>
        {
            typedef TemplateElement Element;
        };
        template <typename T>
        struct function_traits : public function_traits<decltype(&T::eval)>
        {
        };

        template <typename ClassType, typename ReturnType, typename... Args>
        struct function_traits<ReturnType(ClassType::*)(Args...) const>
        {
            typedef ReturnType result_type;
        };
    public:
        typedef typename element_traits<decltype(TetrisSearch().search(TetrisMap(), nullptr))>::Element LandPoint;
        typedef typename function_traits<TetrisAI>::result_type Status;
        typedef std::pair<LandPoint, Status> Result;

        template<class TreeNode>
        static void eval(TetrisAI &ai, TetrisMap &map, LandPoint &node, TreeNode *tree_node)
        {
            TetrisMap &new_map = tree_node->map;
            new_map = map;
            size_t clear = node->attach(new_map);
            tree_node->identity = node;
            tree_node->status = TetrisCallEval<TetrisAI, LandPoint>::eval(ai, tree_node->identity, new_map, map, clear, tree_node->parent->status);
        }
    };

    struct TetrisTreeNodeBase
    {
        TetrisTreeNodeBase() : node(' '), hold(' '), level(), flag()
        {
        }
        TetrisTreeNodeBase *base_parent, *base_left, *base_right;
        union
        {
            struct
            {
                char node;
                char hold;
                uint8_t level;
                uint8_t flag;
            };
            struct
            {
                char : 8;
                char : 8;
                uint8_t : 8;
                uint8_t is_dead : 1;
                uint8_t is_hold : 1;
                uint8_t is_hold_lock : 1;
                uint8_t is_black : 1;
                uint8_t is_nil : 1;
                uint8_t is_virtual : 1;
            };
        };
    };

    template<class Status, class TetrisAI, class TetrisSearch>
    struct TetrisTreeNode : public TetrisTreeNodeBase
    {
        typedef TetrisCore<TetrisAI, TetrisSearch> Core;
        struct Context
        {
        public:
            struct ValueTreeInterface
            {
                typedef decltype(TetrisTreeNode::status) key_t;
                typedef TetrisTreeNodeBase node_t;
                typedef TetrisTreeNode value_node_t;
                static key_t const &get_key(TetrisTreeNode *node)
                {
                    return node->status;
                }
                static bool is_nil(TetrisTreeNodeBase *node)
                {
                    return node->is_nil;
                }
                static void set_nil(TetrisTreeNodeBase *node, bool nil)
                {
                    node->is_nil = nil;
                }
                static TetrisTreeNodeBase *get_parent(TetrisTreeNodeBase *node)
                {
                    return node->base_parent;
                }
                static void set_parent(TetrisTreeNodeBase *node, TetrisTreeNodeBase *parent)
                {
                    node->base_parent = parent;
                }
                static TetrisTreeNodeBase *get_left(TetrisTreeNodeBase *node)
                {
                    return node->base_left;
                }
                static void set_left(TetrisTreeNodeBase *node, TetrisTreeNodeBase *left)
                {
                    node->base_left = left;
                }
                static TetrisTreeNodeBase *get_right(TetrisTreeNodeBase *node)
                {
                    return node->base_right;
                }
                static void set_right(TetrisTreeNodeBase *node, TetrisTreeNodeBase *right)
                {
                    node->base_right = right;
                }
                static bool is_black(TetrisTreeNodeBase *node)
                {
                    return node->is_black;
                }
                static void set_black(TetrisTreeNodeBase *node, bool black)
                {
                    node->is_black = black;
                }
                static bool predicate(key_t const &left, key_t const &right)
                {
                    return right < left;
                }
            };
        public:
            Context() : version(), is_complete(), is_open_hold(), width(), total(), avg()
            {
            }
            void release()
            {
                for(auto node : tree_cache_)
                {
                    delete node;
                }
            }
        public:
            typedef zzz::rb_tree<ValueTreeInterface> value_tree_t;
            size_t version;
            TetrisContext const *context;
            TetrisAI *ai;
            TetrisSearch *search;
            std::vector<value_tree_t> sort;
            std::vector<value_tree_t> wait;
            bool is_complete;
            bool is_open_hold;
            bool is_virtual;
            bool unused_bool;
            size_t max_length;
            size_t width;
            std::vector<TetrisTreeNode *> tree_cache_;
            TetrisNode virtual_flag;
            TetrisNode const *current;
            double total;
            double avg;
        public:
            TetrisTreeNode *alloc(TetrisTreeNode *parent)
            {
                TetrisTreeNode *node;
                if(!tree_cache_.empty())
                {
                    node = tree_cache_.back();
                    tree_cache_.pop_back();
                    node->version = version - 1;
                }
                else
                {
                    node = new TetrisTreeNode(this);
                }
                node->parent = parent;
                return node;
            }
            void dealloc(TetrisTreeNode *node)
            {
                for(auto child : node->children)
                {
                    dealloc(child);
                }
                node->children.clear();
                node->node_flag.clear();
                node->node = ' ';
                node->hold = ' ';
                node->level = 0;
                node->flag = 0;
                node->next.clear();
                tree_cache_.push_back(node);
            }
        };
        struct StatusTreeInterface
        {
            typedef TetrisBlockStatus key_t;
            typedef TetrisTreeNodeBase node_t;
            typedef TetrisTreeNode value_node_t;
            static TetrisBlockStatus const &get_key(TetrisTreeNode *node)
            {
                return node->identity->status;
            }
            static bool is_nil(TetrisTreeNodeBase *node)
            {
                return node->is_nil;
            }
            static void set_nil(TetrisTreeNodeBase *node, bool nil)
            {
                node->is_nil = nil;
            }
            static TetrisTreeNodeBase *get_parent(TetrisTreeNodeBase *node)
            {
                return node->base_parent;
            }
            static void set_parent(TetrisTreeNodeBase *node, TetrisTreeNodeBase *parent)
            {
                node->base_parent = parent;
            }
            static TetrisTreeNodeBase *get_left(TetrisTreeNodeBase *node)
            {
                return node->base_left;
            }
            static void set_left(TetrisTreeNodeBase *node, TetrisTreeNodeBase *left)
            {
                node->base_left = left;
            }
            static TetrisTreeNodeBase *get_right(TetrisTreeNodeBase *node)
            {
                return node->base_right;
            }
            static void set_right(TetrisTreeNodeBase *node, TetrisTreeNodeBase *right)
            {
                node->base_right = right;
            }
            static bool is_black(TetrisTreeNodeBase *node)
            {
                return node->is_black;
            }
            static void set_black(TetrisTreeNodeBase *node, bool black)
            {
                node->is_black = black;
            }
            static bool predicate(key_t const &left, key_t const &right)
            {
                return TetrisBlockStatusCompare()(left, right);
            }
        };
        struct TetrisNodeFlag
        {
            TetrisNode const *flag[2];
            TetrisNodeFlag()
            {
                clear();
            }
            bool empty()
            {
                return flag[0] == nullptr;
            }
            bool check(TetrisNode const *node1)
            {
                assert(node1 != nullptr);
                return flag[0] == node1 && flag[1] == nullptr;
            }
            bool check(TetrisNode const *node1, TetrisNode const *node2)
            {
                assert(node1 != nullptr);
                assert(node2 != nullptr);
                return (flag[0] == node1 && flag[1] == node2) || (flag[0] == node2 && flag[1] == node1);
            }
            void set(TetrisNode const *node1)
            {
                assert(node1 != nullptr);
                flag[0] = node1;
                flag[1] = nullptr;
            }
            void set(TetrisNode const *node1, TetrisNode const *node2)
            {
                assert(node1 != nullptr);
                assert(node2 != nullptr);
                flag[0] = node1;
                flag[1] = node2;
            }
            void clear()
            {
                flag[0] = nullptr;
                flag[1] = nullptr;
            }
        };
        typedef zzz::rb_tree<StatusTreeInterface> children_sort_t;
        TetrisTreeNode(Context *_context) : TetrisTreeNodeBase(), context(_context), version(context->version - 1), parent(), identity()
        {
        }
        Context *context;
        size_t version;
        TetrisMap map;
        typename Core::LandPoint identity;
        Status status;
        TetrisTreeNode *parent;
        std::vector<TetrisTreeNode *> children;
        TetrisNodeFlag node_flag;
        std::vector<char> next;
        void (TetrisTreeNode::*search_ptr)(bool);

        TetrisTreeNode *update_root(TetrisMap const &_map)
        {
            if(map == _map)
            {
                return this;
            }
            TetrisTreeNode *new_root = nullptr;
            for(auto it = children.begin(); it != children.end(); ++it)
            {
                auto &child = *it;
                if(child->map == _map)
                {
                    new_root = child;
                    children.erase(it);
                    break;
                }
            }
            if(new_root == nullptr)
            {
                new_root = context->alloc(nullptr);
                new_root->map = _map;
            }
            context->dealloc(this);
            context->is_complete = false;
            new_root->parent = new_root;
            return new_root;
        }
        TetrisTreeNode *update(TetrisMap const &_map, Status const &status, TetrisNode const *_node, char const *_next, size_t _next_length)
        {
            TetrisTreeNode *root = update_root(_map);
            bool is_virtual;
            if(_next_length > 0 && _next[_next_length - 1] == '?')
            {
                --_next_length;
                is_virtual = true;
            }
            else
            {
                is_virtual = false;
            }
            if(root != this || context->is_open_hold || is_virtual != context->is_virtual || _node != context->current || _next_length != root->next.size() || std::memcmp(_next, root->next.data(), _next_length) != 0)
            {
                ++context->version;
                context->total += context->width;
                context->avg = context->total / context->version;
                context->width = 0;
                context->wait.clear();
                context->sort.clear();
                context->max_length = _next_length;
                context->wait.resize(context->max_length + 1);
                context->sort.resize(context->max_length + 1);
            }
            context->is_open_hold = false;
            context->is_virtual = is_virtual;
            context->current = _node;
            root->node = _node->status.t;
            root->status = status;
            root->next.assign(_next, _next + _next_length);
            return root;
        }
        TetrisTreeNode *update(TetrisMap const &_map, Status const &status, TetrisNode const *_node, char _hold, bool _hold_lock, char const *_next, size_t _next_length)
        {
            TetrisTreeNode *root = update_root(_map);
            bool is_virtual;
            if(_next_length > 0 && _next[_next_length - 1] == '?')
            {
                --_next_length;
                is_virtual = true;
            }
            else
            {
                is_virtual = false;
            }
            if(root != this || !context->is_open_hold || is_virtual != context->is_virtual || _node != context->current || _hold != root->hold || !!_hold_lock != root->is_hold_lock || _next_length != root->next.size() || std::memcmp(_next, root->next.data(), _next_length) != 0)
            {
                ++context->version;
                context->total += context->width;
                context->avg = context->total / context->version;
                context->width = 0;
                context->wait.clear();
                context->sort.clear();
                context->max_length = _next_length;
                if(_hold != ' ' && (_next_length > 1 || !_hold_lock))
                {
                    ++context->max_length;
                }
                context->wait.resize(context->max_length + 1);
                context->sort.resize(context->max_length + 1);
            }
            context->is_open_hold = true;
            context->is_virtual = is_virtual;
            context->current = _node;
            root->status = status;
            root->node = _node->status.t;
            root->hold = _hold;
            root->is_hold = false;
            root->is_hold_lock = _hold_lock;
            root->next.assign(_next, _next + _next_length);
            return root;
        }
        void search(TetrisNode const *search_node, bool is_hold)
        {
            if(node_flag.empty())
            {
                node_flag.set(search_node);
                for(auto land_point_node : *context->search->search(map, search_node))
                {
                    TetrisTreeNode *child = context->alloc(this);
                    Core::eval(*context->ai, map, land_point_node, child);
                    child->is_hold = is_hold;
                    children.push_back(child);
                }
            }
            else if(!node_flag.check(search_node))
            {
                node_flag.set(search_node);
                children_sort_t old;
                old.insert(children.begin(), children.end());
                children.clear();
                for(auto land_point_node : *context->search->search(map, search_node))
                {
                    TetrisTreeNode *child;
                    auto find = old.find(land_point_node->status);
                    if(find != old.end())
                    {
                        child = &*find;
                        old.erase(find);
                    }
                    else
                    {
                        child = context->alloc(this);
                        Core::eval(*context->ai, map, land_point_node, child);
                    }
                    child->is_hold = is_hold;
                    children.push_back(child);
                }
                for(auto &child : old)
                {
                    context->dealloc(&child);
                }
            }
        }
        void search(TetrisNode const *search_node, TetrisNode const *hold_node)
        {
            if(search_node == hold_node)
            {
                return search(search_node, false);
            }
            if(search_node->status.t == hold_node->status.t)
            {
                if(node_flag.empty())
                {
                    node_flag.set(search_node, hold_node);
                    children_sort_t sort;
                    for(auto land_point_node : *context->search->search(map, search_node))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core::eval(*context->ai, map, land_point_node, child);
                        child->is_hold = false;
                        children.push_back(child);
                        sort.insert(child);
                    }
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        if(sort.find(land_point_node->status) != sort.end())
                        {
                            continue;
                        }
                        TetrisTreeNode *child = context->alloc(this);
                        Core::eval(*context->ai, map, land_point_node, child);
                        child->is_hold = true;
                        children.push_back(child);
                    }
                }
                else if(!node_flag.check(search_node, hold_node))
                {
                    node_flag.set(search_node, hold_node);
                    children_sort_t old;
                    children_sort_t sort;
                    old.insert(children.begin(), children.end());
                    children.clear();
                    for(auto land_point_node : *context->search->search(map, search_node))
                    {
                        TetrisTreeNode *child;
                        auto find = old.find(land_point_node->status);
                        if(find != old.end())
                        {
                            child = &*find;
                            old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core::eval(*context->ai, map, land_point_node, child);
                        }
                        child->is_hold = false;
                        children.push_back(child);
                        sort.insert(child);
                    }
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        if(sort.find(land_point_node->status) != sort.end())
                        {
                            continue;
                        }
                        TetrisTreeNode *child;
                        auto find = old.find(land_point_node->status);
                        if(find != old.end())
                        {
                            child = &*find;
                            old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core::eval(*context->ai, map, land_point_node, child);
                        }
                        child->is_hold = true;
                        children.push_back(child);
                    }
                    for(auto &child : old)
                    {
                        context->dealloc(&child);
                    }
                }
            }
            else
            {
                if(node_flag.empty())
                {
                    node_flag.set(search_node, hold_node);
                    for(auto land_point_node : *context->search->search(map, search_node))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core::eval(*context->ai, map, land_point_node, child);
                        child->is_hold = false;
                        children.push_back(child);
                    }
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core::eval(*context->ai, map, land_point_node, child);
                        child->is_hold = true;
                        children.push_back(child);
                    }
                }
                else if(!node_flag.check(search_node, hold_node))
                {
                    node_flag.set(search_node, hold_node);
                    children_sort_t old;
                    old.insert(children.begin(), children.end());
                    children.clear();
                    for(auto land_point_node : *context->search->search(map, search_node))
                    {
                        TetrisTreeNode *child;
                        auto find = old.find(land_point_node->status);
                        if(find != old.end())
                        {
                            child = &*find;
                            old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core::eval(*context->ai, map, land_point_node, child);
                        }
                        child->is_hold = false;
                        children.push_back(child);
                    }
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        TetrisTreeNode *child;
                        auto find = old.find(land_point_node->status);
                        if(find != old.end())
                        {
                            child = &*find;
                            old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core::eval(*context->ai, map, land_point_node, child);
                        }
                        child->is_hold = true;
                        children.push_back(child);
                    }
                    for(auto &child : old)
                    {
                        context->dealloc(&child);
                    }
                }
            }
        }
        void search()
        {
            assert(node_flag.empty());
            for(size_t i = 0; i < context_->type_max(); ++i)
            {
                for(auto land_point_node : *context->search->search(map, context_->generate(i)))
                {
                    TetrisTreeNode *child = context->alloc(this);
                    Core::eval(*context->ai, map, land_point_node, child);
                    child->is_hold = false;
                    children.push_back(child);
                }
            }
        }
        template<bool EnableHold>
        bool build_children()
        {
            if(version == context->version || is_dead)
            {
                return false;
            }
            version = context->version;
            if(parent == this)
            {
                assert(context->current->status.t == node);
                level = 0;
                if(EnableHold)
                {
                    if(hold == ' ' || is_hold_lock)
                    {
                        search(context->current, false);
                    }
                    else
                    {
                        search(context->current, context->context->generate(hold));
                    }
                }
                else
                {
                    search(context->current, false);
                }
                return true;
            }
            level = parent->level + 1;
            if(EnableHold)
            {
                if(is_hold)
                {
                    if(parent->hold == ' ' && parent->next.empty())
                    {
                        node = ' ';
                        hold = parent->node;
                        next.clear();
                    }
                    else
                    {
                        node = parent->next.front();
                        hold = parent->node;
                        next.assign(parent->next.begin() + 1, parent->next.end());
                    }
                }
                else
                {
                    node = parent->next.front();
                    hold = parent->hold;
                    next.assign(parent->next.begin() + 1, parent->next.end());
                }
                assert(node != ' ' || hold != ' ');
                if(node == ' ')
                {
                    search(context->context->generate(hold), true);
                }
                else
                {
                    if(hold == ' ')
                    {
                        search(context->context->generate(node), context->context->generate(parent->next.front()));
                    }
                    else
                    {
                        search(context->context->generate(node), context->context->generate(hold));
                    }
                }
            }
            else
            {
                assert(!parent->next.empty());
                node = parent->next.front();
                next.assign(parent->next.begin() + 1, parent->next.end());
                search(context->context->generate(node), false);
            }
            if(children.empty())
            {
                is_dead = true;
                return false;
            }
            return true;
        }
        template<bool EnableHold>
        bool run()
        {
            if(context->is_complete)
            {
                return false;
            }
            assert(parent == this);
            if(context->width == 0)
            {
                build_children<EnableHold>();
                context->wait.back().insert(children.begin(), children.end());
            }
            ++context->width;
            bool complete = true;
            size_t next_length = context->max_length;
            while(next_length-- > 0)
            {
                size_t level_prune_hold = (next_length * 36 / 10) / context->max_length + 1;
                auto wait = &context->wait[next_length + 1];
                if(level_prune_hold <= wait->size())
                {
                    complete = false;
                }
                if(wait->empty())
                {
                    continue;
                }
                auto sort = &context->sort[next_length + 1];
                auto next = &context->wait[next_length];
                do
                {
                    TetrisTreeNode *child = &*wait->begin();
                    wait->erase(child);
                    sort->insert(child);
                    --level_prune_hold;
                    if(child->build_children<EnableHold>())
                    {
                        next->insert(child->children.begin(), child->children.end());
                    }
                } while(level_prune_hold > 0 && !wait->empty());
            }
            if(complete)
            {
                context->is_complete = true;
                return true;
            }
            return false;
        }
        std::pair<TetrisTreeNode const *, Status const *> get_best()
        {
            TetrisTreeNode *best = nullptr;
            for(size_t i = 0; i < context->wait.size() && i < context->sort.size(); ++i)
            {
                auto wait_best = (context->wait.size() <= i || context->wait[i].empty()) ? nullptr : &*context->wait[i].begin();
                auto sort_best = (context->sort.size() <= i || context->sort[i].empty()) ? nullptr : &*context->sort[i].begin();
                if(wait_best == nullptr)
                {
                    if(sort_best == nullptr)
                    {
                        continue;
                    }
                    else
                    {
                        best = sort_best;
                    }
                }
                else
                {
                    if(sort_best == nullptr)
                    {
                        best = wait_best;
                    }
                    else
                    {
                        best = sort_best->status < wait_best->status ? wait_best : sort_best;
                    }
                }
                break;
            }
            if(best == nullptr)
            {
                return std::make_pair(nullptr, nullptr);
            }
            while(best->parent->parent != nullptr)
            {
                best = best->parent;
            }
            return std::make_pair(best, &best->status);
        }
    };

    template<class TetrisRule, class TetrisAI, class TetrisSearch>
    class TetrisEngine
    {
    private:
        typedef TetrisCore<TetrisAI, TetrisSearch> Core;
        typedef TetrisTreeNode<typename Core::Status, TetrisAI, TetrisSearch> TreeNode;
        typedef TetrisContextBuilder<TetrisRule, TetrisAI, TetrisSearch> ContextBuilder;
        typedef typename Core::LandPoint LandPoint;
        typename ContextBuilder::TetrisContextEx *context_;
        TetrisAI ai_;
        TetrisSearch search_;
        typename TreeNode::Context tree_context_;
        TreeNode *tree_root_;

    public:
        typedef typename Core::Status Status;
        struct RunResult
        {
            RunResult() : target(), status(), change_hold()
            {
            }
            RunResult(std::pair<TreeNode const *, typename Status const *> const &_result) : target(_result.first ? _result.first->identity : nullptr), status(_result.second), change_hold()
            {
            }
            RunResult(std::pair<TreeNode const *, typename Status const *> const &_result, bool _change_hold) : target(_result.first ? _result.first->identity : nullptr), status(_result.second), change_hold(_change_hold)
            {
            }
            LandPoint target;
            typename Status const *status;
            bool change_hold;
        };

    public:
        TetrisEngine() : context_(ContextBuilder::build_context()), ai_(), tree_root_(new TreeNode(&tree_context_))
        {
            tree_context_.context = context_;
            tree_context_.ai = &ai_;
            tree_context_.search = &search_;
        }
        ~TetrisEngine()
        {
            tree_context_.dealloc(tree_root_);
            tree_context_.release();
            delete context_;
        }
        //从状态获取当前块
        TetrisNode const *get(TetrisBlockStatus const &status) const
        {
            return context_->get(status);
        }
        //上下文对象...用来做什么呢= =?
        TetrisContext const *context() const
        {
            return context_;
        }
        //AI名称
        std::string ai_name() const
        {
            return ai_.ai_name();
        }
        auto ai_config()->decltype(context_->ai_config())
        {
            return context_->ai_config();
        }
        auto ai_config() const->decltype(context_->ai_config())
        {
            return context_->ai_config();
        }
        auto search_config()->decltype(context_->search_config())
        {
            return context_->search_config();
        }
        auto search_config() const->decltype(context_->search_config())
        {
            return context_->search_config();
        }
        //准备好上下文
        bool prepare(int width, int height)
        {
            if(!TetrisRuleInit<TetrisRule>::init(width, height))
            {
                return false;
            }
            TetrisContext::PrepareResult result = context_->prepare(width, height);
            if(result == TetrisContext::rebuild)
            {
                ContextBuilder::init_ai(ai_, context_);
                ContextBuilder::init_search(search_, context_);
                return true;
            }
            else if(result == TetrisContext::fail)
            {
                return false;
            }
            return true;
        }
        //update!强制刷新上下文
        void update()
        {
            ++tree_context_.version;
            tree_context_.total += tree_context_.width;
            tree_context_.avg = tree_context_.total / tree_context_.version;
            tree_context_.width = 0;
        }
        //run!
        RunResult run(TetrisMap const &map, Status const &status, TetrisNode const *node, char const *next, size_t next_length, time_t limit = 100)
        {
            if(node == nullptr || !node->check(map))
            {
                return RunResult();
            }
            double now = clock() / double(CLOCKS_PER_SEC), end = now + limit / 1000.;
            tree_root_ = tree_root_->update(map, status, node, next, next_length);
            do
            {
                if(tree_root_->run<false>())
                {
                    break;
                }
            }
            while((now = clock() / double(CLOCKS_PER_SEC)) < end);
            return RunResult(tree_root_->get_best());
        }
        //带hold的run!
        RunResult run_hold(TetrisMap const &map, Status const &status, TetrisNode const *node, char hold, bool hold_free, char const *next, size_t next_length, time_t limit = 100)
        {
            if(node == nullptr || !node->check(map))
            {
                return RunResult();
            }
            double now = clock() / double(CLOCKS_PER_SEC), end = now + limit / 1000.;
            tree_root_ = tree_root_->update(map, status, node, hold, !hold_free, next, next_length);
            do
            {
                if(tree_root_->run<true>())
                {
                    break;
                }
            } while((now = clock() / double(CLOCKS_PER_SEC)) < end);
            auto best = tree_root_->get_best();
            return RunResult(best, best.first == nullptr ? false : best.first->is_hold);
        }
        //根据run的结果得到一个操作路径
        std::vector<char> make_path(TetrisNode const *node, LandPoint const &land_point, TetrisMap const &map, bool cut_drop = true)
        {
            auto path = search_.make_path(node, land_point, map);
            if(cut_drop)
            {
                while(!path.empty() && (path.back() == 'd' || path.back() == 'D'))
                {
                    path.pop_back();
                }
            }
            return path;
        }
        //根据run的结果得到一组按键状态
        std::vector<char> make_status(TetrisNode const *node, LandPoint const &land_point, TetrisMap const &map)
        {
            return search_.make_status(node, land_point, map);
        }
        //单块评价
        template<class Node>
        void search(TetrisNode const *node, TetrisMap const &map, std::vector<Node> &result)
        {
            auto const *land_point = search_.search(map, node);
            result.assign(land_point->begin(), land_point->end());
        }
    };

}

namespace m_tetris_rule_tools
{
    using namespace m_tetris;


    //创建一个节点(只支持4x4矩阵,这里包含了矩阵收缩)
    TetrisNode create_node(size_t w, size_t h, char T, int8_t X, int8_t Y, uint8_t R, uint32_t line1, uint32_t line2, uint32_t line3, uint32_t line4, TetrisOpertion const &op);

    //创建一个节点(只支持4x4矩阵,这里包含了矩阵收缩)
    template<char T, int8_t X, int8_t Y, uint8_t R, uint32_t line1, uint32_t line2, uint32_t line3, uint32_t line4>
    TetrisNode create_node(size_t w, size_t h, TetrisOpertion const &op)
    {
        static_assert(X < 0 || X >= 4 || Y < 0 || Y >= 4 || (line1 || line2 || line3 || line3), "data error");
        return create_node(w, h, T, X, Y, R, line1, line2, line3, line4, op);
    }

    //一个通用的旋转
    bool rotate_default(TetrisNode &node, unsigned char R, TetrisContext const *context);

    //一个通用的旋转模板
    template<unsigned char R>
    bool rotate_template(TetrisNode &node, TetrisContext const *context)
    {
        return rotate_default(node, R, context);
    }

    //左移,右移,上移,下移...

    bool move_left(TetrisNode &node, TetrisContext const *context);
    bool move_right(TetrisNode &node, TetrisContext const *context);
    bool move_up(TetrisNode &node, TetrisContext const *context);
    bool move_down(TetrisNode &node, TetrisContext const *context);
}