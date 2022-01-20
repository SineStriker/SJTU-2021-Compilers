#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {
    /* TODO: Put your lab6 code here */

    void RegAllocator::RegAlloc() {
        col::Result color_result;
        std::list<temp::Temp *> added_temps;
        while (true) {
            live::LiveGraph liveness = RegAllocFactory::AnalyzeLiveness(instr_list_);
            // 染色
            col::Color painter(liveness, added_temps);
            painter.setIgnoreAdded(true); // 设置新增节点不被溢出
            painter.Paint();
            color_result = painter.GetResult();

            //            col::Color painter(liveness, new temp::TempList({added_temps}));
            //            color_result = painter.doColoring();

            // 判断是否溢出
            if (!color_result.spills || color_result.spills->isEmpty()) {
                // 没有溢出节点就结束
                break;
            } else {
                // 重写程序
                auto pair =
                    RegAllocFactory::RewriteProgram(frame_, instr_list_, color_result.spills);
                added_temps = pair.first;
                instr_list_ = pair.second;
            }
        }

        auto new_instr_list = RegAllocFactory::RemoveMoveInstr(instr_list_, *color_result.coloring);
        result_->coloring_ = color_result.coloring;
        result_->il_ = new_instr_list;
    }

    live::LiveGraph RegAllocFactory::AnalyzeLiveness(assem::InstrList *instr_list) {
        fg::FlowGraphFactory flow_graph_fac(instr_list);
        flow_graph_fac.AssemFlowGraph(); // 构造控制流图

        fg::FGraphPtr flow_graph = flow_graph_fac.GetFlowGraph();

        live::LiveGraphFactory live_graph_fac(flow_graph);
        live_graph_fac.Liveness(); // 构造活跃性图（冲突图）

        return live_graph_fac.GetLiveGraph();
    }

    std::pair<std::list<temp::Temp *>, assem::InstrList *>
        RegAllocFactory::RewriteProgram(frame::Frame *frame,                // 栈帧
                                        const assem::InstrList *instrList,  // 旧汇编指令序列
                                        const live::INodeList *spilledNodes // 溢出的结点
                                        ) // 返回产生的新寄存器和新汇编指令序列
    {
        std::list<temp::Temp *> newTempList;
        auto newInstrList = new assem::InstrList();

        int ws = reg_manager->WordSize();
        std::string label = frame->GetLabel();
        temp::Temp *rsp = reg_manager->StackPointer();

        const auto &spilled_node_list = spilledNodes->GetList();
        const auto &old_instr_list = instrList->GetList();

        // 用来存放上一次更新的汇编指令序列，第一次它就是旧指令序列
        std::list<assem::Instr *> prev_instr_list;
        for (auto instr : old_instr_list) {
            prev_instr_list.push_back(instr);
        }

        // 遍历所有溢出的结点
        for (auto node : spilled_node_list) {
            auto spilledTemp = node->NodeInfo();
            frame->offset -= ws; // 在帧中开辟新的空间
            std::list<assem::Instr *> cur_instr_list;
            for (auto instr : prev_instr_list) {
                auto use_regs = instr->Use(); // 源
                auto def_regs = instr->Def(); // 目标

                // 如果溢出的寄存器被这条语句使用了，那么需要再其之前插入一条取数指令
                if (use_regs->Contain(spilledTemp)) {
                    auto newTemp = temp::TempFactory::NewTemp();

                    // movq (fun_framesize-offset)(src), dst
                    std::string assem = "movq (" + label + "_framesize-" +
                                        std::to_string(std::abs(frame->offset)) + ")(`s0), `d0";

                    // 源：新临时寄存器
                    // 目标：帧指针（通过栈指针计算出）-偏移
                    auto pre_instr = new assem::OperInstr(assem, new temp::TempList({newTemp}),
                                                          new temp::TempList({rsp}), nullptr);
                    cur_instr_list.push_back(pre_instr);

                    // 替换原指令成员变量src_保存的溢出节点
                    use_regs->replaceTemp(spilledTemp, newTemp);
                    // 加入列表
                    newTempList.push_back(newTemp);
                }
                cur_instr_list.push_back(instr);
                if (def_regs->Contain(spilledTemp)) {
                    auto newTemp = temp::TempFactory::NewTemp();

                    // movq src, (fun_framesize-offset)(dst)
                    std::string assem = "movq `s0, (" + label + "_framesize-" +
                                        std::to_string(std::abs(frame->offset)) + ")(`d0)";

                    // 源：帧指针（通过栈指针计算出）-偏移
                    // 目标：新临时寄存器
                    assem::Instr *back_instr = new assem::OperInstr(
                        assem, new temp::TempList({rsp}), new temp::TempList({newTemp}), nullptr);

                    cur_instr_list.push_back(back_instr);

                    // 替换原指令成员变量dst_保存的溢出节点
                    def_regs->replaceTemp(spilledTemp, newTemp);
                    // 加入列表
                    newTempList.push_back(newTemp);
                }
            }

            // 保存这一轮的指令列表
            prev_instr_list = cur_instr_list;
        }

        for (auto instr : prev_instr_list) {
            newInstrList->Append(instr);
        }

        return std::make_pair(newTempList, newInstrList);
    }

    assem::InstrList *RegAllocFactory::RemoveMoveInstr(assem::InstrList *instrList,
                                                       temp::Map &colorMap) {
        auto res = new assem::InstrList();
        const auto &list = instrList->GetList();
        for (auto instr : list) {
            if (instr->type() == assem::Instr::Move) {
                temp::Temp *src = *instr->Use()->GetList().begin();
                temp::Temp *dst = *instr->Def()->GetList().begin();
                // 忽略源与目标相同的指令
                auto src_ptr = colorMap.Look(src);
                auto dst_ptr = colorMap.Look(dst);
                if (src_ptr == dst_ptr || (src_ptr && dst_ptr && (*src_ptr == *dst_ptr))) {
                    continue;
                }
            }
            res->Append(instr);
        }
        return res;
    }

} // namespace ra