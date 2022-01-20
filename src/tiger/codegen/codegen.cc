#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

    constexpr int maxlen = 1024;

} // namespace

namespace cg {

    void CodeGen::Codegen() {
        /* TODO: Put your lab5 code here */

        // 栈帧尺寸标签，此处与ProcEntryExit3设置的栈帧尺寸名保持一致
        fs_ = frame_->name_->Name() + "_framesize";

        assem::InstrList *instr_list = new assem::InstrList();
        temp::TempList *callee_saved_regs = reg_manager->CalleeSaves();

        std::list<std::pair<temp::Temp *, temp::Temp *>> stores;

        // 保存被调用者保护寄存器，存到虚寄存器中（%rbx ~ %r15）
        const std::string movq_assem("movq `s0, `d0");
        const auto &callee_saved_regs_list = callee_saved_regs->GetList();
        for (temp::Temp *reg : callee_saved_regs_list) {
            temp::Temp *storeReg = temp::TempFactory::NewTemp();
            instr_list->Append(new assem::MoveInstr(movq_assem,
                                                    // 目标：需要分配的虚寄存器
                                                    new temp::TempList({storeReg}),
                                                    // 源：被调用者保护寄存器
                                                    new temp::TempList({reg})));
            // 成对存储
            stores.emplace_back(reg, storeReg);
        }

        // 指令选择，对所有块进行指令选择
        const auto &trace_stm_list = traces_->GetStmList()->GetList();
        for (tree::Stm *stm : trace_stm_list) {
            stm->Munch(*instr_list, fs_);
        }

        // 恢复被调用者保护寄存器，倒着恢复
        for (auto it = stores.rbegin(); it != stores.rend(); ++it) {
            const auto &pair = *it;

            temp::Temp *reg = pair.first;
            temp::Temp *storeReg = pair.second;

            instr_list->Append(new assem::MoveInstr(movq_assem,
                                                    // 目标：被调用者保护寄存器
                                                    new temp::TempList({reg}),
                                                    // 源：刚才分配的对应的虚寄存器
                                                    new temp::TempList({storeReg})));
        }

        // 指定出口活跃，并转变为cg::AssemInstr
        assem_instr_ =
            std::make_unique<AssemInstr>(frame::FrameFactory::ProcEntryExit2(instr_list));
    }

    void AssemInstr::Print(FILE *out, temp::Map *map) const {
        for (auto instr : instr_list_->GetList())
            instr->Print(out, map);
        fprintf(out, "\n");
    }
} // namespace cg

namespace tree {
    /* TODO: Put your lab5 code here */

    void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */

        // 先对左边指令选择，再对右边指令选择
        this->left_->Munch(instr_list, fs);
        this->right_->Munch(instr_list, fs);
    }

    void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */

        instr_list.Append(
            // 新建标签
            new assem::LabelInstr(temp::LabelFactory::LabelString(this->label_), this->label_));
    }

    void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */

        auto jump_instr =                   // 跳转指令一般是一个标签
            new assem::OperInstr("jmp `j0", // 选择第零个地址
                                 nullptr, nullptr, new assem::Targets(this->jumps_));
        jump_instr->unconditional_jump = true; // 无条件跳转
        instr_list.Append(jump_instr);
    }

    void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */

        temp::Temp *left_temp = left_->Munch(instr_list, fs);
        temp::Temp *right_temp = right_->Munch(instr_list, fs);
        std::string op;
        switch (this->op_) {
        case tree::RelOp::EQ_OP:
            op = "je";
            break;
        case tree::RelOp::NE_OP:
            op = "jne";
            break;
        case tree::RelOp::LT_OP:
            op = "jl";
            break;
        case tree::RelOp::LE_OP:
            op = "jle";
            break;
        case tree::RelOp::GE_OP:
            op = "jge";
            break;
        case tree::RelOp::GT_OP:
            op = "jg";
            break;
        case tree::RelOp::ULT_OP:
            op = "jb";
            break;
        case tree::RelOp::ULE_OP:
            op = "jbe";
            break;
        case tree::RelOp::UGE_OP:
            op = "jae";
            break;
        case tree::RelOp::UGT_OP:
            op = "ja";
            break;
        default:
            assert(false);
            break;
        }

        // 条件跳转之前通常经过cmpq改变条件码
        instr_list.Append(new assem::OperInstr("cmpq `s1, `s0", // 两个都是源操作数，比较规范
                                               nullptr, new temp::TempList({left_temp, right_temp}),
                                               nullptr));
        // 然后执行jmp
        instr_list.Append(new assem::OperInstr(op + " `j0", // 选择第零个地址
                                               nullptr, nullptr,
                                               new assem::Targets(new std::vector<temp::Label *>(
                                                   {this->true_label_, this->false_label_}))));
    }

    void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */

        //由于x86_64的寻址模式比较多，复杂的实在难以实现（比如含比例因子的），所以只考虑了几种比较简单的

        if (dst_->type() == tree::Exp::Memory) {
            // 如果目标是内存，有以下几种方案
            auto mem_dst = static_cast<tree::MemExp *>(dst_);
            auto mem_dst_exp = mem_dst->exp_;

            // 模式 1：MOVE(MEM(BINOP(PLUS, e1, CONST(i)), e2)
            // 模式 2：MOVE(MEM(BINOP(PLUS, CONST(i), e1), e2)
            bool success = false;
            if (mem_dst_exp->type() == tree::Exp::BinOperation) {
                // 如果内存表达式是二元运算
                auto binop_exp = static_cast<tree::BinopExp *>(mem_dst_exp);
                if (binop_exp->op_ == tree::BinOp::PLUS_OP) {
                    // 如果目标表达式是加法
                    auto left_exp = binop_exp->left_;
                    auto right_exp = binop_exp->right_;

                    tree::Exp *e1 = nullptr;
                    int imm = -1;

                    if (right_exp->type() == tree::Exp::Constant) {
                        // 如果加法右边是常数，那么直接取出这个常数
                        auto const_exp = static_cast<tree::ConstExp *>(right_exp);
                        imm = const_exp->consti_;
                        e1 = binop_exp->left_;
                    } else if (left_exp->type() == tree::Exp::Constant) {
                        // 如果加法左边是常数，那么直接取出这个常数
                        auto const_exp = static_cast<tree::ConstExp *>(left_exp);
                        imm = const_exp->consti_;
                        e1 = binop_exp->right_;
                    }

                    if (e1) {
                        // 对源操作数与二元运算的其中一个操作数执行指令选择，并存在两个临时寄存器中
                        auto e1_reg = e1->Munch(instr_list, fs);
                        auto e2_reg = src_->Munch(instr_list, fs);

                        // movq e2, imm(e1)
                        instr_list.Append(new assem::OperInstr(
                            "movq `s0, " + std::to_string(imm) + "(`s1)", nullptr,
                            new temp::TempList({e2_reg, e1_reg}), nullptr));

                        success = true;
                    }
                }
            }

            // 如果内存表达式不是加法，或者不是二元运算，那就需要先用别的指令算出来，存到寄存器里再使用
            // 上述过程如果不适用，那么success将为false

            // 模式 3：MOVE(MEM(e1), MEM(e2))
            if (!success && src_->type() == tree::Exp::Memory) {
                // 如果源也是内存
                auto mem_src = static_cast<tree::MemExp *>(src_);
                auto mem_src_exp = mem_src->exp_;

                // 需要两条语句，因为x86_64规定不能两个操作数都是内存，所以需要一个中间寄存器
                temp::Temp *t = temp::TempFactory::NewTemp();

                auto e1_reg = mem_dst_exp->Munch(instr_list, fs);
                auto e2_reg = mem_src_exp->Munch(instr_list, fs);

                instr_list.Append(new assem::OperInstr("movq (`s0), `d0",
                                                       // 把源操作数转移到中间寄存器
                                                       new temp::TempList({t}),
                                                       new temp::TempList({e2_reg}), nullptr));

                instr_list.Append(new assem::OperInstr("movq `s0, (`s1)",
                                                       // 把中间寄存器转移到目标操作数
                                                       nullptr, new temp::TempList({t, e1_reg}),
                                                       nullptr));
                success = true;
            }

            // 模式 4：MOVE(MEM(e1), e2)  ---  摆烂
            if (!success) {
                auto e1_reg = mem_dst_exp->Munch(instr_list, fs);
                auto e2_reg = src_->Munch(instr_list, fs);

                instr_list.Append(new assem::OperInstr(
                    "movq `s0, (`s1)", nullptr, new temp::TempList({e2_reg, e1_reg}), nullptr));
            }

        } else if (dst_->type() == tree::Exp::Temp) {
            // 如果目标是寄存器
            auto tmp_dst = static_cast<tree::TempExp *>(dst_);

            temp::Temp *e2_reg = src_->Munch(instr_list, fs);

            // 模式 5：MOVE(TEMP(i), e2)
            instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                                   new temp::TempList({tmp_dst->temp_}),
                                                   new temp::TempList({e2_reg})));
        } else {
            // 目标操作数不可能是别的表达式
            assert(false);
        }
    }

    void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */
        this->exp_->Munch(instr_list, fs);
    }

    temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */

        temp::Temp *left = left_->Munch(instr_list, fs);
        temp::Temp *right = right_->Munch(instr_list, fs);

        // 结果寄存器
        temp::Temp *res = temp::TempFactory::NewTemp();

        // addq a,b  <=>  b <- a+b

        // 把左值复制到结果寄存器内，然后让结果寄存器去参与运算
        instr_list.Append(new assem::MoveInstr("movq `s0, `d0", new temp::TempList({res}),
                                               new temp::TempList({left})));

        switch (this->op_) {
        case tree::BinOp::PLUS_OP:
            // reg是隐藏的src，计算完了以后res里存的是加起来的和
            instr_list.Append(new assem::OperInstr("addq `s0, `d0", new temp::TempList({res}),
                                                   new temp::TempList({right, res}), nullptr));
            break;
        case tree::BinOp::MINUS_OP:
            // 计算完了以后res里存的是差
            instr_list.Append(new assem::OperInstr("subq `s0, `d0", new temp::TempList({res}),
                                                   new temp::TempList({right, res}), nullptr));
            break;

        case tree::BinOp::MUL_OP:
            // R[%rdx] : R[%rax] <- S × R[%rax] 全符号乘法
            instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                                   // 将左值移动到%rax里
                                                   new temp::TempList({reg_manager->ReturnValue()}),
                                                   new temp::TempList({res})));

            instr_list.Append(new assem::OperInstr(
                "imulq `s0",
                // 将右值作为imulq的参数，它会与%rax做乘法，并将结果存放在%rax和%rdx中
                reg_manager->OperateRegs(), new temp::TempList({right}), nullptr));

            instr_list.Append(
                // 最后把%rax移动到reg中
                new assem::MoveInstr("movq `s0, `d0", new temp::TempList({res}),
                                     new temp::TempList({reg_manager->ReturnValue()})));
            break;

        case tree::BinOp::DIV_OP:
            // R[%rax] <- R[%rax]  ÷  S 全符号除法
            // R[%rdx] <- R[%rax] mod S
            instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                                   // 将左值移动到%rax里
                                                   new temp::TempList({reg_manager->ReturnValue()}),
                                                   new temp::TempList({res})));

            instr_list.Append(
                new assem::OperInstr("cqto",
                                     // cqto读取%rax的符号位，并将它复制到%rdx的所有位，执行符号扩展
                                     reg_manager->OperateRegs(),
                                     new temp::TempList({reg_manager->ReturnValue()}), nullptr));

            instr_list.Append(new assem::OperInstr(
                "idivq `s0",
                // 将右值作为idivq的参数，它会与%rax做除法，并将商和余数存放在%rax和%rdx中
                reg_manager->OperateRegs(), new temp::TempList({right}), nullptr));

            instr_list.Append(new assem::MoveInstr(
                "movq `s0, `d0",
                // 最后把%rax移动到reg中
                new temp::TempList({res}), new temp::TempList({reg_manager->ReturnValue()})));

            break;
        default:
            // 不应该运行到这一步
            assert(false);
            break;
        }
        return res;
    }

    temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */

        temp::Temp *res = temp::TempFactory::NewTemp();
        temp::Temp *exp_reg = exp_->Munch(instr_list, fs);
        // 内存读到寄存器
        instr_list.Append(new assem::OperInstr("movq (`s0), `d0", new temp::TempList({res}),
                                               new temp::TempList(exp_reg), nullptr));

        return res;
    }

    temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */
        if (temp_ == reg_manager->FramePointer()) {
            // 把（栈指针 + 栈帧尺寸）当作返回值
            // "leaq xx_framesize(`s0), `d0"
            temp::Temp *res = temp::TempFactory::NewTemp();
            std::string str = "leaq " + std::string(fs.data()) + "(`s0), `d0";
            instr_list.Append(
                new assem::OperInstr(str, new temp::TempList({res}),
                                     new temp::TempList({reg_manager->StackPointer()}), nullptr));
            return res;
        }
        return this->temp_;
    }

    temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */
        stm_->Munch(instr_list, fs);
        return exp_->Munch(instr_list, fs);
    }

    temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */
        temp::Temp *res = temp::TempFactory::NewTemp();

        // 执行重定位时，由于是相对寻址，Label代表的是与PC的偏移量，因此直接获取Label的值需要加上%rip
        // "leaq name(%rip), `d0"
        std::string str = "leaq " + temp::LabelFactory::LabelString(this->name_) + "(%rip), `d0";
        instr_list.Append(new assem::OperInstr(str, new temp::TempList({res}), nullptr, nullptr));
        return res;
    }

    temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */
        temp::Temp *res = temp::TempFactory::NewTemp();
        // "movq $imm, `d0"
        std::string str = "movq $" + std::to_string(this->consti_) + ", `d0";
        instr_list.Append(new assem::OperInstr(str, new temp::TempList({res}), nullptr, nullptr));
        return res;
    }

    temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */
        temp::Temp *res = temp::TempFactory::NewTemp();

        // 计算参数并存到虚寄存器中
        temp::TempList *args = args_->MunchArgs(instr_list, fs);

        // CallExp的fun必须是个NameExp
        if (fun_->type() != tree::Exp::Name) {
            return nullptr;
        }
        auto fun_name = static_cast<tree::NameExp *>(fun_);

        // "callq fun_name"
        std::string str = "callq " + temp::LabelFactory::LabelString(fun_name->name_);

        // 将虚寄存器中的参数移动到参数寄存器中
        instr_list.Append(new assem::OperInstr(str, reg_manager->CallerSaves(), args, nullptr));

        // 返回值
        instr_list.Append(new assem::MoveInstr("movq `s0, `d0", new temp::TempList({res}),
                                               new temp::TempList({reg_manager->ReturnValue()})));

        return res;
    }

    temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view fs) {
        /* TODO: Put your lab5 code here */
        auto res = new temp::TempList();

        int ws = reg_manager->WordSize();
        const auto &arg_regs = reg_manager->ArgRegs()->GetList();
        auto it = arg_regs.begin();
        bool reg_over = false;

        int i = 0;

        const auto &exp_list = GetList();
        for (tree::Exp *exp : exp_list) {
            temp::Temp *arg_reg = exp->Munch(instr_list, fs); // 计算出参数
            if (!reg_over) {
                /*
                 * 通过寄存器传递的参数有6个
                 *
                 * rdi: a;  rsi: b;  rdx: c;  rcx: d;  r8: e;  r9: f;
                 *
                 */
                instr_list.Append(new assem::MoveInstr("movq `s0, `d0", new temp::TempList({*it}),
                                                       new temp::TempList({arg_reg})));
                res->Append(*it);

                it++;
                if (it == arg_regs.end()) {
                    reg_over = true;
                }
            } else {
                // 其他参数要放到栈上，位置：栈指针 + i * 字长
                std::string str = "movq `s0, " + std::to_string(i * ws) + "(`s1)";
                instr_list.Append(new assem::OperInstr(
                    str, nullptr, new temp::TempList({arg_reg, reg_manager->StackPointer()}),
                    nullptr));
                i++;
            }
        }
        return res;
    }

} // namespace tree
