#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"

#include <iostream>

#define DEFAULT_TYPE type::IntTy::Instance()

namespace absyn {

    void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */
        type::Ty *ty = root_->SemAnalyze(venv, tenv, 0, errormsg);
    }

    type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */
        env::EnvEntry *entry = venv->Look(sym_);
        if (entry && entry->type() == env::EnvEntry::Variable) {
            return (static_cast<env::VarEntry *>(entry))->ty_->ActualTy();
        } else {
            errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
        }
        return DEFAULT_TYPE;
    }

    type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        // Member variable check
        type::Ty *ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

        if (typeid(*ty) != typeid(type::RecordTy)) {
            errormsg->Error(pos_, "not a record type");
        } else {
            type::RecordTy *rec = static_cast<type::RecordTy *>(ty);
            type::FieldList *fields = rec->fields_;
            const auto &list = fields->GetList();

            type::Ty *t = nullptr;
            for (auto it = list.begin(); it != list.end(); ++it) {
                if (sym_ == (*it)->name_) {
                    t = (*it)->ty_;
                    break;
                }
            }
            if (!t) {
                errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
            } else {
                return t->ActualTy();
            }
        }

        return DEFAULT_TYPE;
    }

    type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                                       err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        // Subscript check

        type::Ty *sty = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);
        if (typeid(*sty) != typeid(type::IntTy)) {
            errormsg->Error(pos_, "array variable's subscript must be an integer");
            return DEFAULT_TYPE;
        }

        type::Ty *vty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
        if (typeid(*vty) != typeid(type::ArrayTy)) {
            errormsg->Error(pos_, "array type required");
            return DEFAULT_TYPE;
        } else {
            type::ArrayTy *arr = static_cast<type::ArrayTy *>(vty);
            return arr->ty_->ActualTy();
        }

        return DEFAULT_TYPE;
    }

    type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
    }

    type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        return type::NilTy::Instance();
    }

    type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        return type::IntTy::Instance();
    }

    type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        return type::StringTy::Instance();
    }

    type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        env::EnvEntry *entry = venv->Look(func_);

        // Check function existence
        if (entry && entry->type() == env::EnvEntry::Function) {
            env::FunEntry *fun = static_cast<env::FunEntry *>(entry);

            const auto &reqs = fun->formals_->GetList();
            const auto &args = this->args_->GetList();

            bool success = true;

            // Check arguments match required types
            auto it1 = reqs.begin();
            auto it2 = args.begin();
            absyn::Exp *exp;

            for (; it1 != reqs.end() && it2 != args.end(); ++it1, ++it2) {
                exp = *it2;
                type::Ty *ty = exp->SemAnalyze(venv, tenv, labelcount, errormsg);

                if ((*it1)->ActualTy() != ty->ActualTy()) {
                    errormsg->Error(exp->pos_, "para type mismatch");
                    success = false;
                }
            }

            if (it2 != args.end()) {
                errormsg->Error(exp->pos_, "too many params in function %s", func_->Name().data());
            } else if (it1 != reqs.end()) {
                errormsg->Error(pos_, "too few params in function %s", func_->Name().data());
            }
            return fun->result_->ActualTy();

        } else {
            errormsg->Error(pos_, "undefined function %s", func_->Name().data());
        }

        return DEFAULT_TYPE;
    }

    type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        type::Ty *left_ty = left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
        type::Ty *right_ty = right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

        switch (oper_) {
        case absyn::PLUS_OP:
        case absyn::MINUS_OP:
        case absyn::TIMES_OP:
        case absyn::DIVIDE_OP: {
            if (typeid(*left_ty) != typeid(type::IntTy)) {
                errormsg->Error(left_->pos_, "integer required");
            }
            if (typeid(*right_ty) != typeid(type::IntTy)) {
                errormsg->Error(right_->pos_, "integer required");
            }
            return type::IntTy::Instance();
            break;
        }
        default: {
            if (!left_ty->IsSameType(right_ty)) {
                errormsg->Error(pos_, "same type required");
            };
            break;
        }
        }

        return DEFAULT_TYPE;
    }

    type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        type::Ty *ty = tenv->Look(typ_);
        if (!ty) {
            // Check type existence
            errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
        } else {
            ty = ty->ActualTy();
            if (typeid(*ty) != typeid(type::RecordTy)) {
                // Check if record
                errormsg->Error(pos_, std::string("not a record type"));
                return ty;
            } else {
                type::RecordTy *rec = static_cast<type::RecordTy *>(ty);
                type::FieldList *fields = rec->fields_;

                Exp *exp = nullptr;
                type::Field *field = nullptr;

                const auto &list = fields->GetList();        // Exist fields
                const auto &args = this->fields_->GetList(); // Given efields
                auto it1 = args.begin();

                for (; it1 != args.end(); ++it1) {
                    exp = (*it1)->exp_;
                    field = nullptr;
                    // Check field existence
                    for (auto it2 = list.begin(); it2 != list.end(); ++it2) {
                        if ((*it2)->name_ == (*it1)->name_) {
                            field = *it2;
                            break;
                        }
                    }
                    if (!field) {
                        break;
                    }
                    // Analyze exp
                    type::Ty *ty = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
                    // Check same type
                    if (!ty->IsSameType(field->ty_)) {
                        errormsg->Error(exp->pos_, "unmatched assign exp");
                    }
                }

                if (!field) {
                    errormsg->Error(exp->pos_, "field nam doesn't exist");
                }
                return ty;
            }
        }
        return DEFAULT_TYPE;
    }

    type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        const auto &exps = seq_->GetList();

        type::Ty *ty = type::VoidTy::Instance();
        for (auto it = exps.begin(); it != exps.end(); ++it) {
            ty = (*it)->SemAnalyze(venv, tenv, labelcount, errormsg);
        }

        return ty;
    }

    type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        type::Ty *left_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
        type::Ty *right_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);

        if (typeid(*var_) == typeid(SimpleVar)) {
            SimpleVar *svar = static_cast<SimpleVar *>(var_);
            env::EnvEntry *entry = venv->Look(svar->sym_);
            if (entry->readonly_) {
                errormsg->Error(svar->pos_, "loop variable can't be assigned");
            }
        }

        if (!left_ty->IsSameType(right_ty)) {
            errormsg->Error(pos_, "unmatched assign exp");
        }

        return type::VoidTy::Instance();
    }

    type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
        type::Ty *then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
        type::Ty *else_ty = nullptr;

        if (typeid(*test_ty) != typeid(type::IntTy)) {
            errormsg->Error(test_->pos_, "if exp's test must produce int value");
        }

        if (!elsee_) {
            if (typeid(*then_ty) != typeid(type::VoidTy)) {
                errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
            }
        } else {
            else_ty = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
            if (!then_ty->IsSameType(else_ty)) {
                errormsg->Error(then_->pos_, "then exp and else exp type mismatch");
            }
            return then_ty->ActualTy();
        }

        return type::VoidTy::Instance();
    }

    type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
        type::Ty *body_ty = nullptr;

        if (typeid(*test_ty) != typeid(type::IntTy)) {
            errormsg->Error(test_->pos_, "while exp's test must produce int value");
        }

        if (body_) {
            body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
            if (typeid(*body_ty) != typeid(type::VoidTy)) {
                errormsg->Error(body_->pos_, "while body must produce no value");
            }
        }

        return type::VoidTy::Instance();
    }

    type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        type::Ty *l_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
        type::Ty *h_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);

        type::Ty *body_ty = nullptr;

        if (typeid(*l_ty) != typeid(type::IntTy)) {
            errormsg->Error(lo_->pos_, "for exp's range type is not integer");
        }
        if (typeid(*h_ty) != typeid(type::IntTy)) {
            errormsg->Error(hi_->pos_, "for exp's range type is not integer");
        }

        if (body_) {
            venv->BeginScope();
            venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));
            body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
            venv->EndScope();

            if (typeid(*body_ty) != typeid(type::VoidTy)) {
                errormsg->Error(body_->pos_, "for exp's body must produce no value");
            }
        }

        return type::VoidTy::Instance();
    }

    type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        if (labelcount == 0) {
            errormsg->Error(pos_, "break is not inside any loop");
        }

        return type::VoidTy::Instance();
    }

    type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        venv->BeginScope();
        tenv->BeginScope();

        const auto &list = decs_->GetList();
        for (Dec *dec : list) {
            dec->SemAnalyze(venv, tenv, labelcount, errormsg);
        }
        type::Ty *result;

        if (!body_) {
            result = type::VoidTy::Instance();
        } else {
            result = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
        }

        tenv->EndScope();
        venv->EndScope();

        return result;
    }

    type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        type::Ty *ty = tenv->Look(typ_);
        if (!ty) {
            // Check type existence
            errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
        } else {
            ty = ty->ActualTy();
            if (typeid(*ty) != typeid(type::ArrayTy)) {
                // Check if record
                errormsg->Error(pos_, "not an array type");
            } else {
                type::ArrayTy *arr = static_cast<type::ArrayTy *>(ty);
                type::Ty *item_ty = arr->ty_;

                type::Ty *size_ty = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
                type::Ty *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);

                if (typeid(*size_ty) != typeid(type::IntTy)) {
                    errormsg->Error(size_->pos_, "integer required");
                }
                if (!item_ty->IsSameType(init_ty)) {
                    errormsg->Error(init_->pos_, "type mismatch");
                }

                return arr->ActualTy();
            }
        }

        return DEFAULT_TYPE;
    }

    type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        return type::VoidTy::Instance();
    }

    void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        // absyn::FunDec *function = functions_->GetList().front();
        // type::Ty *result_ty = tenv->Look(function->result_);

        // auto params = function->params_;
        // type::TyList *formals = params->MakeFormalTyList(tenv, errormsg);

        // venv->Enter(function->name_, new env::FunEntry(formals, result_ty));
        // venv->BeginScope();

        // auto formal_it = formals->GetList().begin();
        // auto param_it = params->GetList().begin();
        // for (; param_it != params->GetList().end(); formal_it++, param_it++) {
        //     venv->Enter((*param_it)->name_, new env::VarEntry(*formal_it));
        // }
        // type::Ty *ty = function->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
        // venv->EndScope();

        // ---

        //    const auto &funs = this->functions_->GetList();
        //    for (auto it = funs.begin(); it != funs.end(); it++) {
        //        auto fundec = *it;
        //        type::Ty *result = type::VoidTy::Instance();
        //        if (fundec->result_) {
        //            result = tenv->Look(fundec->result_);
        //        }
        //
        //        auto formaltylist = fundec->params_->MakeFormalTyList(tenv, errormsg);
        //        if (venv->Look(fundec->name_)) {
        //            errormsg->Error(pos_, "two functions have the same name");
        //        } else {
        //            venv->Enter(fundec->name_, new env::FunEntry(formaltylist, result));
        //        }
        //    }
        //
        //    for (auto it = funs.begin(); it != funs.end(); it++) {
        //        venv->BeginScope();
        //        const auto &actuals = (*it)->params_->GetList();
        //        auto funentry = (env::FunEntry *)venv->Look((*it)->name_);
        //        const auto &formals = funentry->formals_->GetList();
        //
        //        auto acit = actuals.begin();
        //        auto fmit = formals.begin();
        //        for (; acit != actuals.end() && fmit != formals.end(); acit++, fmit++) {
        //            venv->Enter((*acit)->name_, new env::VarEntry(*fmit));
        //        }
        //
        //        auto resty = (*it)->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
        //        if (!resty->IsSameType(type::VoidTy::Instance())) {
        //            if (funentry->result_->IsSameType(type::VoidTy::Instance())) {
        //                errormsg->Error(pos_, "procedure returns value");
        //            } else {
        //                errormsg->Error(pos_, "function return type mismatch");
        //            }
        //        }
        //        venv->EndScope();
        //    }

        const auto &list = this->functions_->GetList();
        for (auto it = list.begin(); it != list.end(); ++it) {
            FunDec *fun = *it;
            if (venv->Look(fun->name_)) {
                errormsg->Error(pos_, "two functions have the same name");
            }
            type::TyList *formalTyList = fun->params_->MakeFormalTyList(tenv, errormsg);
            if (fun->result_) {
                type::Ty *resultTy = tenv->Look(fun->result_);
                venv->Enter(fun->name_, new env::FunEntry(formalTyList, resultTy));
            } else {
                venv->Enter(fun->name_, new env::FunEntry(formalTyList, type::VoidTy::Instance()));
            }
        }

        for (auto it = list.begin(); it != list.end(); ++it) {
            FunDec *fun = *it;
            type::TyList *formalTyList = fun->params_->MakeFormalTyList(tenv, errormsg);
            venv->BeginScope();

            const auto &fieldList = fun->params_->GetList();
            const auto &tyList = formalTyList->GetList();

            auto it1 = tyList.begin();
            auto it2 = fieldList.begin();
            for (; it2 != fieldList.end(); ++it1, ++it2) {
                venv->Enter((*it2)->name_, new env::VarEntry(*it1));
            }

            type::Ty *ty = fun->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
            env::EnvEntry *entry = venv->Look(fun->name_);
            if (typeid(*entry) != typeid(env::FunEntry) || ty != static_cast<env::FunEntry *>(entry)->result_->ActualTy()) {
                errormsg->Error(pos_, "procedure returns value");
            }
            venv->EndScope();
        }
    }

    void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        if (venv->Look(this->var_)) {
            return;
        }

        auto init_ty = this->init_->SemAnalyze(venv, tenv, labelcount, errormsg);
        if (typ_) {
            auto ty = tenv->Look(typ_);
            if (!ty) {
                errormsg->Error(pos_, "undefined type of %s", this->typ_->Name().data());
            } else {
                if (!ty->IsSameType(init_ty)) {
                    errormsg->Error(pos_, "type mismatch");
                }
                venv->Enter(this->var_, new env::VarEntry(ty));
            }
        } else {
            if (init_ty->IsSameType(type::NilTy::Instance()) && typeid(*(init_ty->ActualTy())) != typeid(type::RecordTy)) {
                errormsg->Error(pos_, "init should not be nil without type specified");
            } else {
                venv->Enter(this->var_, new env::VarEntry(init_ty));
            }
        }
    }

    void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount, err::ErrorMsg *errormsg) const {
        const auto &list = types_->GetList();

        for (auto it = list.begin(); it != list.end(); ++it) {
            absyn::NameAndTy *nt = *it;
            if (tenv->Look(nt->name_)) {
                errormsg->Error(pos_, "two types have the same name");
            } else {
                tenv->Enter(nt->name_, new type::NameTy(nt->name_, nullptr));
            }
        }

        for (auto it = list.begin(); it != list.end(); ++it) {
            absyn::NameAndTy *nt = *it;
            type::Ty *ty = tenv->Look(nt->name_);
            type::NameTy *name_ty = static_cast<type::NameTy *>(ty);

            name_ty->ty_ = nt->ty_->SemAnalyze(tenv, errormsg);
        }

        bool loop = false;
        for (auto it = list.begin(); it != list.end(); it++) {
            absyn::NameAndTy *nt = *it;
            type::Ty *ty = tenv->Look(nt->name_);
            auto t = static_cast<type::NameTy *>(ty)->ty_;
            while (typeid(*t) == typeid(type::NameTy)) {
                auto namety = static_cast<type::NameTy *>(t);
                if (namety->sym_ == nt->name_) {
                    loop = true;
                    break;
                }
                t = namety->ty_;
            }
            if (loop) {
                break;
            }
        }

        if (loop) {
            errormsg->Error(pos_, "illegal type cycle");
        }
    }

    type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        type::Ty *ty = tenv->Look(name_);
        if (!ty) {
            // Check type existence
            errormsg->Error(pos_, "undefined type %s", name_->Name().data());
        } else {
            return ty;
        }

        return DEFAULT_TYPE;
    }

    type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
    }

    type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
        /* TODO: Put your lab4 code here */

        type::Ty *ty = tenv->Look(array_);
        if (!ty) {
            // Check type existence
            errormsg->Error(pos_, "undefined type %s", array_->Name().data());
        } else {
            return new type::ArrayTy(ty);
        }
        return new type::ArrayTy(DEFAULT_TYPE);
    }

} // namespace absyn

namespace sem {

    void ProgSem::SemAnalyze() {
        FillBaseVEnv();
        FillBaseTEnv();
        absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
    }

} // namespace sem
