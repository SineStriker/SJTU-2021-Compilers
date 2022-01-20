#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

#define QUICK_ESCAPE(SYM, ENV, DEPTH)                                                                                                                          \
    esc::EscapeEntry *entry = ENV->Look(SYM);                                                                                                                  \
    if (entry) {                                                                                                                                               \
        if (!(*entry->escape_) && entry->depth_ < DEPTH) {                                                                                                     \
            *entry->escape_ = true;                                                                                                                            \
        }                                                                                                                                                      \
    }

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
    /* TODO: Put your lab5 code here */

    int depth = 0;
    root_->Traverse(env, depth);
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    QUICK_ESCAPE(sym_, env, depth)
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    var_->Traverse(env, depth);
    QUICK_ESCAPE(sym_, env, depth)
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    var_->Traverse(env, depth);
    subscript_->Traverse(env, depth);
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    var_->Traverse(env, depth);
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) { /* TODO: Put your lab5 code here */
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) { /* TODO: Put your lab5 code here */
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) { /* TODO: Put your lab5 code here */
}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    const auto &args = this->args_->GetList();

    for (auto it = args.begin(); it != args.end(); ++it) {
        (*it)->Traverse(env, depth);
    }
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    left_->Traverse(env, depth);
    right_->Traverse(env, depth);
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    const auto &fields = this->fields_->GetList();

    for (auto it = fields.begin(); it != fields.end(); ++it) {
        (*it)->exp_->Traverse(env, depth);
    }
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    const auto &seq = this->seq_->GetList();

    for (auto it = seq.begin(); it != seq.end(); ++it) {
        (*it)->Traverse(env, depth);
    }
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    var_->Traverse(env, depth);
    exp_->Traverse(env, depth);
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    test_->Traverse(env, depth);
    then_->Traverse(env, depth);
    if (elsee_) {
        elsee_->Traverse(env, depth);
    }
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    test_->Traverse(env, depth);
    if (body_) {
        body_->Traverse(env, depth);
    }
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    lo_->Traverse(env, depth);
    hi_->Traverse(env, depth);

    if (body_) {
        env->BeginScope();
        // depth++;

        escape_ = false;
        env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
        body_->Traverse(env, depth);

        env->EndScope();
        // depth--;
    }
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) { /* TODO: Put your lab5 code here */
}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    env->BeginScope();
    // depth++;

    const auto &decs = this->decs_->GetList();

    for (auto it = decs.begin(); it != decs.end(); ++it) {
        (*it)->Traverse(env, depth);
    }

    if (body_) {
        body_->Traverse(env, depth);
    }

    env->EndScope();
    // depth--;
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    size_->Traverse(env, depth);
    init_->Traverse(env, depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) { /* TODO: Put your lab5 code here */
}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    const auto &funs = this->functions_->GetList();

    for (auto it = funs.begin(); it != funs.end(); it++) {
        env->BeginScope();
        depth++;

        auto fun = *it;
        const auto &actuals = fun->params_->GetList();
        for (auto it2 = actuals.begin(); it2 != actuals.end(); it2++) {
            Field *field = *it2;
            field->escape_ = false;
            env->Enter(field->name_, new esc::EscapeEntry(depth, &field->escape_));
        }

        if (fun->body_) {
            fun->body_->Traverse(env, depth);
        }

        env->EndScope();
        depth--;
    }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    init_->Traverse(env, depth);

    // if (typeid(*init_) == typeid(ArrayExp)) {
    //     escape_ = true;
    // }

    escape_ = false;
    env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
    /* TODO: Put your lab5 code here */

    const auto &types = this->types_->GetList();

    for (auto it = types.begin(); it != types.end(); it++) {
        Ty *type = (*it)->ty_;

        if (typeid(*type) == typeid(RecordTy)) {

            RecordTy *rec = static_cast<RecordTy *>(type);
            const auto &fields = rec->record_->GetList();
            for (auto it2 = fields.begin(); it2 != fields.end(); it2++) {
                Field *field = *it2;
                field->escape_ = true;
            }
        }
    }
}

} // namespace absyn
