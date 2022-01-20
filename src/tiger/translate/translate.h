#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

#include <list>
#include <memory>

#include "tiger/absyn/absyn.h"
#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/semant/types.h"

namespace tr {

    class Exp;
    class ExpAndTy;
    class Level;

    class Access {
    public:
        Level *level_;
        frame::Access *access_;

        Access(Level *level, frame::Access *access) : level_(level), access_(access) {
        }
        static Access *AllocLocal(Level *level, bool escape);
    };

    class Level {
    public:
        frame::Frame *frame_;
        Level *parent_;

        /* TODO: Put your lab5 code here */
        Level(frame::Frame *frame, Level *parent) : frame_(frame), parent_(parent) {
        }

        std::list<tr::Access *> GetAccessList() {
            std::list<frame::Access *> formals = frame_->formals_;
            std::list<tr::Access *> ret;
            for (frame::Access *access : formals) {
                ret.push_back(new tr::Access(this, access));
            }
            return ret;
        }

        static Level *NewLevel(tr::Level *parent, temp::Label *name,
                               const std::list<bool> &formals) {
            auto list = formals;
            list.push_front(true); // 隐式在头部添加静链，并且它一定是逃逸的
            frame::Frame *f = frame::FrameFactory::NewFrame(name, list);
            return new Level(f, parent);
        }
    };

    class ProgTr {
    public:
        /* TODO: Put your lab5 code here */
        ProgTr(std::unique_ptr<absyn::AbsynTree> absyn_tree,
               std::unique_ptr<err::ErrorMsg> errormsg);

        /**
         * Translate IR tree
         */
        void Translate();

        /**
         * Transfer the ownership of errormsg to outer scope
         * @return unique pointer to errormsg
         */
        std::unique_ptr<err::ErrorMsg> TransferErrormsg() {
            return std::move(errormsg_);
        }

    private:
        std::unique_ptr<absyn::AbsynTree> absyn_tree_;
        std::unique_ptr<err::ErrorMsg> errormsg_;
        std::unique_ptr<Level> main_level_;
        std::unique_ptr<env::TEnv> tenv_;
        std::unique_ptr<env::VEnv> venv_;

        // Fill base symbol for var env and type env
        void FillBaseVEnv();
        void FillBaseTEnv();
    };


    class TrFactory {
    public:
        // 求解静态链的中间方法
        static tree::Exp *StaticLink(tr::Level *current, tr::Level *target);

    public:
        // 翻译成表达式
        static tr::Exp *SimpleVar(tr::Access *access, tr::Level *level, err::ErrorMsg *errorMsg);

        static tr::Exp *FieldVar(tr::Exp *exp, int offset, err::ErrorMsg *errorMsg);

        static tr::Exp *SubscriptVar(Exp *exp, Exp *index, err::ErrorMsg *errorMsg);

        static tr::Exp *NilExp(err::ErrorMsg *errorMsg);

        static tr::Exp *IntExp(int val, err::ErrorMsg *errorMsg);

        static tr::Exp *StringExp(const std::string &str, err::ErrorMsg *errorMsg);

        static tr::Exp *CallExp(temp::Label *label, tree::Exp *static_link,
                                const std::list<Exp *> &args, Level *caller,
                                err::ErrorMsg *errorMsg);

        static tr::Exp *OpExp(absyn::Oper oper, tr::Exp *left, tr::Exp *right,
                              err::ErrorMsg *errorMsg);

        // 判断字符串是否相等的表达式
        static tr::Exp *StringEqual(tr::Exp *left, tr::Exp *right, err::ErrorMsg *errorMsg);

        // 创建记录类型，使用vector便于访问下标
        static tr::Exp *RecordExp(const std::vector<tr::Exp *> &exp_list, err::ErrorMsg *errorMsg);

        // 用来执行RecordExp的递归函数
        static tree::Stm *RecordField(const std::vector<tr::Exp *> &exp_list, temp::Temp *r,
                                      int index, err::ErrorMsg *errorMsg);

        static tr::Exp *SeqExp(const std::list<tr::Exp *> &exp_list, err::ErrorMsg *errorMsg);

        static tr::Exp *AssignExp(Exp *var, Exp *exp, err::ErrorMsg *errorMsg);

        static tr::Exp *IfExp(Exp *test, Exp *then, Exp *elsee, err::ErrorMsg *errorMsg);

        static tr::Exp *WhileExp(Exp *test, Exp *body, temp::Label *done_label,
                                 err::ErrorMsg *errorMsg);

        static tr::Exp *ForExp(frame::Access *access, Exp *lo, Exp *hi, Exp *body,
                               temp::Label *done_label, err::ErrorMsg *errorMsg);

        static tr::Exp *BreakExp(temp::Label *label, err::ErrorMsg *errorMsg);

        static tr::Exp *LetExp(const std::list<Exp *> &dec_list, tr::Exp *body,
                               err::ErrorMsg *errorMsg);

        static tr::Exp *ArrayExp(Exp *size, Exp *init, err::ErrorMsg *errorMsg);

        // 函数定义，不返回值，将其存到片段即可
        static void FunctionDec(Exp *body, Level *level, err::ErrorMsg *errorMsg);
    };

} // namespace tr

#endif
