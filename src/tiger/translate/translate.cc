#include "tiger/translate/translate.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

    Access *Access::AllocLocal(Level *level, bool escape) {
        /* TODO: Put your lab5 code here */
        return new tr::Access(level, level->frame_->AllocLocal(escape));
    }

    class Cx {
    public:
        temp::Label **trues_;
        temp::Label **falses_;
        tree::Stm *stm_;

        Cx(temp::Label **trues, temp::Label **falses, tree::Stm *stm)
            : trues_(trues), falses_(falses), stm_(stm) {
        }
    };

    class Exp {
    public:
        [[nodiscard]] virtual tree::Exp *UnEx() const = 0;
        [[nodiscard]] virtual tree::Stm *UnNx() const = 0;
        [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) const = 0;
    };

    class ExpAndTy {
    public:
        tr::Exp *exp_;
        type::Ty *ty_;

        ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {
        }
    };

    class ExExp : public Exp {
    public:
        tree::Exp *exp_;

        explicit ExExp(tree::Exp *exp) : exp_(exp) {
        }

        [[nodiscard]] tree::Exp *UnEx() const override {
            /* TODO: Put your lab5 code here */
            return exp_; // 直接返回exp
        }
        [[nodiscard]] tree::Stm *UnNx() const override {
            /* TODO: Put your lab5 code here */
            return new tree::ExpStm(exp_); // 把exp转为stm返回
        }
        [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
            /* TODO: Put your lab5 code here */
            temp::Label *t = temp::LabelFactory::NewLabel(); // CJump为真时要跳转的标签
            temp::Label *f = temp::LabelFactory::NewLabel(); // CJump为假时要跳转的标签

            // 如果exp不等于0，那么跳转到t，否则跳转到f
            tree::CjumpStm *stm =
                new tree::CjumpStm(tree::NE_OP, exp_, new tree::ConstExp(0), t, f);

            // 把构造完的stm的标签的二级指针传给Cx，并且这个t和f还未填入
            return tr::Cx(&(stm->true_label_), &(stm->false_label_), stm);
        }
    };

    class NxExp : public Exp {
    public:
        tree::Stm *stm_;

        explicit NxExp(tree::Stm *stm) : stm_(stm) {
        }

        [[nodiscard]] tree::Exp *UnEx() const override {
            /* TODO: Put your lab5 code here */
            return new tree::EseqExp(
                stm_, new tree::ConstExp(0)); // 返回一个表达式序列，执行stm以后的返回值随便选个0
        }
        [[nodiscard]] tree::Stm *UnNx() const override {
            /* TODO: Put your lab5 code here */
            return stm_; // 直接返回stm
        }
        [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
            /* TODO: Put your lab5 code here */
            assert(false); // 这个函数不应该执行
            return Cx(nullptr, nullptr, nullptr);
        }
    };

    class CxExp : public Exp {
    public:
        Cx cx_;

        CxExp(temp::Label **trues, temp::Label **falses, tree::Stm *stm) : cx_(trues, falses, stm) {
        }

        [[nodiscard]] tree::Exp *UnEx() const override {
            /* TODO: Put your lab5 code here */
            // 用来作为返回值的临时寄存器
            temp::Temp *r = temp::TempFactory::NewTemp();

            // CJump为要跳转的标签
            temp::Label *t = temp::LabelFactory::NewLabel();
            temp::Label *f = temp::LabelFactory::NewLabel();

            // 类似于执行Do Patch
            *(cx_.trues_) = t;
            *(cx_.falses_) = f;

            // cx_.trues_和cx_.falses_是两个二级指针，并且cx_.stm是一个CJump语句
            // 这两个二级指针分别指向这个CJump语句的t和f
            // 所以这个stm语句执行了就会跳转
            // 在后续构造的语句中只需要按需插入tree::LabelStm分配t和f即可

            return
                // 构造一个表达式序列
                new tree::EseqExp(
                    // 构造一个move语句，将1赋值给r
                    new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
                    // 构造一个表达式序列，move完了以后执行stm（一般是一个CJump语句）
                    new tree::EseqExp(
                        cx_.stm_,
                        // 执行完stm后，如果是f，跳转到f标签的位置也就是下一条，如果是t跳转到最下面t标签的位置
                        new tree::EseqExp(
                            // f标签的位置
                            new tree::LabelStm(f),
                            // f标签之后，那么将0赋值给r
                            new tree::EseqExp(
                                // move语句，将0赋值给r
                                new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(0)),
                                // 构造一个表达式序列，返回值是r
                                new tree::EseqExp(
                                    // t标签的位置，相当于跳过了将0赋值给r
                                    new tree::LabelStm(t), new tree::TempExp(r))))));
        }
        [[nodiscard]] tree::Stm *UnNx() const override {
            /* TODO: Put your lab5 code here */
            return new tree::ExpStm(UnEx()); // 先转换为Ex，再转为stm
        }
        [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
            /* TODO: Put your lab5 code here */
            return cx_; // 直接返回cx
        }
    };

    ProgTr::ProgTr(std::unique_ptr<absyn::AbsynTree> absyn_tree,
                   std::unique_ptr<err::ErrorMsg> errormsg)
        : absyn_tree_(std::move(absyn_tree)), errormsg_(std::move(errormsg)),
          tenv_(std::make_unique<env::TEnv>()), venv_(std::make_unique<env::VEnv>()) {

        // main函数的名字标签，即入口点
        temp::Label *main_label = temp::LabelFactory::NamedLabel("tigermain");

        // main函数栈帧
        frame::Frame *main_frame = frame::FrameFactory::NewFrame(main_label, {});

        // level是栈帧的代理，在翻译过程中表示一个层级关系
        main_level_ = std::make_unique<Level>(main_frame, nullptr);
    }

    void ProgTr::Translate() {
        /* TODO: Put your lab5 code here */

        FillBaseVEnv(); // 将运行时函数加入变量环境
        FillBaseTEnv(); // 将基本的变量int、string加入类型环境

        // main函数的标签
        temp::Label *label = temp::LabelFactory::NewLabel();
        tr::ExpAndTy root_info = absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level_.get(),
                                                        label, errormsg_.get());

        // 保存整个程序的片段
        frags->PushBack(new frame::ProcFrag(root_info.exp_->UnNx(), main_level_->frame_));
    }

    tree::Exp *TrFactory::StaticLink(tr::Level *current, tr::Level *target) {
        // 根据当前栈帧和目标栈帧推导计算的表达式
        tree::Exp *fp = new tree::TempExp(reg_manager->FramePointer());
        while (current != target) {
            // 如果没有父层级，那说明出错了
            if (!current || !current->parent_) {
                return nullptr;
            }
            // 静态链应当是栈帧的第一个参数
            auto access = current->frame_->formals_.front();

            // 得到一个内存表达式，其运算结果是上一个帧指针
            fp = access->toExp(fp);

            // 重复迭代（target的层级-current的层级）次，fp的值就是目标层级的帧指针
            current = current->parent_;
        }
        return fp;
    }

    tr::Exp *TrFactory::SimpleVar(tr::Access *access, tr::Level *level, err::ErrorMsg *errorMsg) {
        // 先计算出静态链，再通过此帧指针计算access
        tree::Exp *fp = StaticLink(level, access->level_);
        // MEM(BINOP(PLUS, TEMP fp, CONST offset));
        return new tr::ExExp(access->access_->toExp(fp));
    }

    tr::Exp *TrFactory::FieldVar(tr::Exp *exp, int offset, err::ErrorMsg *errorMsg) {
        return new tr::ExExp(new tree::MemExp(
            new tree::BinopExp(tree::BinOp::PLUS_OP,
                               // 记录对象的指针（首地址） + 偏移 × 字长
                               exp->UnEx(), new tree::ConstExp(offset * reg_manager->WordSize()))));
    }

    tr::Exp *TrFactory::SubscriptVar(tr::Exp *exp, tr::Exp *index, err::ErrorMsg *errorMsg) {
        return new tr::ExExp(new tree::MemExp(
            new tree::BinopExp(tree::BinOp::PLUS_OP, exp->UnEx(),
                               // 数组对象的指针（首地址） + index × 字长
                               new tree::BinopExp(tree::BinOp::MUL_OP, index->UnEx(),
                                                  new tree::ConstExp(reg_manager->WordSize())))));
    }

    tr::Exp *TrFactory::NilExp(err::ErrorMsg *errorMsg) {
        // 随便产生一个值
        return new tr::ExExp(new tree::ConstExp(0));
    }

    tr::Exp *TrFactory::IntExp(int val, err::ErrorMsg *errorMsg) {
        // 常数表达式
        return new tr::ExExp(new tree::ConstExp(val));
    }

    tr::Exp *TrFactory::StringExp(const std::string &str, err::ErrorMsg *errorMsg) {
        temp::Label *label = temp::LabelFactory::NewLabel();
        // 把字符串存到片段中
        frags->PushBack(new frame::StringFrag(label, str));
        return new tr::ExExp(new tree::NameExp(label));
    }

    tr::Exp *TrFactory::CallExp(temp::Label *label, tree::Exp *static_link,
                                const std::list<tr::Exp *> &args, tr::Level *caller,
                                err::ErrorMsg *errorMsg) {
        // 把所有参数转换为值表达式存入临时链表
        auto *args_list = new tree::ExpList();
        for (tr::Exp *exp : args) {
            args_list->Append(exp->UnEx());
        }

        auto caller_frame = caller->frame_;
        if (static_link) {
            // 如果存在静态链，那么把静态链插入作为第一个参数
            args_list->Insert(static_link);

            // 更新栈帧的maxArgs值，当这个数量超过参数寄存器数量时，就要在栈上分配空间存储参数
            int args_size = static_cast<int>(args.size() + 1);
            if (args_size > caller_frame->maxArgs) {
                caller_frame->maxArgs = args_size;
            }
            return new tr::ExExp(new tree::CallExp(new tree::NameExp(label), args_list));
        } else {
            int args_size = static_cast<int>(args.size());
            if (args_size > caller_frame->maxArgs) {
                caller_frame->maxArgs = args_size;
            }
            // 没有静态链，说明调用的是库函数
            return new tr::ExExp(frame::FrameFactory::ExternalCall(
                temp::LabelFactory::LabelString(label), args_list));
        }
    }

    tr::Exp *TrFactory::OpExp(absyn::Oper oper, tr::Exp *left, tr::Exp *right,
                              err::ErrorMsg *errorMsg) {
        tree::CjumpStm *stm = nullptr;
        tree::BinopExp *exp = nullptr;

        switch (oper) {
            // 算术表达式
            // + - * /
        case absyn::PLUS_OP:
            exp = new tree::BinopExp(tree::BinOp::PLUS_OP, left->UnEx(), right->UnEx());
            break;
        case absyn::MINUS_OP:
            exp = new tree::BinopExp(tree::BinOp::MINUS_OP, left->UnEx(), right->UnEx());
            break;
        case absyn::TIMES_OP:
            exp = new tree::BinopExp(tree::BinOp::MUL_OP, left->UnEx(), right->UnEx());
            break;
        case absyn::DIVIDE_OP:
            exp = new tree::BinopExp(tree::BinOp::DIV_OP, left->UnEx(), right->UnEx());
            break;
            // 关系表达式
            // == != < <= > >=
        case absyn::EQ_OP:
            stm = new tree::CjumpStm(tree::RelOp::EQ_OP, left->UnEx(), right->UnEx(), nullptr,
                                     nullptr);
            break;
        case absyn::NEQ_OP:
            stm = new tree::CjumpStm(tree::RelOp::NE_OP, left->UnEx(), right->UnEx(), nullptr,
                                     nullptr);
            break;
        case absyn::LT_OP:
            stm = new tree::CjumpStm(tree::RelOp::LT_OP, left->UnEx(), right->UnEx(), nullptr,
                                     nullptr);
            break;
        case absyn::LE_OP:
            stm = new tree::CjumpStm(tree::RelOp::LE_OP, left->UnEx(), right->UnEx(), nullptr,
                                     nullptr);
            break;
        case absyn::GT_OP:
            stm = new tree::CjumpStm(tree::RelOp::GT_OP, left->UnEx(), right->UnEx(), nullptr,
                                     nullptr);
            break;
        case absyn::GE_OP:
            stm = new tree::CjumpStm(tree::RelOp::GE_OP, left->UnEx(), right->UnEx(), nullptr,
                                     nullptr);
            break;
        default:
            break;
        }
        if (stm) {
            return new tr::CxExp(&(stm->true_label_), &(stm->false_label_), stm);
        } else if (exp) {
            return new tr::ExExp(exp);
        }
        // 不会走到下面这种情况
        return nullptr;
    }

    tr::Exp *TrFactory::StringEqual(tr::Exp *left, tr::Exp *right, err::ErrorMsg *errorMsg) {
        auto *exp_list = new tree::ExpList();
        exp_list->Append(left->UnEx());
        exp_list->Append(right->UnEx());
        return new tr::ExExp(frame::FrameFactory::ExternalCall("string_equal", exp_list));
    }

    tr::Exp *TrFactory::RecordExp(const std::vector<tr::Exp *> &exp_list, err::ErrorMsg *errorMsg) {
        temp::Temp *r = temp::TempFactory::NewTemp(); // 存储实例的指针的寄存器

        // 第一步，调用库函数申请内存
        auto *args = new tree::ExpList();
        args->Append(
            // 参数是需要分配的域的数量，每个域用一个指针指向
            new tree::ConstExp(reg_manager->WordSize() * static_cast<int>(exp_list.size())));
        tree::Stm *stm = new tree::MoveStm(new tree::TempExp(r),
                                           frame::FrameFactory::ExternalCall("alloc_record", args));

        // 第二步，开始SeqStm递归，初始index为0
        stm = new tree::SeqStm(stm, RecordField(exp_list, r, 0, errorMsg));

        // 先执行嵌套move语句的seq语句，再把实例指针寄存器当作Eseq的返回值
        return new tr::ExExp(new tree::EseqExp(stm, new tree::TempExp(r)));
    }

    tree::Stm *TrFactory::RecordField(const std::vector<tr::Exp *> &exp_list, temp::Temp *r,
                                      int index, err::ErrorMsg *errorMsg) {
        int size = exp_list.size();
        if (size == 0) {
            return nullptr;
        }

        // r存储的是记录实例的首地址
        auto move_stm = new tree::MoveStm(new tree::MemExp(new tree::BinopExp(
                                              // 目标：内存[首地址 + index × 字长]
                                              tree::BinOp::PLUS_OP, new tree::TempExp(r),
                                              new tree::ConstExp(index * reg_manager->WordSize()))),
                                          // 源：要存入域中的值或表达式
                                          exp_list[index]->UnEx());

        if (index == size - 1) {
            // 如果是最后一个，那么结束递归
            return move_stm;
        } else {
            // 如果不是最后一个，那么继续递归
            return new tree::SeqStm(
                // 先执行移动
                move_stm,
                // 再执行下一个域的递归，使用下一个index
                tr::TrFactory::RecordField(exp_list, r, index + 1, errorMsg));
        }
    }
    tr::Exp *TrFactory::SeqExp(const std::list<tr::Exp *> &exp_list, err::ErrorMsg *errorMsg) {
        tr::Exp *res = new tr::ExExp(new tree::ConstExp(0)); // 语句序列随便用一个返回值
        for (tr::Exp *exp : exp_list) {
            if (exp) {
                res = new tr::ExExp(new tree::EseqExp(res->UnNx(), exp->UnEx()));
            } else {
                // 没有表达式就当成0
                res = new tr::ExExp(new tree::EseqExp(res->UnNx(), new tree::ConstExp(0)));
            }
        }
        return res;
    }

    tr::Exp *TrFactory::AssignExp(tr::Exp *var, tr::Exp *exp, err::ErrorMsg *errorMsg) {
        // 赋值语句没有返回值
        return new tr::NxExp(new tree::MoveStm(var->UnEx(), exp->UnEx()));
    }


    tr::Exp *TrFactory::IfExp(tr::Exp *test, tr::Exp *then, tr::Exp *elsee,
                              err::ErrorMsg *errorMsg) {

        /*
         * 过程：
         *
         * test:
         *       if (condition) then goto t else goto f
         *
         * t:
         *       r <- then
         *       goto done
         *
         * f:
         *       r <- else
         *       goto done
         *
         * done:
         *
         */

        temp::Temp *r = temp::TempFactory::NewTemp(); // 作为返回值的寄存器

        temp::Label *t = temp::LabelFactory::NewLabel(); // 真标签
        temp::Label *f = temp::LabelFactory::NewLabel(); // 假标签

        temp::Label *done = temp::LabelFactory::NewLabel(); // t和f做完后跳转到的后面的公共位置

        auto *done_label_list = new std::vector<temp::Label *>();
        done_label_list->push_back(done);

        Cx cx = test->UnCx(errorMsg);

        // 执行 Do Patch
        *(cx.trues_) = t;
        *(cx.falses_) = f;

        // 如果有else分支
        if (elsee) {
            return new ExExp(
                // 整个是一个表达式序列
                new tree::EseqExp(
                    // if(test)语句，如果是真直接往下，如果是假跳转到f标签
                    cx.stm_,
                    new tree::EseqExp(
                        // 如果是真，那么将then的值给r
                        new tree::LabelStm(t),
                        new tree::EseqExp(
                            new tree::MoveStm(new tree::TempExp(r), then->UnEx()),
                            new tree::EseqExp(
                                // t完毕后，不执行下面的f部分，直接跳转到done位置
                                new tree::JumpStm(new tree::NameExp(done), done_label_list),
                                // --- 分割线 上下不干扰 ---
                                new tree::EseqExp(
                                    // 如果是假，再把else的值给r
                                    new tree::LabelStm(f),
                                    new tree::EseqExp(
                                        new tree::MoveStm(new tree::TempExp(r), elsee->UnEx()),
                                        new tree::EseqExp(
                                            new tree::JumpStm(new tree::NameExp(done),
                                                              done_label_list),
                                            // 这里设置done标签，t或f完毕后跳转到此
                                            // 把r当作返回值
                                            new tree::EseqExp(new tree::LabelStm(done),
                                                              new tree::TempExp(r))))))))));
        } else {
            return new NxExp(
                new tree::SeqStm(cx.stm_, new tree::SeqStm(
                                              // t标签后面是then的语句
                                              new tree::LabelStm(t),
                                              new tree::SeqStm(then->UnNx(),
                                                               // 如果为f那么相当于跳过了then语句
                                                               new tree::LabelStm(f)))));
        }
    }

    tr::Exp *TrFactory::WhileExp(tr::Exp *test, tr::Exp *body, temp::Label *done_label,
                                 err::ErrorMsg *errorMsg) {

        /*
         * 过程：
         *
         * test:
         *       if condition then body else goto done
         *
         * body:
         *       ...
         *       goto test
         *
         * done:
         *
         */

        temp::Label *test_label = temp::LabelFactory::NewLabel(); // test标签
        temp::Label *body_label = temp::LabelFactory::NewLabel(); // body标签

        Cx cx = test->UnCx(errorMsg);

        // 执行 Do Patch
        *(cx.trues_) = body_label;
        *(cx.falses_) = done_label;

        auto *test_label_list = new std::vector<temp::Label *>();
        test_label_list->push_back(test_label);

        return new tr::NxExp(new tree::SeqStm(
            // 放置test标签
            new tree::LabelStm(test_label),
            new tree::SeqStm(
                // test语句，判断之后跳转，如果为真跳转到body，为假跳转到done
                cx.stm_, new tree::SeqStm(
                             // 放置body标签
                             new tree::LabelStm(body_label),
                             new tree::SeqStm(body->UnNx(),
                                              new tree::SeqStm(
                                                  // body完毕后跳转到test位置
                                                  new tree::JumpStm(new tree::NameExp(test_label),
                                                                    test_label_list),
                                                  // 放置done标签
                                                  new tree::LabelStm(done_label)))))));
    }

    tr::Exp *TrFactory::ForExp(frame::Access *access, tr::Exp *lo, tr::Exp *hi, tr::Exp *body,
                               temp::Label *done_label, err::ErrorMsg *errorMsg) {

        /*
         * 过程：
         *
         * i <-lo
         *
         * test:
         *       if i<hi then body else done
         *
         * body:
         *       ...
         *       i <- i+1
         *       goto test
         *
         * done:
         *
         *
         */

        temp::Label *body_label = temp::LabelFactory::NewLabel();
        temp::Label *test_label = temp::LabelFactory::NewLabel();

        // 循环变量i可能是逃逸的也可能不是，所以结果可能是寄存器也可能是栈上
        tree::Exp *i = access->toExp(new tree::TempExp(reg_manager->FramePointer()));
        return new tr::NxExp(new tree::SeqStm(
            // 把lo赋值给i
            new tree::MoveStm(i, lo->UnEx()),
            new tree::SeqStm(
                // 放置test标签
                new tree::LabelStm(test_label),
                new tree::SeqStm(
                    // 如果i<hi那么跳转到body位置，否则跳转到done
                    new tree::CjumpStm(tree::RelOp::LE_OP, i, hi->UnEx(), body_label, done_label),
                    new tree::SeqStm(
                        // 放置body标签
                        new tree::LabelStm(body_label),
                        new tree::SeqStm(
                            // 执行body语句
                            body->UnNx(),
                            new tree::SeqStm(
                                // 然后将i的值加1
                                new tree::MoveStm(i, new tree::BinopExp(tree::BinOp::PLUS_OP, i,
                                                                        new tree::ConstExp(1))),
                                new tree::SeqStm(
                                    // 跳转到test表亲啊的位置
                                    new tree::JumpStm(
                                        new tree::NameExp(test_label),
                                        new std::vector<temp::Label *>(1, test_label)),
                                    // 放置done标签
                                    new tree::LabelStm(done_label)))))))));
    }

    tr::Exp *TrFactory::BreakExp(temp::Label *label, err::ErrorMsg *errorMsg) {
        return new tr::NxExp(
            // 简单的跳转语句，直接跳转到label位置
            new tree::JumpStm(new tree::NameExp(label), new std::vector<temp::Label *>(1, label)));
    }


    tr::Exp *TrFactory::LetExp(const std::list<Exp *> &dec_list, tr::Exp *body,
                               err::ErrorMsg *errorMsg) {

        /*
         * let a = ...                    (dec)
         *     b = ...
         *     function f1() = ...
         *
         *  in
         *     ...                        (body)
         *
         *  end
         *
         *
         */

        tr::Exp *res = nullptr;
        for (tr::Exp *dec : dec_list) {
            if (!res) {
                res = dec;
            } else {
                // 与SeqStm同理
                if (dec) {
                    res = new tr::ExExp(new tree::EseqExp(res->UnNx(), dec->UnEx()));
                } else {
                    res = new tr::ExExp(new tree::EseqExp(res->UnNx(), new tree::ConstExp(0)));
                }
            }
        }
        if (!res) {
            res = body; // 如果没有定义任何东西，那么直接使用body
        } else {
            res = new tr::ExExp(new tree::EseqExp(res->UnNx(), body->UnEx()));
        }
        return res;
    }

    tr::Exp *TrFactory::ArrayExp(tr::Exp *size, tr::Exp *init, err::ErrorMsg *errorMsg) {
        // 调用库函数给数组分配空间
        return new tr::ExExp(frame::FrameFactory::ExternalCall(
            "init_array", new tree::ExpList({size->UnEx(), init->UnEx()})));
    }

    void TrFactory::FunctionDec(tr::Exp *body, tr::Level *level, err::ErrorMsg *errorMsg) {
        // 此过程在编译器执行，生成指令以后把指令存起来，运行时可以拿出来用

        /*
         * 过程：
         *
         *      Seq
         *       |
         *   ---------
         *   |       |
         *  v_s      Move
         *            |
         *      -------------
         *      |           |
         *     %rax      Body Exp
         *                  |
         *              ----------
         *              |        |
         *             ...      ...
         *
         *  运行时整个执行顺序：
         *  先做view_shift，把参数寄存器和栈上的所有参数都存起来
         *  再做body_exp，算完以后，把返回值存到%rax里
         *
         */

        tree::Stm *stm = new tree::MoveStm(
            // 将函数的返回值移动到%rax寄存器上的指令
            new tree::TempExp(reg_manager->ReturnValue()), body->UnEx());

        stm = frame::FrameFactory::ProcEntryExit1(level->frame_, stm); // 执行view_shift

        // 保存函数片段
        frags->PushBack(new frame::ProcFrag(stm, level->frame_));
    }

} // namespace tr

namespace absyn {

    tr::ExpAndTy AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                      temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        return root_->Translate(venv, tenv, level, label, errormsg); // 从根节点开始翻译
    }

    tr::ExpAndTy SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                      temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        // 在环境中寻找此变量
        env::EnvEntry *entry = venv->Look(sym_);
        env::VarEntry *var_entry = static_cast<env::VarEntry *>(entry);

        // 此access应该已经在创建时被分配好了
        auto exp = tr::TrFactory::SimpleVar(var_entry->access_, level, errormsg);

        return tr::ExpAndTy(exp, var_entry->ty_->ActualTy());
    }

    tr::ExpAndTy FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                     temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        // 先翻译结构体的名字
        tr::ExpAndTy var_info = var_->Translate(venv, tenv, level, label, errormsg);
        type::RecordTy *record_ty = static_cast<type::RecordTy *>(var_info.ty_->ActualTy());

        int i = 0;
        std::list<type::Field *> field_list = record_ty->fields_->GetList();

        // 在所有域中找当前域的名字
        for (type::Field *field : field_list) {
            if (field->name_->Name() == sym_->Name()) {
                return tr::ExpAndTy(tr::TrFactory::FieldVar(var_info.exp_, i, errormsg),
                                    var_info.ty_->ActualTy());
            }
            i++;
        }
        errormsg->Error(pos_, "field %s dosen't exist", sym_->Name().c_str());
        return tr::ExpAndTy(nullptr, type::IntTy::Instance());
    }

    tr::ExpAndTy SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                         temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        // 先翻译数组的名字
        tr::ExpAndTy var_info = var_->Translate(venv, tenv, level, label, errormsg);
        // 再翻译数组的下标
        tr::ExpAndTy subscript_info = subscript_->Translate(venv, tenv, level, label, errormsg);
        type::Ty *ty = var_info.ty_->ActualTy();

        return tr::ExpAndTy(
            tr::TrFactory::SubscriptVar(var_info.exp_, subscript_info.exp_, errormsg),
            static_cast<type::ArrayTy *>(ty)->ty_->ActualTy());
    }

    tr::ExpAndTy VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                   temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        return var_->Translate(venv, tenv, level, label, errormsg);
    }

    tr::ExpAndTy NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                   temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        return tr::ExpAndTy(tr::TrFactory::NilExp(errormsg), type::NilTy::Instance());
    }

    tr::ExpAndTy IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                   temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        return tr::ExpAndTy(tr::TrFactory::IntExp(val_, errormsg), type::IntTy::Instance());
    }

    tr::ExpAndTy StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                      temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        return tr::ExpAndTy(tr::TrFactory::StringExp(str_, errormsg), type::StringTy::Instance());
    }

    tr::ExpAndTy CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                    temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        env::EnvEntry *entry = venv->Look(func_);
        // 从类型环境中找该函数
        if (!entry || entry->type() != env::EnvEntry::Function) {
            errormsg->Error(pos_, "undefined function %s", func_->Name().c_str());
            return tr::ExpAndTy(nullptr, type::IntTy::Instance());
        }

        env::FunEntry *fun_entry = static_cast<env::FunEntry *>(entry);
        std::list<tr::Exp *> arg_exp_list;

        // 对每个参数进行翻译
        for (absyn::Exp *exp : args_->GetList()) {
            tr::ExpAndTy arg_info = exp->Translate(venv, tenv, level, label, errormsg);
            arg_exp_list.push_back(arg_info.exp_);
        }

        return tr::ExpAndTy(
            tr::TrFactory::CallExp(this->func_,
                                   // 需要传递的是函数定义所在的层级，这个层级是函数自身层级的父层级
                                   tr::TrFactory::StaticLink(level, fun_entry->level_->parent_),
                                   arg_exp_list, level, errormsg),
            fun_entry->result_);
    }

    tr::ExpAndTy OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                  temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        tr::ExpAndTy left_info = left_->Translate(venv, tenv, level, label, errormsg);
        tr::ExpAndTy right_info = right_->Translate(venv, tenv, level, label, errormsg);
        if (left_info.ty_->IsSameType(type::StringTy::Instance()) &&
            right_info.ty_->IsSameType(type::StringTy::Instance())) {
            // 如果是比较字符串
            return tr::ExpAndTy(
                tr::TrFactory::StringEqual(left_info.exp_, right_info.exp_, errormsg),
                type::IntTy::Instance());
        }
        // 如果是数值操作
        return tr::ExpAndTy(tr::TrFactory::OpExp(oper_, left_info.exp_, right_info.exp_, errormsg),
                            type::IntTy::Instance());
    }

    tr::ExpAndTy RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                      temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        type::Ty *ty = tenv->Look(typ_)->ActualTy();
        std::vector<tr::Exp *> exp_list;

        // 这是定义一个新记录的表达式，需要对每个域传入的表达式都做翻译
        const auto &efields = fields_->GetList();
        for (absyn::EField *efield : efields) {
            tr::ExpAndTy exp_info = efield->exp_->Translate(venv, tenv, level, label, errormsg);
            exp_list.push_back(exp_info.exp_);
        }
        // 为其开辟空间
        return tr::ExpAndTy(tr::TrFactory::RecordExp(exp_list, errormsg), ty);
    }

    tr::ExpAndTy SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                   temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        if (!seq_) {
            return tr::ExpAndTy(nullptr, type::VoidTy::Instance());
        }
        std::list<tr::Exp *> exp_list;
        type::Ty *ty = type::VoidTy::Instance();

        // 对序列中每个表达式都进行翻译
        const auto &seqs = seq_->GetList();
        for (absyn::Exp *exp : seqs) {
            tr::ExpAndTy expAndTy = exp->Translate(venv, tenv, level, label, errormsg);
            exp_list.push_back(expAndTy.exp_);
            ty = expAndTy.ty_;
        }
        // 然后连接起来
        return tr::ExpAndTy(tr::TrFactory::SeqExp(exp_list, errormsg), ty);
    }

    tr::ExpAndTy AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                      temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        // 翻译左值
        tr::ExpAndTy var_info = var_->Translate(venv, tenv, level, label, errormsg);
        // 翻译右值
        tr::ExpAndTy exp_info = exp_->Translate(venv, tenv, level, label, errormsg);
        // 执行move语句
        return tr::ExpAndTy(tr::TrFactory::AssignExp(var_info.exp_, exp_info.exp_, errormsg),
                            type::VoidTy::Instance());
    }

    tr::ExpAndTy IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                  temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        // 翻译条件表达式
        tr::ExpAndTy test_info = test_->Translate(venv, tenv, level, label, errormsg);
        // 翻译真值表达式
        tr::ExpAndTy then_info = then_->Translate(venv, tenv, level, label, errormsg);
        // 翻译假值表达式，可能没有
        tr::ExpAndTy else_info(nullptr, nullptr);
        if (elsee_) {
            else_info = elsee_->Translate(venv, tenv, level, label, errormsg);
        }
        return tr::ExpAndTy(
            tr::TrFactory::IfExp(test_info.exp_, then_info.exp_, else_info.exp_, errormsg),
            then_info.ty_);
    }

    tr::ExpAndTy WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                     temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        // While循环结束后的位置，生成语句时需要传入这个标签，而test标签在生成语句时自动创建
        temp::Label *done_label = temp::LabelFactory::NewLabel();
        // 翻译条件表达式
        tr::ExpAndTy test_info = test_->Translate(venv, tenv, level, label, errormsg);
        // 翻译循环体
        tr::ExpAndTy body_info = body_->Translate(venv, tenv, level, done_label, errormsg);

        return tr::ExpAndTy(
            tr::TrFactory::WhileExp(test_info.exp_, body_info.exp_, done_label, errormsg),
            body_info.ty_);
    }

    tr::ExpAndTy ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                   temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        venv->BeginScope();

        temp::Label *done_label = temp::LabelFactory::NewLabel();
        tr::ExpAndTy lo_info = lo_->Translate(venv, tenv, level, label, errormsg);
        tr::ExpAndTy hi_info = hi_->Translate(venv, tenv, level, label, errormsg);

        // 给循环计数器开辟空间，可能是逃逸的，逃逸就要在栈上开辟
        tr::Access *access = tr::Access::AllocLocal(level, this->escape_);

        venv->Enter(var_, new env::VarEntry(access, lo_info.ty_, true));

        // 翻译循环体表达式
        tr::ExpAndTy body_info = body_->Translate(venv, tenv, level, done_label, errormsg);

        venv->EndScope();

        return tr::ExpAndTy(tr::TrFactory::ForExp(access->access_, lo_info.exp_, hi_info.exp_,
                                                  body_info.exp_, done_label, errormsg),
                            type::VoidTy::Instance());
    }

    tr::ExpAndTy BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                     temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        // 传入done的标签
        return tr::ExpAndTy(tr::TrFactory::BreakExp(label, errormsg), type::VoidTy::Instance());
    }

    tr::ExpAndTy LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                   temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        std::list<tr::Exp *> dec_list;

        const auto &decs = decs_->GetList();
        // 翻译所有的声明
        for (absyn::Dec *dec : decs) {
            dec_list.push_back(dec->Translate(venv, tenv, level, label, errormsg));
        }
        // 翻译body
        tr::ExpAndTy body_info = body_->Translate(venv, tenv, level, label, errormsg);
        return tr::ExpAndTy(tr::TrFactory::LetExp(dec_list, body_info.exp_, errormsg),
                            body_info.ty_);
    }

    tr::ExpAndTy ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                     temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        // 翻译中括号内表达式
        tr::ExpAndTy size_info = size_->Translate(venv, tenv, level, label, errormsg);
        // 翻译名字
        tr::ExpAndTy init_info = init_->Translate(venv, tenv, level, label, errormsg);

        return tr::ExpAndTy(tr::TrFactory::ArrayExp(size_info.exp_, init_info.exp_, errormsg),
                            tenv->Look(typ_));
    }

    tr::ExpAndTy VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                    temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        return tr::ExpAndTy(nullptr, type::VoidTy::Instance());
    }

    tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                    temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */

        const auto &funs = functions_->GetList();

        // 为了使后面的函数调用前面的函数不会找不到，先把所有声明加入环境
        for (absyn::FunDec *function : funs) {
            type::TyList *tyList = function->params_->MakeFormalTyList(tenv, errormsg);
            // 创建一个以函数名为名字的标签，唯一的
            temp::Label *function_label = temp::LabelFactory::NamedLabel(function->name_->Name());

            // 逃逸表
            std::list<bool> escapes;
            for (absyn::Field *field : function->params_->GetList()) {
                escapes.push_back(field->escape_);
            }

            // 新建层级
            tr::Level *new_level = tr::Level::NewLevel(level, function_label, escapes);

            // 如果有返回值，那么传入返回值类型，否则用void
            if (function->result_) {
                type::Ty *result_ty = tenv->Look(function->result_);
                venv->Enter(function->name_,
                            new env::FunEntry(new_level, function_label, tyList, result_ty));
            } else {
                venv->Enter(function->name_, new env::FunEntry(new_level, function_label, tyList,
                                                               type::VoidTy::Instance()));
            }
        }

        // 再逐个翻译函数体
        for (absyn::FunDec *function : funs) {
            type::TyList *tyList = function->params_->MakeFormalTyList(tenv, errormsg);
            env::FunEntry *entry = static_cast<env::FunEntry *>(venv->Look(function->name_));

            const auto &param_list = function->params_->GetList();
            const auto &type_list = tyList->GetList();
            const auto &access_list = entry->level_->GetAccessList();

            venv->BeginScope();

            auto type_it = type_list.begin();
            auto param_it = param_list.begin();
            auto access_it = std::next(access_list.begin()); // 跳过静态链

            // 把参数一个一个存入变量环境
            for (; param_it != param_list.end(); type_it++, param_it++, access_it++) {
                venv->Enter((*param_it)->name_, new env::VarEntry(*access_it, *type_it));
            }

            // 翻译函数体
            tr::ExpAndTy body_info =
                function->body_->Translate(venv, tenv, entry->level_, label, errormsg);
            venv->EndScope();

            // 生成函数片段并存储
            tr::TrFactory::FunctionDec(body_info.exp_, entry->level_, errormsg);
        }
        return tr::TrFactory::NilExp(errormsg);
    }

    tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                               temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        tr::ExpAndTy init_info = init_->Translate(venv, tenv, level, label, errormsg);
        type::Ty *init_ty = init_info.ty_->ActualTy();
        if (this->typ_) {
            init_ty = tenv->Look(this->typ_)->ActualTy();
        }

        // 给变量分配空间，在寄存器中或栈上
        tr::Access *access = tr::Access::AllocLocal(level, this->escape_);
        // 变量加入环境
        venv->Enter(this->var_, new env::VarEntry(access, init_ty));

        // 访问变量的表达式
        tr::Exp *var_exp = tr::TrFactory::SimpleVar(access, level, errormsg);

        // 把初值赋给变量
        return new tr::NxExp(new tree::MoveStm(var_exp->UnEx(), init_info.exp_->UnEx()));
    }

    tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                                temp::Label *label, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        const auto &type_list = types_->GetList();
        // 查找同名类型，并将声明加入环境以前面的类型使用后面的类型出错
        for (absyn::NameAndTy *name_and_type : type_list) {
            for (absyn::NameAndTy *check_name : type_list) {
                // 如果是同一个那就跳过
                if (check_name == name_and_type) {
                    continue;
                }
                // 如果不是同一个又是同名的，那么报错
                if (check_name->name_->Name() == name_and_type->name_->Name()) {
                    errormsg->Error(pos_, "two types have the same name");
                }
            }
            // 临时给所有名字都创建一个NameTy
            tenv->Enter(name_and_type->name_, new type::NameTy(name_and_type->name_, nullptr));
        }

        // 翻译每个类型，并给对应的NameTy对象激活为实际类型
        for (absyn::NameAndTy *name_and_type : type_list) {
            type::Ty *ty = name_and_type->ty_->Translate(tenv, errormsg);
            // 当前层所有能找到的都是NameTy
            type::NameTy *name_ty = static_cast<type::NameTy *>(tenv->Look(name_and_type->name_));
            name_ty->ty_ = ty;
        }

        for (absyn::NameAndTy *name_and_type : type_list) {
            type::Ty *ty = tenv->Look(name_and_type->name_);
            std::vector<sym::Symbol *> trace;
            while (ty->type() == type::Ty::Name) {
                type::NameTy *name_ty = static_cast<type::NameTy *>(ty);
                // 如果都是NameTy并且绕了一圈
                for (sym::Symbol *symbol : trace) {
                    if (symbol->Name() == name_ty->sym_->Name()) {
                        errormsg->Error(pos_, "illegal type cycle");
                    }
                }
                trace.push_back(name_ty->sym_);
                if (!name_ty->ty_) {
                    errormsg->Error(pos_, "undefined type %s", name_ty->sym_->Name().c_str());
                }
                ty = name_ty->ty_;
            }
        }
        return tr::TrFactory::NilExp(errormsg);
    }

    // 与语义分析一样的处理
    type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        return new type::NameTy(name_, tenv->Look(name_));
    }

    type::Ty *RecordTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        type::FieldList *fields = record_->MakeFieldList(tenv, errormsg);
        return new type::RecordTy(fields);
    }

    type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab5 code here */
        type::Ty *ty = tenv->Look(array_);
        if (!ty) {
            errormsg->Error(pos_, "undefined type %s", array_->Name().c_str());
            return type::IntTy::Instance();
        }
        return new type::ArrayTy(ty);
    }

} // namespace absyn
