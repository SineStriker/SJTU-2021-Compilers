#include "tiger/liveness/liveness.h"

extern frame::RegManager *reg_manager;

namespace live {

    bool MoveList::Contain(INodePtr src, INodePtr dst) const {
        return std::any_of(move_list_.cbegin(), move_list_.cend(),
                           [src, dst](std::pair<INodePtr, INodePtr> move) {
                               return move.first == src && move.second == dst;
                           });
    }

    void MoveList::Delete(INodePtr src, INodePtr dst) {
        assert(src && dst);
        auto move_it = move_list_.begin();
        for (; move_it != move_list_.end(); move_it++) {
            if (move_it->first == src && move_it->second == dst) {
                break;
            }
        }
        move_list_.erase(move_it);
    }

    MoveList *MoveList::Union(MoveList *list) const {
        auto *res = new MoveList();
        for (auto move : move_list_) {
            res->move_list_.push_back(move);
        }
        for (auto move : list->GetList()) {
            if (!res->Contain(move.first, move.second))
                res->move_list_.push_back(move);
        }
        return res;
    }

    MoveList *MoveList::Intersect(MoveList *list) const {
        auto *res = new MoveList();
        for (auto move : list->GetList()) {
            if (Contain(move.first, move.second))
                res->move_list_.push_back(move);
        }
        return res;
    }

    // ----------------------------------------- 补充 -----------------------------------------

    bool MoveList::isEmpty() const {
        return move_list_.empty();
    }

    MoveList MoveList::Unioned(const MoveList &list) const {
        std::list<std::pair<INodePtr, INodePtr>> new_move_list;
        for (auto move : move_list_) {
            new_move_list.push_back(move);
        }
        for (auto move : list.move_list_) {
            if (!Contain(move.first, move.second)) {
                new_move_list.push_back(move);
            }
        }
        MoveList res;
        res.move_list_ = new_move_list;
        return res;
    }

    MoveList MoveList::Intersected(const MoveList &list) const {
        std::list<std::pair<INodePtr, INodePtr>> new_move_list;
        for (auto move : list.move_list_) {
            if (Contain(move.first, move.second)) {
                new_move_list.push_back(move);
            }
        }
        MoveList res;
        res.move_list_ = new_move_list;
        return res;
    }

    MoveList &MoveList::Union(const MoveList &list) {
        move_list_ = Unioned(list).move_list_;
        return *this;
    }

    MoveList &MoveList::Intersect(const MoveList &list) {
        move_list_ = Unioned(list).move_list_;
        return *this;
    }

    // ----------------------------------------------------------------------------------------

    void LiveGraphFactory::LiveMap() {
        /* TODO: Put your lab6 code here */

        // 初始化
        const auto &fnode_list = flowgraph_->Nodes()->GetList();
        for (auto node : fnode_list) {
            // 每个控制流节点都有一个入口活跃表与出口活跃表
            in_->Enter(node, new temp::TempList());
            out_->Enter(node, new temp::TempList());
        }

        // 活跃分析
        bool loop = true;
        while (loop) {
            loop = false;

            /*
             * 迭代公式
             * in[s] = use[s] ∪ (out[s] – def[s])
             * out[s] = ∪(n∈succ[s]) in[n]
             *
             */

            for (auto it = fnode_list.rbegin(); it != fnode_list.rend(); ++it) {
                auto node = *it;

                temp::TempList *org_in_nodes = in_->Look(node);   // 上一轮的入口活跃
                temp::TempList *org_out_nodes = out_->Look(node); // 上一轮的出口活跃

                assert(org_in_nodes);
                assert(org_out_nodes);

                // 更新out链表，遍历该节点的所有后继
                const auto &succ_list = node->Succ()->GetList();
                for (fg::FNodePtr succ : succ_list) {
                    auto in_nodes = in_->Look(succ); // 后继节点上一轮的入口活跃
                    auto out_nodes = out_->Look(node);

                    assert(in_nodes);
                    assert(out_nodes);

                    out_->Set(node, out_nodes->Union(in_nodes)); // 不停地求并
                }

                // 更新in链表
                {
                    auto out_nodes = out_->Look(node);
                    if (out_nodes) {
                        temp::TempList *uses = node->NodeInfo()->Use();
                        temp::TempList *diff = out_nodes->Diff(node->NodeInfo()->Def());
                        in_->Set(node, uses->Union(diff));
                    }
                }

                // 判断是否与上一轮迭代相同
                {
                    auto in_nodes = in_->Look(node);   // 本轮入口活跃
                    auto out_nodes = out_->Look(node); // 本轮出口活跃

                    assert(in_nodes);
                    assert(out_nodes);

                    // 如果有一个不相等，则继续迭代
                    if (!(in_nodes->Equal(org_in_nodes) && out_nodes->Equal(org_out_nodes))) {
                        loop = true;
                    }
                }
            }
        }
    }

    void LiveGraphFactory::InterfGraph() {
        /* TODO: Put your lab6 code here */

        auto &interf_graph = live_graph_.interf_graph;
        auto &moves = live_graph_.moves;

        interf_graph = new IGraph();
        moves = new MoveList();

        temp::TempList *registers = reg_manager->Registers(); // 跳过栈指针
        const auto &reg_list = registers->GetList();

        // 把实寄存器存入冲突图，预着色
        for (temp::Temp *reg : reg_list) {
            INodePtr node = interf_graph->NewNode(reg);
            temp_node_map_->Enter(reg, node);
        }

        // 实寄存器两两冲突，每两个都有边
        for (auto it = reg_list.begin(); it != reg_list.end(); ++it) {
            for (auto it2 = std::next(it); it2 != reg_list.end(); ++it2) {
                auto first_node = temp_node_map_->Look(*it);
                auto second_node = temp_node_map_->Look(*it2);

                assert(first_node);
                assert(second_node);

                // 无向图
                interf_graph->AddEdge(first_node, second_node);
                interf_graph->AddEdge(second_node, first_node);
            }
        }

        // 把控制流中所有的寄存器全部添加到冲突图中
        const auto &fnode_list = flowgraph_->Nodes()->GetList();
        for (fg::FNodePtr node : fnode_list) {
            auto instr = node->NodeInfo();
            const auto &use_list = instr->Use()->GetList();
            const auto &def_list = instr->Def()->GetList();

            // 把使用的加入
            for (auto t : use_list) {
                if (t == reg_manager->StackPointer()) {
                    continue; // 跳过栈指针
                }
                if (!temp_node_map_->Look(t)) {
                    INodePtr inode = interf_graph->NewNode(t);
                    temp_node_map_->Enter(t, inode);
                }
            }
            // 把赋值的加入
            for (auto t : def_list) {
                if (t == reg_manager->StackPointer()) {
                    continue; // 跳过栈指针
                }
                if (!temp_node_map_->Look(t)) {
                    INodePtr inode = interf_graph->NewNode(t);
                    temp_node_map_->Enter(t, inode);
                }
            }
        }

        /*
         * 添加冲突边规则
         *
         * 1、任何对变量a赋值的非传送指令，对该指令出口活跃的任意变量b[i]，添加冲突边(a, b[i])
         *
         * 2、对于传送指令a<-c，对该指令出口活跃的任意不同于c的变量b[i]，添加冲突边(a, b[i])
         * （传送指令需要保存起来）
         *
         */

        // 栈指针后面会经常用到
        temp::Temp *rsp = reg_manager->StackPointer();

        // 遍历所有的指令语句
        for (fg::FNodePtr node : fnode_list) {
            auto instr = node->NodeInfo();
            if (instr->type() != assem::Instr::Move) {
                // 处理非传送指令
                auto out_regs = out_->Look(node);
                assert(out_regs);

                const auto &def_reg_list = instr->Def()->GetList(); // 定值的寄存器（list)
                const auto &out_reg_list = out_regs->GetList(); // 出口活跃的寄存器（list)

                for (auto def_reg : def_reg_list) {
                    if (def_reg == rsp) {
                        continue; // 跳过栈指针
                    }
                    // 左值节点
                    auto def_node = temp_node_map_->Look(def_reg); // 左值寄存器对应的节点
                    assert(def_node);

                    for (auto out_reg : out_reg_list) {
                        if (out_reg == rsp) {
                            continue; // 跳过栈指针
                        }
                        // 出口活跃节点
                        auto out_node = temp_node_map_->Look(out_reg); // 出口活跃寄存器对应的节点
                        assert(out_node);

                        // 为左值节点与出口活跃节点添加冲突边
                        interf_graph->AddEdge(def_node, out_node);
                        interf_graph->AddEdge(out_node, def_node);
                    }
                }
            } else {
                // 处理传送指令
                auto out_regs = out_->Look(node);
                assert(out_regs);

                const auto &use_regs = instr->Use(); // 使用的寄存器
                const auto &def_regs = instr->Def(); // 定值的寄存器

                const auto &use_reg_list = use_regs->GetList(); // 使用的寄存器（list)
                const auto &def_reg_list = def_regs->GetList(); // 定值的寄存器（list)
                const auto &out_reg_list = out_regs->GetList(); // 出口活跃的寄存器(list)

                // 出口活跃寄存器减去使用的寄存器
                temp::TempList *out_sub_use_regs = out_regs->Diff(use_regs);
                const auto &out_sub_use_reg_list = out_sub_use_regs->GetList();

                for (auto def_reg : def_reg_list) {
                    if (def_reg == rsp) {
                        continue; // 跳过栈指针
                    }
                    // 左值节点
                    auto def_node = temp_node_map_->Look(def_reg);
                    assert(def_node);

                    for (auto out_sub_use_reg : out_sub_use_reg_list) {
                        if (out_sub_use_reg == rsp) {
                            continue; // 跳过栈指针
                        }
                        // 出口活跃节点
                        auto out_node = temp_node_map_->Look(out_sub_use_reg);
                        assert(out_node);

                        // 为左值节点与出口活跃节点添加冲突边
                        interf_graph->AddEdge(def_node, out_node);
                        interf_graph->AddEdge(out_node, def_node);
                    }

                    // 保存传送语句
                    for (auto use_reg : use_reg_list) {
                        if (use_reg == rsp) {
                            continue; // 跳过栈指针
                        }
                        // 使用节点
                        auto use_node = temp_node_map_->Look(use_reg);
                        assert(use_node);

                        // 有序成对添加到列表
                        if (!moves->Contain(use_node, def_node)) {
                            moves->Append(use_node, def_node); // src, dst
                        }
                    }
                }
            }
            // 一次遍历结束
        }
        // 冲突图构造结束
    }

    void LiveGraphFactory::Liveness() {
        LiveMap();
        InterfGraph();
    }

} // namespace live
