#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

//#include "tiger/absyn/absyn.h"
#include "tiger/frame/frame.h"
#include "tiger/translate/printtree.h"
#include "tiger/util/util.h"
#include "tiger/translate/tree.h"
#include "tiger/escape/escape.h"
/* Forward Declarations */
namespace A {
    class Exp;
}  // namespace A
namespace TR {


    class PatchList {
    public:
        // address of the NULLS
        TEMP::Label **head;
        PatchList *tail;

        PatchList(TEMP::Label **head, PatchList *tail) : head(head), tail(tail) {}
    };

    class Cx {
    public:
        TR::PatchList *trues;
        TR::PatchList *falses;
        T::Stm *stm;

        Cx(TR::PatchList *trues, TR::PatchList *falses, T::Stm *stm)
                : trues(trues), falses(falses), stm(stm) {}
    };

    class Exp {
    public:
        enum Kind {
            EX, NX, CX
        };

        Kind kind;

        Exp(Kind kind) : kind(kind) {}

        virtual T::Exp *UnEx() const = 0;

        virtual T::Stm *UnNx() const = 0;

        virtual Cx UnCx() const = 0;
    };

    void do_patch(PatchList *tList, TEMP::Label *label);

    PatchList *join_patch(PatchList *first, PatchList *second);

    // p157
    class ExExp : public Exp {
    public:
        T::Exp *exp;

        ExExp(T::Exp *exp) : Exp(EX), exp(exp) {}

        T::Exp *UnEx() const override {
            return exp;
        }

        T::Stm *UnNx() const override {
            return new T::ExpStm(exp);
        }

        Cx UnCx() const override {
            T::Stm *stm_ = new T::CjumpStm(T::NE_OP, exp, new T::ConstExp(0), nullptr, nullptr);
            PatchList *trues = new PatchList(&(((T::CjumpStm *) stm_)->true_label), nullptr);
            PatchList *falses = new PatchList(&(((T::CjumpStm *) stm_)->false_label), nullptr);
            return {trues, falses, stm_};
        }
    };

    class NxExp : public Exp {
    public:
        T::Stm *stm;

        NxExp(T::Stm *stm) : Exp(NX), stm(stm) {}

        T::Exp *UnEx() const override {
            return new T::EseqExp(stm, new T::ConstExp(0));
        }

        T::Stm *UnNx() const override {
            return stm;
        }

        Cx UnCx() const override {
            return {nullptr, nullptr, nullptr};
        }
    };


    class CxExp : public Exp {
    public:
        Cx cx;


        CxExp(TR::PatchList *trues, TR::PatchList *falses, T::Stm *stm)
                : Exp(CX), cx(trues, falses, stm) {}

        CxExp(struct Cx cx) : Exp(CX), cx(cx) {}

        T::Exp *UnEx() const override {
            TEMP::Temp *r = TEMP::Temp::NewTemp();
            TEMP::Label *t = TEMP::NewLabel(), *f = TEMP::NewLabel();
            do_patch(cx.trues, t);
            do_patch(cx.falses, f);
            return new T::EseqExp(new T::MoveStm(new T::TempExp(r), new T::ConstExp(1)),
                                  new T::EseqExp(cx.stm, new T::EseqExp(new T::LabelStm(f),
                                                                        new T::EseqExp(new T::MoveStm(new T::TempExp(r),
                                                                                                      new T::ConstExp(
                                                                                                              0)),
                                                                                       new T::EseqExp(
                                                                                               new T::LabelStm(t),
                                                                                               new T::TempExp(r))))));
        }

        T::Stm *UnNx() const override {
            TEMP::Label *label = TEMP::NewLabel();
            do_patch(cx.trues, label);
            do_patch(cx.falses, label);
            return new T::SeqStm(cx.stm, new T::LabelStm(label));
        }

        Cx UnCx() const override {
            return cx;
        }
    };

    class ExpAndTy;

    class Level;

    Level *Outermost();

    F::FragList *TranslateProgram(A::Exp *);

    static F::FragList *fragList = new F::FragList(nullptr, nullptr), *_tail = fragList;

    T::Stm *procEntryExit1(F::Frame *frame, TR::Exp *body);


    static void addFrag(F::Frag *frag) {
        _tail = _tail->tail = new F::FragList(frag, nullptr);
    }

    static T::Stm *stmList2SeqStm(T::StmList *);

    template<typename t>
    static uint listLen(t *i) {
        uint count = 0;
        while (i) {
            i = i->tail;
            count++;
        }
        return count;
    };

    bool runtime(std::string);
}  // namespace TR

#endif
