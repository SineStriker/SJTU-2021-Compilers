#include "tiger/regalloc/color.h"

extern frame::RegManager *reg_manager;

namespace col {
    /* TODO: Put your lab6 code here */

    Color::Color(live::LiveGraph liveness, std::list<temp::Temp *> added_temp_list)
        : liveness_(liveness), added_temps(temp::TempList(std::move(added_temp_list))),
          m_ignoreAdded(false), m_useCache(false) {
        temp::TempList *pc_regs = reg_manager->Registers();

        // 初始化预着色寄存器集合
        const auto &pc_regs_list = pc_regs->GetList();
        for (auto t : pc_regs_list) {
            auto name = reg_manager->temp_map_->Look(t);
            assert(name);
            precolored.insert(std::make_pair(t, *name));
        }
    }

    void Color::Paint() {
        Build();
        MakeWorklist();

        do {
            if (!simplifyWorklist.isEmpty()) {
                Simplify();
            } else if (!worklistMoves.isEmpty()) {
                Coalesce();
            } else if (!freezeWorklist.isEmpty()) {
                Freeze();
            } else if (!spillWorklist.isEmpty()) {
                SelectSpill();
            }
        } while (!simplifyWorklist.isEmpty() || !worklistMoves.isEmpty() ||
                 !freezeWorklist.isEmpty() || !spillWorklist.isEmpty());

        AssignColors();

        // 转移结果
        result_.coloring = temp::Map::Empty();
        for (auto &it : color_map) {
            result_.coloring->Enter(it.first, new std::string(it.second));
        }
        result_.spills = new live::INodeList();
        result_.spills->CatList(&spilled_nodes);
    }

    bool Color::ignoreAdded() const {
        return m_ignoreAdded;
    }

    void Color::setIgnoreAdded(bool value) {
        m_ignoreAdded = value;
    }

    bool Color::useCache() const {
        return m_useCache;
    }

    void Color::setUseCache(bool value) {
        m_useCache = value;
    }

    int Color::K() {
        return reg_manager->ColorNum();
    }

    bool Color::isPrecolored(temp::Temp *t) const {
        return precolored.find(t) != precolored.end();
    }

    live::MoveList Color::NodeMoves(live::INode *node) const {
        auto it = moveList.find(node);
        if (it == moveList.end()) {
            return live::MoveList();
        }
        // moveList[n] ∩ (activeMoves ∪ worklistMoves)
        return (it->second).Intersected(activeMoves.Unioned(worklistMoves));
    }

    bool Color::MoveRelated(live::INode *node) const {
        // NodeMove(n) != Ø
        return !NodeMoves(node).isEmpty();
    }

    live::INodeList Color::Adjacent(live::INode *node) const {
        auto it = adjList.find(node);
        assert(it != adjList.end());
        // adjList[n] \ (selectStack ∪ coalescedNodes)
        return it->second.Diffed(selectStack.Unioned(coalescedNodes));

        // return node->Adj()->Diffed(selectStack.Unioned(coloredNodes));
    }

    live::INode *Color::GetAlias(live::INode *node) const {
        if (coalescedNodes.Contain(node)) {
            auto it = alias.find(node);
            assert(it != alias.end());
            return GetAlias(it->second);
        }
        return node;
    }

    void Color::DecrementDegree(live::INode *node) {
        // 假设a节点被简化了，那么node就表示a的临界点，DecrementDegree会依次操作a的所有临界点
        if (isPrecolored(node->NodeInfo())) {
            return;
        }

        auto it = degrees.find(node);
        assert(it != degrees.end());

        int d = it->second;
        it->second--;

        // 当它的度数从k变成k-1时，如果是传送相关的，那么可能变为可合并的
        if (d == K()) {
            // EnableMoves({m} ∪ Adjacent(m))
            EnableMoves(Adjacent(node).Unioned(live::INodeList({node})));
            spillWorklist.DeleteNode(node);
            if (MoveRelated(node)) {
                freezeWorklist.Append(node);
            } else {
                simplifyWorklist.Append(node);
            }
        }
    }

    void Color::EnableMoves(const live::INodeList &nodes) {
        const auto &node_list = nodes.GetList();
        for (auto node : node_list) {
            auto node_moves = NodeMoves(node);
            const auto &move_list = node_moves.GetList();
            for (auto move : move_list) {
                if (activeMoves.Contain(move.first, move.second)) {
                    // activeMoves <- activeMoves \ m
                    activeMoves.Delete(move.first, move.second);
                    // worklistMoves <- worklistMoves ∪ m
                    worklistMoves.Append(move.first, move.second);
                }
            }
        }
    }

    void Color::FreezeMoves(live::INode *u) {
        auto node_moves = NodeMoves(u);
        const auto &move_list = node_moves.GetList();
        for (auto move : move_list) {
            auto x = move.first;  // src
            auto y = move.second; // dest

            live::INode *v;
            if (GetAlias(y) == GetAlias(u)) {
                v = GetAlias(x);
            } else {
                v = GetAlias(y);
            }
            activeMoves.Delete(x, y);
            frozenMoves.Append(x, y);

            if ((!isPrecolored(v->NodeInfo())) && NodeMoves(v).isEmpty() && degrees[v] < K()) {
                freezeWorklist.DeleteNode(v);
                simplifyWorklist.Append(v);
            }
        }
    }

    void Color::AddWorkList(live::INode *node) {
        int d = degrees[node];
        // 是低度数节点并非传送相关
        if (!isPrecolored(node->NodeInfo()) && !MoveRelated(node) && d < K()) {
            freezeWorklist.DeleteNode(node);
            simplifyWorklist.Append(node);
        }
    }

    bool Color::OK(live::INode *t, live::INode *r) {
        // int d = m_useCache ? degrees[t] : t->Degree();
        // bool c = m_useCache ? (adjSet.find(std::make_pair(t, r)) != adjSet.end()) : (!t->Adj(r));
        int d = degrees[t];
        bool c = (adjSet.find(std::make_pair(t, r)) != adjSet.end());
        // 要么是低度数，要么本身就是冲突的
        return isPrecolored(t->NodeInfo()) || (d < K()) || c;
    }

    bool Color::OK(const live::INodeList &nodes, live::INode *r) {
        // 批量处理，必须全部是OK才返回true
        const auto &node_list = nodes.GetList();
        bool res = true;
        for (auto node : node_list) {
            if (!OK(node, r)) {
                res = false;
                break;
            }
        }
        return res;
    }

    bool Color::Conservative(const live::INodeList &nodes) {
        int k = 0;
        const auto &node_list = nodes.GetList();
        for (auto node : node_list) {
            // int d = m_useCache ? degrees[node] : node->Degree();
            int d = degrees[node];
            if (isPrecolored(node->NodeInfo()) || d >= K()) {
                k++;
            }
        }
        return k < K();
    }

    void Color::Combine(live::INode *u, live::INode *v) {
        if (freezeWorklist.Contain(v)) {
            freezeWorklist.DeleteNode(v);
        } else {
            spillWorklist.DeleteNode(v);
        }
        coalescedNodes.Append(v);

        // 更新代表元
        alias[v] = u;

        auto it1 = moveList.find(u);
        auto it2 = moveList.find(v);
        assert(it1 != moveList.end());
        assert(it2 != moveList.end());

        // 合并
        it1->second = it1->second.Unioned(it2->second);

        EnableMoves(live::INodeList({v}));

        auto adjacent = Adjacent(v);
        const auto &adj_list = adjacent.GetList();
        for (auto adj_node : adj_list) {
            AddEdge(adj_node, u);
            DecrementDegree(adj_node);
        }

        if (degrees[u] >= K() && freezeWorklist.Contain(u)) {
            freezeWorklist.DeleteNode(u);
            spillWorklist.Append(u);
        }
    }

    void Color::Build() {
        // 初始化
        const auto &org_move_list = liveness_.moves->GetList();

        // 构造moveList，初始化所有传送指令的源与目标节点对应的列表
        for (auto move : org_move_list) {
            auto src = move.first;
            auto dest = move.second;
            moveList[src].Append(src, dest);
            moveList[dest].Append(src, dest);
        }

        worklistMoves = *liveness_.moves; // 加入所有传送指令

        const auto &interf_node_list = liveness_.interf_graph->Nodes()->GetList();
        for (auto u : interf_node_list) {
            degrees[u] = 0;
        }
        for (auto u : interf_node_list) {
            const auto &adj_list = u->Adj()->GetList();
            for (auto v : adj_list) {
                AddEdge(u, v);
            }
        }

        // 预着色
        for (auto &it : precolored) {
            color_map.insert(std::make_pair(it.first, it.second));
        }
    }

    void Color::MakeWorklist() {
        const auto &interf_node_list = liveness_.interf_graph->Nodes()->GetList();
        for (auto node : interf_node_list) {
            // int d = node->Degree();
            // degrees.insert(std::make_pair(node, d)); // 保存度数

            temp::Temp *tmp = node->NodeInfo();
            if (isPrecolored(tmp)) {
                continue; // 跳过预着色
            }
            int d = degrees[node];
            if (d >= K()) {
                spillWorklist.Append(node); // 高度数
            } else if (MoveRelated(node)) {
                freezeWorklist.Append(node); // 低度数、传送相关
            } else {
                simplifyWorklist.Append(node); // 低度数，传送无关
            }
        }
    }

    void Color::Simplify() {
        auto node = simplifyWorklist.GetList().front(); // 选第一个
        simplifyWorklist.DeleteNode(node);

        // 加入到删除的栈中
        selectStack.Append(node);

        live::INodeList adj = Adjacent(node);

        // 如果直接操作图的话，就孤立这个节点
        // if (!m_useCache) {
        //     IsolateNode(node);
        // }

        // 把临界点的度数减1并更新其他列表状态
        std::list<live::INode *> adj_list = adj.GetList();
        for (auto adj_node : adj_list) {
            DecrementDegree(adj_node);
        }
    }

    void Color::Coalesce() {
        auto move = worklistMoves.GetList().front();
        auto x = move.first;  // src
        auto y = move.second; // dest

        live::INode *u, *v;
        if (isPrecolored(y->NodeInfo())) {
            u = GetAlias(y);
            v = GetAlias(x);
        } else {
            u = GetAlias(x);
            v = GetAlias(y);
        }
        worklistMoves.Delete(x, y);
        if (u == v) {
            // 如果源与目标一致，那么很简单地合并
            coalescedMoves.Append(x, y);
            AddWorkList(u);
        } else if (isPrecolored(v->NodeInfo()) ||
                   adjSet.find(std::make_pair(u, v)) != adjSet.end()) {
            constrainedMoves.Append(x, y); // 好像这句话没用
            // 如果源与目标本身就是冲突的，那么这个传送语句已经没有作为传送语句的意义了
            AddWorkList(u);
            AddWorkList(v);
        } else if (
            // 符合 George 条件，对v的任意邻居，要么已经和u冲突，要么是低度数，一般u已经是实寄存器
            (isPrecolored(u->NodeInfo()) && OK(Adjacent(v), u)) ||
            // 或者符合 Briggs 条件，uv合并后不会变成高度数节点，一般合并两个虚寄存器
            (!isPrecolored(u->NodeInfo()) && Conservative(Adjacent(u).Unioned(Adjacent(v))))) {
            coalescedMoves.Append(x, y);
            Combine(u, v);
            AddWorkList(u);
        } else {
            // 否则加入没准备好合并的集合，等日后再使用EnableMoves恢复到worklistMoves中
            activeMoves.Append(x, y);
        }
    }

    void Color::Freeze() {
        auto node = freezeWorklist.GetList().front(); // 随便找第一个节点冻结
        freezeWorklist.DeleteNode(node);
        simplifyWorklist.Append(node);
        FreezeMoves(node);
    }

    void Color::SelectSpill() {
        live::INode *m = nullptr;

        // 找度数最大的点溢出
        int max = 0;
        const auto &spill_list = spillWorklist.GetList();
        for (auto node : spill_list) {
            if (!m_ignoreAdded && added_temps.Contain(node->NodeInfo())) {
                continue;
            }
            int d = degrees[node];
            if (d > max) {
                max = d;
                m = node;
            }
        }
        // 无论如何要溢出一个
        if (!m) {
            m = spillWorklist.GetList().front();
        }
        spillWorklist.DeleteNode(m);
        simplifyWorklist.Append(m);
        FreezeMoves(m);
    }

    void Color::AssignColors() {
        while (!selectStack.isEmpty()) {
            auto node = selectStack.GetList().back();
            selectStack.DeleteNode(node);

            // 可以着的色
            std::set<std::string> colors;
            for (auto &it : precolored) {
                colors.insert(it.second);
            }

            // 去掉冲突节点的颜色
            auto neighbors_it = adjList.find(node);
            assert(neighbors_it != adjList.end());
            const auto &adj_list = neighbors_it->second.GetList();

            for (auto adj_node : adj_list) {
                auto root = GetAlias(adj_node);
                auto tmp = root->NodeInfo();
                if (coloredNodes.Contain(root) || isPrecolored(tmp)) {
                    auto it = color_map.find(tmp);
                    assert(it != color_map.end());

                    auto it2 = colors.find(it->second);
                    if (it2 != colors.end()) {
                        colors.erase(it2);
                    }
                }
            }
            if (colors.empty()) {
                // 如果没有颜色可以用了，那么它必须被溢出
                spilled_nodes.Append(node);
            } else {
                coloredNodes.Append(node);
                color_map.insert(std::make_pair(node->NodeInfo(), *colors.begin()));
            }
        }

        // 把合并的节点也加入map
        const auto &coalesced_list = coalescedNodes.GetList();
        for (auto node : coalesced_list) {
            auto root = GetAlias(node);
            auto it = color_map.find(root->NodeInfo());
            assert(it != color_map.end());
            color_map.insert(std::make_pair(node->NodeInfo(), it->second));
        }
    }

    void Color::AddEdge(live::INode *u, live::INode *v) {
        if (u == v) {
            return;
        }

        auto it = adjSet.find(std::make_pair(u, v));
        if (it == adjSet.end()) {
            // 成对添加
            adjSet.insert(std::make_pair(u, v));
            adjSet.insert(std::make_pair(v, u));

            // 不用给预着色寄存器添加度数，因为默认是无穷大
            if (!isPrecolored(u->NodeInfo())) {
                adjList[u].Append(v);
                degrees[u]++;
            }
            if (!isPrecolored(v->NodeInfo())) {
                adjList[v].Append(u);
                degrees[v]++;
            }
        }
    }

    void Color::IsolateNode(live::INode *node) const {
        auto preds = node->Pred()->GetList();
        auto succs = node->Succ()->GetList();
        for (auto pred : preds) {
            liveness_.interf_graph->RemoveEdge(pred, node);
        }
        for (auto succ : succs) {
            liveness_.interf_graph->RemoveEdge(node, succ);
        }
    }

} // namespace col
