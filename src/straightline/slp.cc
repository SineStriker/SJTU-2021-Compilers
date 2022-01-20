#include "straightline/slp.h"

#include <iostream>

namespace A {

    //===========================================================================
    Table::Table(std::string id, int value, const Table *tail)
        : id(std::move(id)), value(value), tail(tail) {
    }

    //---------------------------------------------------------------------------
    int Table::Lookup(const std::string &key) const {
        if (id == key) {
            return value;
        } else if (tail != nullptr) {
            return tail->Lookup(key);
        } else {
            assert(false);
        }
    }

    //---------------------------------------------------------------------------
    Table *Table::Update(const std::string &key, int val) const {
        return new Table(key, val, this);
    }

    //===========================================================================
    IntAndTable::IntAndTable(int i, Table *t) : i(i), t(t) {
    }

    //===========================================================================
    Stm::Stm() {
    }

    //---------------------------------------------------------------------------
    int Stm::MaxArgs() const {
        return 0;
    }

    //---------------------------------------------------------------------------
    Table *Stm::Interp(Table *t) const {
        return t;
    }

    //===========================================================================
    int A::CompoundStm::MaxArgs() const {
        return std::max(stm1->MaxArgs(), stm2->MaxArgs());
    }

    //---------------------------------------------------------------------------
    Table *A::CompoundStm::Interp(Table *t) const {
        t = stm1->Interp(t);
        t = stm2->Interp(t);
        return t;
    }

    //---------------------------------------------------------------------------
    CompoundStm::CompoundStm(Stm *stm1, Stm *stm2) : stm1(stm1), stm2(stm2) {
    }

    //===========================================================================
    AssignStm::AssignStm(std::string id, Exp *exp) : id(std::move(id)), exp(exp) {
    }

    //---------------------------------------------------------------------------
    int A::AssignStm::MaxArgs() const {
        return exp->MaxArgs();
    }

    //---------------------------------------------------------------------------
    Table *A::AssignStm::Interp(Table *t) const {
        IntAndTable it = exp->Interp(t);
        if (t) {
            t = t->Update(id, it.i);
        } else {
            t = new Table(id, it.i, nullptr);
        }

        return t; // Ignore potential memory leak
    }

    //===========================================================================
    PrintStm::PrintStm(ExpList *exps) : exps(exps) {
    }

    //---------------------------------------------------------------------------
    int A::PrintStm::MaxArgs() const {
        return std::max(exps->MaxArgs(), exps->NumExps());
    }

    //---------------------------------------------------------------------------
    Table *PrintStm::Interp(Table *t) const {
        IntAndTable it = exps->Interp(t);
        std::cout << std::endl;
        return it.t;
    }

    //===========================================================================
    Exp::Exp() {
    }

    //---------------------------------------------------------------------------
    int Exp::MaxArgs() const {
        return 0;
    }

    //---------------------------------------------------------------------------
    IntAndTable Exp::Interp(Table *t) const {
        return IntAndTable(0, t);
    }

    //===========================================================================
    IdExp::IdExp(std::string id) : id(std::move(id)) {
    }

    //---------------------------------------------------------------------------
    IntAndTable IdExp::Interp(Table *t) const {
        return IntAndTable(t->Lookup(id), t);
    }

    //===========================================================================
    NumExp::NumExp(int num) : num(num) {
    }

    //---------------------------------------------------------------------------
    IntAndTable NumExp::Interp(Table *t) const {
        return IntAndTable(num, t);
    }

    //===========================================================================
    OpExp::OpExp(Exp *left, BinOp oper, Exp *right) : left(left), oper(oper), right(right) {
    }

    //---------------------------------------------------------------------------
    int OpExp::MaxArgs() const {
        return std::max(left->MaxArgs(), right->MaxArgs());
    }

    //---------------------------------------------------------------------------
    IntAndTable OpExp::Interp(Table *t) const {
        const IntAndTable &it1 = left->Interp(t);
        t = it1.t;
        const IntAndTable &it2 = right->Interp(t);
        t = it2.t;

        int res = 0;
        switch (oper) {
        case PLUS:
            res = it1.i + it2.i;
            break;
        case MINUS:
            res = it1.i - it2.i;
            break;
        case TIMES:
            res = it1.i * it2.i;
            break;
        default:
            res = int(double(it1.i) / it2.i);
            break;
        }

        return IntAndTable(res, t);
    }

    //===========================================================================
    EseqExp::EseqExp(Stm *stm, Exp *exp) : stm(stm), exp(exp) {
    }

    //===========================================================================
    int EseqExp::MaxArgs() const {
        return std::max(exp->MaxArgs(), stm->MaxArgs());
    }

    //===========================================================================
    IntAndTable EseqExp::Interp(Table *t) const {
        t = stm->Interp(t);
        return exp->Interp(t);
    }

    //------------------------------------------------------max---------------------
    ExpList::ExpList() {
    }

    //---------------------------------------------------------------------------
    int ExpList::MaxArgs() const {
        return 0;
    }

    //---------------------------------------------------------------------------
    int ExpList::NumExps() const {
        return 0;
    }

    //---------------------------------------------------------------------------
    IntAndTable ExpList::Interp(Table *t) const {
        return IntAndTable(0, t);
    }

    //===========================================================================
    PairExpList::PairExpList(Exp *exp, ExpList *tail) : exp(exp), tail(tail) {
    }

    //---------------------------------------------------------------------------
    int PairExpList::MaxArgs() const {
        return std::max(exp->MaxArgs(), tail->MaxArgs());
    }

    //---------------------------------------------------------------------------
    int PairExpList::NumExps() const {
        return 1 + tail->NumExps();
    }

    //---------------------------------------------------------------------------
    IntAndTable PairExpList::Interp(Table *t) const {
        IntAndTable it = exp->Interp(t);
        t = it.t;

        std::cout << it.i << " ";

        return tail->Interp(t);
    }

    //===========================================================================
    LastExpList::LastExpList(Exp *exp) : exp(exp) {
    }

    //---------------------------------------------------------------------------
    int LastExpList::MaxArgs() const {
        return exp->MaxArgs();
    }

    //---------------------------------------------------------------------------
    int LastExpList::NumExps() const {
        return 1;
    }

    //---------------------------------------------------------------------------
    IntAndTable LastExpList::Interp(Table *t) const {
        IntAndTable it = exp->Interp(t);
        std::cout << it.i;
        return it;
    }

} // namespace A
