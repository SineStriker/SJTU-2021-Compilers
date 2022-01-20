#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <list>
#include <memory>
#include <string>

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/translate/tree.h"

namespace frame {

    class RegManager {
    public:
        RegManager() : temp_map_(temp::Map::Empty()) {
        }

        temp::Temp *GetRegister(int regno) {
            return regs_[regno];
        }

        /**
         * Get general-purpose registers except RSI
         * NOTE: returned temp list should be in the order of calling convention
         * @return general-purpose registers
         */
        [[nodiscard]] virtual temp::TempList *Registers() = 0;

        /**
         * Get registers which can be used to hold arguments
         * NOTE: returned temp list must be in the order of calling convention
         * @return argument registers
         */
        [[nodiscard]] virtual temp::TempList *ArgRegs() = 0;

        /**
         * Get caller-saved registers
         * NOTE: returned registers must be in the order of calling convention
         * @return caller-saved registers
         */
        [[nodiscard]] virtual temp::TempList *CallerSaves() = 0;

        /**
         * Get callee-saved registers
         * NOTE: returned registers must be in the order of calling convention
         * @return callee-saved registers
         */
        [[nodiscard]] virtual temp::TempList *CalleeSaves() = 0;

        /**
         * Get return-sink registers
         * @return return-sink registers
         */
        [[nodiscard]] virtual temp::TempList *ReturnSink() = 0;

        /**
         * Get word size
         */
        [[nodiscard]] virtual int WordSize() = 0;

        [[nodiscard]] virtual temp::Temp *FramePointer() = 0;

        [[nodiscard]] virtual temp::Temp *StackPointer() = 0;

        [[nodiscard]] virtual temp::Temp *ReturnValue() = 0;

        // for * /
        [[nodiscard]] virtual temp::TempList *OperateRegs() = 0;

        // The number of colors to use
        [[nodiscard]] virtual int ColorNum() const = 0;

        temp::Map *temp_map_;

    protected:
        std::vector<temp::Temp *> regs_;
    };

    class Access {
    public:
        /* TODO: Put your lab5 code here */
        virtual tree::Exp *toExp(tree::Exp *fp = nullptr) const = 0;

        virtual ~Access() = default;
    };

    class Frame {
        /* TODO: Put your lab5 code here */
    public:
        temp::Label *name_;
        std::list<Access *> formals_;

        int offset;
        int maxArgs; // 当前层作为caller调用其他函数时传递的参数的最大数量，用于生成该栈帧时分配足够多的空间
        tree::Stm *view_shift_; // 在分配栈帧时执行view shift的语句

        explicit Frame(temp::Label *name)
            : name_(name), offset(0), maxArgs(0), view_shift_(nullptr) {
        }

        virtual void setFormals(const std::list<bool> &escapes) {
        }

        virtual frame::Access *AllocLocal(bool escape) {
            return nullptr;
        }

        [[nodiscard]] std::string GetLabel() const {
            return temp::LabelFactory::LabelString(name_);
        }
    };

    /**
     * Fragments
     */

    class Frag {
    public:
        virtual ~Frag() = default;

        enum OutputPhase {
            Proc,
            String,
        };

        /**
         *Generate assembly for main program
         * @param out FILE object for output assembly file
         */
        virtual void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const = 0;
    };

    class StringFrag : public Frag {
    public:
        temp::Label *label_;
        std::string str_;

        StringFrag(temp::Label *label, std::string str) : label_(label), str_(std::move(str)) {
        }

        void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
    };

    class ProcFrag : public Frag {
    public:
        tree::Stm *body_;
        Frame *frame_;

        ProcFrag(tree::Stm *body, Frame *frame) : body_(body), frame_(frame) {
        }

        void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
    };

    class Frags {
    public:
        Frags() = default;
        void PushBack(Frag *frag) {
            frags_.emplace_back(frag);
        }
        const std::list<Frag *> &GetList() {
            return frags_;
        }

    private:
        std::list<Frag *> frags_;
    };

    /* TODO: Put your lab5 code here */

} // namespace frame

#endif