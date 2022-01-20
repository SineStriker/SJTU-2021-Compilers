//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
    class X64RegManager : public RegManager {
        /* TODO: Put your lab5 code here */
    public:
        X64RegManager();

        /**
         * Get general-purpose registers except RSI
         * NOTE: returned temp list should be in the order of calling convention
         * @return general-purpose registers
         */
        [[nodiscard]] temp::TempList *Registers() override;

        /**
         * Get registers which can be used to hold arguments
         * NOTE: returned temp list must be in the order of calling convention
         * @return argument registers
         */
        [[nodiscard]] temp::TempList *ArgRegs() override;

        /**
         * Get caller-saved registers
         * NOTE: returned registers must be in the order of calling convention
         * @return caller-saved registers
         */
        [[nodiscard]] temp::TempList *CallerSaves() override;

        /**
         * Get callee-saved registers
         * NOTE: returned registers must be in the order of calling convention
         * @return callee-saved registers
         */
        [[nodiscard]] temp::TempList *CalleeSaves() override;

        /**
         * Get return-sink registers
         * @return return-sink registers
         */
        [[nodiscard]] temp::TempList *ReturnSink() override;

        /**
         * Get word size
         */
        [[nodiscard]] int WordSize() override;

        [[nodiscard]] temp::Temp *FramePointer() override;

        [[nodiscard]] temp::Temp *StackPointer() override;

        [[nodiscard]] temp::Temp *ReturnValue() override;

        [[nodiscard]] temp::TempList *OperateRegs() override;

        [[nodiscard]] int ColorNum() const override;
    };

    class X64Frame : public Frame {
        /* TODO: Put your lab5 code here */
    public:
        explicit X64Frame(temp::Label *name) : Frame(name) {
        }

        void setFormals(const std::list<bool> &escapes) override;

        frame::Access *AllocLocal(bool escape) override;
    };

    class InFrameAccess : public Access {
    public:
        int offset;

        explicit InFrameAccess(int offset) : offset(offset) {
        }
        /* TODO: Put your lab5 code here */

        tree::Exp *toExp(tree::Exp *fp = nullptr) const override {
            auto *binopExp = new tree::BinopExp(tree::BinOp::PLUS_OP, fp,
                                                new tree::ConstExp(offset)); // 帧指针加上偏移
            return new tree::MemExp(binopExp);                               // 从内存中取
        }
    };

    class InRegAccess : public Access {
    public:
        temp::Temp *reg;

        explicit InRegAccess(temp::Temp *reg) : reg(reg) {
        }
        /* TODO: Put your lab5 code here */

        tree::Exp *toExp(tree::Exp *fp = nullptr) const override {
            return new tree::TempExp(reg); // 直接返回Temp表达式
        }
    };

    class FrameFactory {
    public:
        static Frame *NewFrame(temp::Label *label, const std::list<bool> &formals);

        static tree::Stm *ProcEntryExit1(Frame *f, tree::Stm *stm);
        static assem::InstrList *ProcEntryExit2(assem::InstrList *body);
        static assem::Proc *ProcEntryExit3(frame::Frame *f, assem::InstrList *body);

        static tree::Exp *ExternalCall(const std::string &name, tree::ExpList *args);
    };

} // namespace frame

#endif // TIGER_COMPILER_X64FRAME_H
