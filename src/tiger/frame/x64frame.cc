#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {

    /* TODO: Put your lab5 code here */

    X64RegManager::X64RegManager() {
        // 寄存器名
        std::vector<std::string *> names;

        names.push_back(new std::string("%rax"));
        names.push_back(new std::string("%rbx"));
        names.push_back(new std::string("%rcx"));
        names.push_back(new std::string("%rdx"));
        names.push_back(new std::string("%rsi"));
        names.push_back(new std::string("%rdi"));
        names.push_back(new std::string("%rbp"));
        names.push_back(new std::string("%rsp"));
        names.push_back(new std::string("%r8"));
        names.push_back(new std::string("%r9"));
        names.push_back(new std::string("%r10"));
        names.push_back(new std::string("%r11"));
        names.push_back(new std::string("%r12"));
        names.push_back(new std::string("%r13"));
        names.push_back(new std::string("%r14"));
        names.push_back(new std::string("%r15"));

        // 生成 x86_64 的16个寄存器
        for (auto &name : names) {
            auto reg = temp::TempFactory::NewTemp();
            regs_.push_back(reg);
            temp_map_->Enter(reg, name); // 加入寄存器map
        }
    }

    /**
     * Get general-purpose registers except RSI
     * NOTE: returned temp list should be in the order of calling convention
     * @return general-purpose registers
     */
    temp::TempList *X64RegManager::Registers() {
        auto temp_list = new temp::TempList();
        for (size_t i = 0; i < 16; i++) {
            // 跳过 %rsp
            if (i == 7) {
                continue;
            }
            temp_list->Append(GetRegister(i));
        }
        return temp_list;
    }

    /**
     * Get registers which can be used to hold arguments
     * NOTE: returned temp list must be in the order of calling convention
     * @return argument registers
     */
    temp::TempList *X64RegManager::ArgRegs() {
        // 参数寄存器，6个
        auto temp_list = new temp::TempList();
        temp_list->Append(GetRegister(5)); //%rdi
        temp_list->Append(GetRegister(4)); //%rsi
        temp_list->Append(GetRegister(3)); //%rdx
        temp_list->Append(GetRegister(2)); //%rcx
        temp_list->Append(GetRegister(8)); //%r8
        temp_list->Append(GetRegister(9)); //%r9
        return temp_list;
    }

    /**
     * Get caller-saved registers
     * NOTE: returned registers must be in the order of calling convention
     * @return caller-saved registers
     */
    temp::TempList *X64RegManager::CallerSaves() {
        // 调用者保存寄存器
        auto temp_list = new temp::TempList();
        temp_list->Append(GetRegister(0));  //%rax
        temp_list->Append(GetRegister(2));  //%rcx
        temp_list->Append(GetRegister(3));  //%rdx
        temp_list->Append(GetRegister(4));  //%rsi
        temp_list->Append(GetRegister(5));  //%rdi
        temp_list->Append(GetRegister(8));  //%r8
        temp_list->Append(GetRegister(9));  //%r9
        temp_list->Append(GetRegister(10)); //%r10
        temp_list->Append(GetRegister(11)); //%r11
        return temp_list;
    }

    /**
     * Get callee-saved registers
     * NOTE: returned registers must be in the order of calling convention
     * @return callee-saved registers
     */
    temp::TempList *X64RegManager::CalleeSaves() {
        // 被调用者保存寄存器
        auto temp_list = new temp::TempList();
        temp_list->Append(GetRegister(1));  //%rbx
        temp_list->Append(GetRegister(6));  //%rbp
        temp_list->Append(GetRegister(12)); //%r12
        temp_list->Append(GetRegister(13)); //%r13
        temp_list->Append(GetRegister(14)); //%r14
        temp_list->Append(GetRegister(15)); //%r15
        return temp_list;
    }

    /**
     * Get return-sink registers
     * @return return-sink registers
     */
    temp::TempList *X64RegManager::ReturnSink() {
        // 出口活跃寄存器
        temp::TempList *temp_list = CalleeSaves();
        temp_list->Append(StackPointer()); //%rsp
        temp_list->Append(ReturnValue());  //%rax
        return temp_list;
    }

    /**
     * Get word size
     */
    int X64RegManager::WordSize() {
        return 8;
    }

    temp::Temp *X64RegManager::FramePointer() {
        return GetRegister(6); //%rbp
    }

    temp::Temp *X64RegManager::StackPointer() {
        return GetRegister(7); //%rsp
    }

    temp::Temp *X64RegManager::ReturnValue() {
        return GetRegister(0); //%rax
    }

    temp::TempList *X64RegManager::OperateRegs() {
        auto temp_list = new temp::TempList();
        temp_list->Append(GetRegister(3)); //%rdx
        temp_list->Append(GetRegister(0)); //%rax
        return temp_list;
    }

    int X64RegManager::ColorNum() const {
        return 15;
    }

    /* TODO: Put your lab5 code here */

    void X64Frame::setFormals(const std::list<bool> &escapes) {
        for (bool escape : escapes) {
            formals_.push_back(AllocLocal(escape)); // 根据逃逸与否为每个参数开辟空间
        }

        tree::Exp *src, *dst;
        tree::Stm *stm;

        // 参数寄存器
        const auto &reg_list = reg_manager->ArgRegs()->GetList(); // 这个 list 的长度是 6
        auto it = reg_list.begin();
        bool reg_over = false;

        int i = 0;
        const int ws = reg_manager->WordSize();

        // 执行view shift，把所有参数都转移到该函数的虚寄存器中
        for (frame::Access *access : formals_) {
            // 目标：虚寄存器或栈帧
            dst = access->toExp(new tree::TempExp(reg_manager->FramePointer()));

            // 源需要根据参数传递的情况进行判断，假设参数是a，b，c，d，e，f，g，h...
            if (!reg_over) {
                /*
                 * 源：参数寄存器
                 *
                 * 通过寄存器传递的参数有6个
                 *
                 * rdi: a;  rsi: b;  rdx: c;  rcx: d;  r8: e;  r9: f;
                 *
                 */
                src = new tree::TempExp(*it);

                it++;
                if (it == reg_list.end()) {
                    reg_over = true;
                }
            } else {
                /*
                 * 源：通过栈传递的参数
                 *
                 * 超过6个参数，将使用栈传递
                 *
                 * |     ...     | 更多参数
                 * |      h      | 第八个参数
                 * |      g      | 第七个参数
                 * | return addr |
                 * |  saved rbp  | <- rbp (frame pointer)
                 * |  ---------  |
                 * |  saved reg  |
                 * |  local var  |
                 * |     xxx     |
                 * |     yyy     |
                 * |     zzz     | <- rsp (stack pointer)
                 *    ---------
                 * |     ???     | 低地址
                 * |  red  zone  | 更低的地址
                 *
                 */

                // 执行call的时候会自动把返回地址压入栈中，然后才是帧指针，因此获取栈上的参数需要多加一个字长

                src = new tree::MemExp( // 内存操作
                                        // 加法
                    new tree::BinopExp(tree::BinOp::PLUS_OP,
                                       // 帧指针 + 1个字长（跳过返回地址） + i个字长
                                       new tree::TempExp(reg_manager->FramePointer()),
                                       new tree::ConstExp((i + 1) * ws)));
                i++;
            }

            stm = new tree::MoveStm(dst, src);

            // view shift 中的语句负责把参数转移到自己的Access中
            view_shift_ = view_shift_ ? new tree::SeqStm(view_shift_, stm) : stm;
        }
    }

    frame::Access *X64Frame::AllocLocal(bool escape) {
        frame::Access *access;
        if (escape) {
            offset -= reg_manager->WordSize(); // 如果是逃逸的，那么栈帧向低地址方向移动一个字长
            access = new InFrameAccess(offset);
        } else {
            access = new InRegAccess(temp::TempFactory::NewTemp()); // 否则直接使用寄存器
        }
        return access;
    }

    Frame *FrameFactory::NewFrame(temp::Label *label, const std::list<bool> &formals) {
        auto f = new X64Frame(label);
        f->setFormals(formals); // 将构造函数与申请内存分开，避免在构造函数内使用虚函数
        return f;
    }

    tree::Stm *FrameFactory::ProcEntryExit1(Frame *f, tree::Stm *stm) {
        // 函数入口保存调用者保护寄存器，并执行一些初始化语句
        return new tree::SeqStm(f->view_shift_, stm);
    }

    assem::InstrList *FrameFactory::ProcEntryExit2(assem::InstrList *body) {
        // 在函数体末尾添加下沉指令，表示某些寄存器是出口活跃的，通常是返回地址和被调用者保护寄存器
        body->Append(
            new assem::OperInstr("", new temp::TempList(), reg_manager->ReturnSink(), nullptr));
        return body;
    }

    assem::Proc *FrameFactory::ProcEntryExit3(frame::Frame *f, assem::InstrList *body) {
        int argRegs = reg_manager->ArgRegs()->GetList().size();
        int ws = reg_manager->WordSize();

        // 传统的做法：
        // call 的时候先把返回地址压入栈中
        // 然后把 %rbp 压入栈中
        // 再把 %rsp 赋值给 %rbp，这样 %rbp 指向的对象就是保存的旧值
        // 再将调用者保护寄存器压入栈中
        // ...

        // 现在的做法：
        // call 的时候保存一个栈帧大小，假设为k
        // 那么 %rsp + k 就是帧指针，即 FP = SP + k
        //

        // 栈帧尺寸伪指令 ".set xx_framesize, size"
        std::string prolog =
            ".set " + f->name_->Name() + "_framesize, " + std::to_string(-(f->offset)) + "\n";

        // 当前过程标签 "name:"
        prolog += f->name_->Name() + ":\n";

        // 当参数超过6个时需要额外的栈空间存这些参数
        int size_for_args = std::max(f->maxArgs - argRegs, 0) * ws;
        // "subq $size, %rsp"
        prolog += "subq $" + std::to_string(size_for_args - f->offset) + ", %rsp\n";

        // 出口恢复栈指针
        //"addq $size, %rsp"
        std::string epilog = "addq $" + std::to_string(size_for_args - f->offset) + ", %rsp\n";

        // 返回指令
        epilog += "retq\n";
        return new assem::Proc(prolog, body, epilog);
    }

    tree::Exp *FrameFactory::ExternalCall(const std::string &name, tree::ExpList *args) {
        // 调用库函数
        return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(name)), args);
    }

} // namespace frame