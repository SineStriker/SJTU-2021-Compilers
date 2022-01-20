#ifndef STRAIGHTLINE_SLP_H_
#define STRAIGHTLINE_SLP_H_

#include <algorithm>
#include <cassert>
#include <list>
#include <string>

namespace A {

    class Exp;

    class ExpList;

    enum BinOp { PLUS = 0, MINUS, TIMES, DIV };

    //===========================================================================
    class Table {
    public:
        Table(std::string id, int value, const Table *tail);

        int Lookup(const std::string &key) const;
        Table *Update(const std::string &key, int val) const;

    private:
        std::string id;
        int value;
        const Table *tail;
    };

    //===========================================================================
    struct IntAndTable {
        int i;
        Table *t;

        IntAndTable(int i, Table *t);
    };

    //===========================================================================
    class Stm {
    public:
        Stm();

        virtual int MaxArgs() const;
        virtual Table *Interp(Table *t) const;
    };

    //===========================================================================
    class CompoundStm : public Stm {
    public:
        CompoundStm(Stm *stm1, Stm *stm2);

        int MaxArgs() const override;
        Table *Interp(Table *) const override;

    private:
        Stm *stm1, *stm2;
    };

    //===========================================================================
    class AssignStm : public Stm {
    public:
        AssignStm(std::string id, Exp *exp);

        int MaxArgs() const override;
        Table *Interp(Table *) const override;

    private:
        std::string id;
        Exp *exp;
    };

    //===========================================================================
    class PrintStm : public Stm {
    public:
        explicit PrintStm(ExpList *exps);

        int MaxArgs() const override;
        Table *Interp(Table *t) const override;

    private:
        ExpList *exps;
    };

    //===========================================================================
    class Exp {
    public:
        Exp();

        virtual int MaxArgs() const;

        virtual IntAndTable Interp(Table *t) const;
    };

    //===========================================================================
    class IdExp : public Exp {
    public:
        explicit IdExp(std::string id);

        IntAndTable Interp(Table *t) const override;

    private:
        std::string id;
    };

    //===========================================================================
    class NumExp : public Exp {
    public:
        explicit NumExp(int num);

        IntAndTable Interp(Table *t) const override;

    private:
        int num;
    };

    //===========================================================================
    class OpExp : public Exp {
    public:
        OpExp(Exp *left, BinOp oper, Exp *right);

        int MaxArgs() const override;
        IntAndTable Interp(Table *t) const override;

    private:
        Exp *left;
        BinOp oper;
        Exp *right;
    };

    //===========================================================================
    class EseqExp : public Exp {
    public:
        EseqExp(Stm *stm, Exp *exp);

        int MaxArgs() const override;
        IntAndTable Interp(Table *t) const override;

    private:
        Stm *stm;
        Exp *exp;
    };

    //===========================================================================
    class ExpList {
    public:
        ExpList();

        virtual int MaxArgs() const;
        virtual int NumExps() const;
        virtual IntAndTable Interp(Table *t) const;
    };

    //===========================================================================
    class PairExpList : public ExpList {
    public:
        PairExpList(Exp *exp, ExpList *tail);

        int MaxArgs() const override;
        int NumExps() const override;
        IntAndTable Interp(Table *t) const override;

    private:
        Exp *exp;
        ExpList *tail;
    };

    //===========================================================================
    class LastExpList : public ExpList {
    public:
        LastExpList(Exp *exp);

        int MaxArgs() const override;
        int NumExps() const override;
        IntAndTable Interp(Table *t) const override;

    private:
        Exp *exp;
    };

} // namespace A

#endif // STRAIGHTLINE_SLP_H_
