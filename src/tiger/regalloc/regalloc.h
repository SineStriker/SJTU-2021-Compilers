#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {

    class Result {
    public:
        temp::Map *coloring_;
        assem::InstrList *il_;

        Result() : coloring_(nullptr), il_(nullptr) {
        }
        Result(temp::Map *coloring, assem::InstrList *il) : coloring_(coloring), il_(il) {
        }
        Result(const Result &result) = delete;
        Result(Result &&result) = delete;
        Result &operator=(const Result &result) = delete;
        Result &operator=(Result &&result) = delete;
        ~Result() = default;
    };

    class RegAllocator {
        /* TODO: Put your lab6 code here */

    public:
        RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> traces)
            : frame_(frame), instr_list_(traces->GetInstrList()),
              result_(std::make_unique<ra::Result>()) {
            // 初始化，result_的内容等待寄存器分配后赋值
        }

        void RegAlloc();

        std::unique_ptr<ra::Result> TransferResult() {
            return std::move(result_);
        };

    protected:
        frame::Frame *frame_;                // 当前函数或过程的栈帧
        assem::InstrList *instr_list_;       // 汇编指令列表
        std::unique_ptr<ra::Result> result_; // 虚寄存器（Temp）与实际寄存器的对应关系
    };

    class RegAllocFactory {
    public:
        // 对汇编指令列表生成活跃分析图，返回两个指针的结构体
        static live::LiveGraph AnalyzeLiveness(assem::InstrList *instr_list);

        static std::pair<std::list<temp::Temp *>, assem::InstrList *>
            RewriteProgram(frame::Frame *frame, const assem::InstrList *instrList,
                           const live::INodeList *spilledNodes);

        // 删掉汇编指令列表中无意义的传送指令
        static assem::InstrList *RemoveMoveInstr(assem::InstrList *instrList, temp::Map &colorMap);
    };

} // namespace ra

#endif