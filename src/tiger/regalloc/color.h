#ifndef TIGER_COMPILER_COLOR_H
#define TIGER_COMPILER_COLOR_H

#include <utility>

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/util/graph.h"

#include <map>
#include <set>

namespace col {
    struct Result {
        Result() : coloring(nullptr), spills(nullptr) {
        }
        Result(temp::Map *coloring, live::INodeListPtr spills)
            : coloring(coloring), spills(spills) {
        }
        temp::Map *coloring;
        live::INodeListPtr spills;
    };

    class Color {
        /* TODO: Put your lab6 code here */
    public:
        Color(live::LiveGraph liveness, std::list<temp::Temp *> added_temp_list);
        ~Color() = default;

        void Paint();

        col::Result GetResult() {
            return result_;
        }

        [[nodiscard]] bool ignoreAdded() const;
        void setIgnoreAdded(bool value);

        [[nodiscard]] bool useCache() const;
        void setUseCache(bool value);

    protected:
        bool m_ignoreAdded; // 是否不把新增的寄存器溢出
        bool m_useCache;    // 是否把图存到成员变量中（最后没用到）

    protected:
        live::LiveGraph liveness_;
        col::Result result_;

        // 为了方便一律不用指针
        temp::TempList added_temps;

    protected:
        std::map<temp::Temp *, std::string> precolored; // 机器寄存器集合，只是一个缓存

        live::INodeList simplifyWorklist; // 低度数、传送无关的节点表
        live::INodeList freezeWorklist;   // 低度数、传送有关的节点表
        live::INodeList spillWorklist;    // 高度数的节点表

        live::INodeList coalescedNodes; // 已合并的寄存器集合
        live::INodeList coloredNodes;   // 已成功着色的节点集合

        live::INodeList selectStack; // 包含从图中删除的临时变量的栈

        live::MoveList coalescedMoves;   // 已经合并的传送指令集合
        live::MoveList constrainedMoves; // 源与目标操作数冲突的传送指令集合
        live::MoveList frozenMoves; // 不再考虑合并的传送指令集合（一般是合并和简化都失效时新增）
        live::MoveList worklistMoves; // 有可能合并的传送指令集合
        live::MoveList activeMoves;   // 还没有做好合并准备的传送指令集合

        std::map<live::INode *, live::MoveList> moveList; // 节点 -> 与该节点传送相关的节点列表
        std::map<live::INode *, live::INode *> alias; // 已合并的节点的代表元

        // 由于不更改原图，所以需要先存下所有信息
        std::map<live::INode *, int> degrees;                     // 节点的度数
        std::set<std::pair<live::INode *, live::INode *>> adjSet; // 冲突边集合
        std::map<live::INodePtr, live::INodeList> adjList;        // 冲突邻接表

        std::map<temp::Temp *, std::string> color_map;
        live::INodeList spilled_nodes;

        static int K();

        bool isPrecolored(temp::Temp *t) const;

        live::MoveList NodeMoves(live::INode *node) const; // 与该节点相关的传送指令列表

        bool MoveRelated(live::INode *node) const; // 判断该节点当前是否是传送相关

        live::INodeList Adjacent(live::INode *node) const; // 邻接节点列表

        live::INode *GetAlias(live::INode *node) const; // 并查集查找

        void DecrementDegree(live::INode *node);

        void EnableMoves(const live::INodeList &nodes); // 使给定节点准备好合并

        void FreezeMoves(live::INode *u);

        void AddWorkList(live::INode *node); // 将该节点从传送有关视为传送无关

        // 合并预着色寄存器的启发式函数（George），要么是低度数要么已经冲突
        bool OK(live::INode *t, live::INode *r);

        // 对所有节点，要么是低度数与另一个节点要么已经冲突
        bool OK(const live::INodeList &nodes, live::INode *r);

        // 合并保守启发式函数（Briggs），高度数节点不超过K
        bool Conservative(const live::INodeList &nodes);

        void Combine(live::INode *u, live::INode *v);

        void Build();

        void MakeWorklist();

        void Simplify(); // 执行简化

        void Coalesce(); // 执行合并

        void Freeze(); // 执行冻结

        void SelectSpill(); // 挑选节点溢出

        void AssignColors(); // 着色

    protected:
        // 对图操作

        void AddEdge(live::INode *u, live::INode *v); // 添加边到adjSet与adjList

        void IsolateNode(live::INode *node) const; // 删掉一个节点的所有边
    };

} // namespace col

#endif // TIGER_COMPILER_COLOR_H
