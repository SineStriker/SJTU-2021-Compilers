#include "tiger/liveness/flowgraph.h"

namespace fg {

    void FlowGraphFactory::AssemFlowGraph() {
        /* TODO: Put your lab6 code here */

        // 生成控制流图
        const auto &instr_list = instr_list_->GetList();

        // 为所有语句生成对应的节点
        FNodePtr prev = nullptr;
        for (auto instr : instr_list) {
            FNodePtr node = flowgraph_->NewNode(instr);

            if (prev) {
                bool flag = true;
                auto prev_instr = prev->NodeInfo();
                if (prev_instr->type() == assem::Instr::Operator) {
                    auto op_instr = static_cast<assem::OperInstr *>(prev_instr);
                    if (op_instr->unconditional_jump) {
                        flag = false; // 无条件跳转语句与它下一条语句不可能顺序执行
                    }
                }
                if (flag) {
                    flowgraph_->AddEdge(prev, node);
                }
            }

            // 将所有标签指令加入map，产生一个标签与节点的对应关系
            if (instr->type() == assem::Instr::Label) {
                auto lb_instr = static_cast<assem::LabelInstr *>(instr);
                label_map_->Enter(lb_instr->label_, node);
            }

            // 保存上一条语句
            prev = node;
        }

        // 把所有跳转指令连接的两个语句的节点连起来
        const auto &node_list = flowgraph_->Nodes()->GetList();
        for (auto node : node_list) {
            auto instr = node->NodeInfo();
            if (instr->type() == assem::Instr::Operator) {
                auto op_instr = static_cast<assem::OperInstr *>(instr);
                // 如果当前指令有跳转的目标（x86_64中只可能是一个）
                if (op_instr->jumps_) {
                    auto labels_ptr = op_instr->jumps_->labels_;
                    if (labels_ptr) {
                        const auto &labels = *labels_ptr;
                        for (temp::Label *label : labels) {
                            // 跳转目标存在
                            FNodePtr target_node = label_map_->Look(label);
                            if (target_node) {
                                // 增加一条边
                                flowgraph_->AddEdge(node, target_node);
                            }
                        }
                    }
                }
            }
        }
    }

} // namespace fg

namespace assem {

    temp::TempList *OperInstr::Def() const {
        /* TODO: Put your lab6 code here */
        return dst_ ? dst_ : new temp::TempList(); // 定义的是目标寄存器
    }

    temp::TempList *OperInstr::Use() const {
        /* TODO: Put your lab6 code here */
        return src_ ? src_ : new temp::TempList(); // 使用的是源寄存器
    }

    temp::TempList *LabelInstr::Use() const {
        /* TODO: Put your lab6 code here */
        return new temp::TempList(); // 标签是不使用寄存器的，所以都返回空表
    }

    temp::TempList *LabelInstr::Def() const {
        /* TODO: Put your lab6 code here */
        return new temp::TempList();
    }

    temp::TempList *MoveInstr::Def() const {
        /* TODO: Put your lab6 code here */
        return dst_ ? dst_ : new temp::TempList(); // 定义的是目标寄存器
    }

    temp::TempList *MoveInstr::Use() const {
        /* TODO: Put your lab6 code here */
        return src_ ? src_ : new temp::TempList(); // 使用的是源寄存器
    }
} // namespace assem
