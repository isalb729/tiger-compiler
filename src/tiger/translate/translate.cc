#include "tiger/translate/translate.h"
#include <iostream>
#include <cstdio>
#include <set>
#include <string>
#include "tiger/errormsg/errormsg.h"
#include "tiger/semant/semant.h"

#define p(s, ...) do { \
  printf(s, ##__VA_ARGS__); \
  fflush(stdout); \
  } while (0);
extern EM::ErrorMsg errormsg;
namespace {
    static TY::TyList *make_formal_tylist(S::Table<TY::Ty> *tenv, A::FieldList *params) {
        if (params == nullptr) {
            return nullptr;
        }

        TY::Ty *ty = tenv->Look(params->head->typ);
        if (ty == nullptr) {
            errormsg.Error(params->head->pos, "undefined type %s",
                           params->head->typ->Name().c_str());
        }

        return new TY::TyList(ty->ActualTy(), make_formal_tylist(tenv, params->tail));
    }

    static TY::FieldList *make_fieldlist(S::Table<TY::Ty> *tenv, A::FieldList *fields) {
        if (fields == nullptr) {
            return nullptr;
        }

        TY::Ty *ty = tenv->Look(fields->head->typ);
        return new TY::FieldList(new TY::Field(fields->head->name, ty),
                                 make_fieldlist(tenv, fields->tail));
    }

    static U::BoolList *make_boollist(A::FieldList *fieldList) {
        // not include the static link
        U::BoolList *boolList = new U::BoolList(true, nullptr), *tail = boolList;
        while (fieldList) {
            tail = tail->tail = new U::BoolList(fieldList->head->escape, nullptr);
            fieldList = fieldList->tail;
        }
        return boolList->tail;
    }

}  // namespace
namespace TR {


    class Access {
    public:
        Level *level;
        // x64frame, inframaccess, inregaccess
        F::Access *access;

        Access(Level *level, F::Access *access) : level(level), access(access) {}

        static Access *AllocLocal(Level *level, bool escape);
    };

    class AccessList {
    public:
        Access *head;
        AccessList *tail;

        AccessList(Access *head, AccessList *tail) : head(head), tail(tail) {}
    };

    class Level {
    public:
        F::Frame *frame;
        Level *parent;

        Level(F::Frame *frame, Level *parent) : frame(frame), parent(parent) {}

        static AccessList *Formals(Level *level) {
            F::AccessList *fAccessList = level->frame->formals;
            AccessList *accessList = new AccessList(nullptr, nullptr), *tail = accessList;
            for (; fAccessList; fAccessList = fAccessList->tail) {
                Access *access = new Access(level, fAccessList->head);
                tail = tail->tail = new AccessList(access, nullptr);
            }
            return accessList->tail;
        }

        // include static link
        static Level *NewLevel(Level *parent, TEMP::Label *name,
                               U::BoolList *formals) {
            U::BoolList *escape;
            if (parent) {
                escape = new U::BoolList(true, formals);
            } else {
                escape = formals;
            }
            F::Frame *frame_ = new F::X64Frame(name, escape);
            Level *level = new Level(frame_, parent);
            // static link added later
            return level;
        };
    };

    Access *Access::AllocLocal(Level *level, bool escape) {
        Access *access_ = new Access(level,
                                     ((F::X64Frame *) level->frame)->allocal(escape));
        return access_;
    }


    class ExpAndTy {
    public:
        TR::Exp *exp;
        TY::Ty *ty;

        ExpAndTy(TR::Exp *exp, TY::Ty *ty) : exp(exp), ty(ty) {}
    };


    void do_patch(PatchList *tList, TEMP::Label *label) {
        for (; tList; tList = tList->tail) *(tList->head) = label;
    }

    PatchList *join_patch(PatchList *first, PatchList *second) {
        if (!first) return second;
        for (; first->tail; first = first->tail);
        first->tail = second;
        return first;
    }

    // parent of the first level
    Level *Outermost() {
        static Level *lv = nullptr;
        if (lv != nullptr) return lv;

        lv = new Level(nullptr, nullptr);
        return lv;
    }


    F::FragList *TranslateProgram(A::Exp *root) {
        if (root) {
            Level *level = Level::NewLevel(Outermost(), TEMP::NamedLabel("tigermain"), nullptr);
            TR::ExpAndTy expAndTy = root->Translate(E::BaseVEnv(), E::BaseTEnv(), level, TEMP::NewLabel());
            T::Stm *stm = expAndTy.exp->UnNx();
            F::Frag *procFrag = new F::ProcFrag(stm, level->frame);
            addFrag(procFrag);
        }
        return fragList->tail;
    }

    T::Stm *procEntryExit1(F::Frame *frame, TR::Exp *body) {
        if (body->kind == TR::Exp::NX) {
            T::Stm *stm = body->UnNx();
            F::Frag *frag = new F::ProcFrag(stm, frame);
            addFrag(frag);
            return stm;
        } else {
            T::Stm *stm = body->UnNx();
            F::Frag *frag = new F::ProcFrag(
                    new T::MoveStm(new T::TempExp(F::RV()), body->UnEx()), frame);
            addFrag(frag);
            return stm;
        }
    }

    T::Stm *stmList2SeqStm(T::StmList *stmList) {
        if (!stmList or !stmList->head) {
            return nullptr;
        } else {
            if (!stmList->tail) {
                return stmList->head;
            } else {
                T::Stm *seqStm = new T::SeqStm(stmList->head, nullptr),
                        *tail = seqStm;
                while (stmList->tail) {
                    stmList = stmList->tail;
                    if (!stmList->tail) {
                        ((T::SeqStm *) tail)->right = stmList->head;
                    } else {
                        tail = ((T::SeqStm *) tail)->right = new T::SeqStm(stmList->head, nullptr);
                    }
                }
                return seqStm;
            }
        }
    }

    bool runtime(std::string name) {
        return name == "initArray" || name == "allocRecord" || name == "stringEqual" || name == "print" ||
               name == "printi" || name == "flush" || name == "ord" || name == "chr" || name == "size" ||
               name == "substring" || name == "concat" || name == "not";
    }

}  // namespace TR

namespace A {

    TR::ExpAndTy SimpleVar::Translate(S::Table<E::EnvEntry> *venv,
                                      S::Table<TY::Ty> *tenv, TR::Level *level,
                                      TEMP::Label *label) const {
        //p("Translate: simplevar\n")
        E::EnvEntry *ve = venv->Look(this->sym);
        T::Exp *framePtr = new T::TempExp(F::FP());
        if (ve && ve->kind == E::EnvEntry::VAR) {
            while (((E::VarEntry *) ve)->access->level != level) {
                framePtr = level->frame->formals->head->toExp(framePtr);
                level = level->parent;
            }

            T::Exp *tExp = ((E::VarEntry *) ve)->access->access->toExp(framePtr);
            TR::Exp *trExp = new TR::ExExp(tExp);
            return TR::ExpAndTy(trExp, ((E::VarEntry *) ve)->ty->ActualTy());
        } else {
            errormsg.Error(this->pos, "undefined variable %s", this->sym->Name().c_str());
            return TR::ExpAndTy(nullptr, TY::IntTy::Instance());
        }
    }

    TR::ExpAndTy FieldVar::Translate(S::Table<E::EnvEntry> *venv,
                                     S::Table<TY::Ty> *tenv, TR::Level *level,
                                     TEMP::Label *label) const {
        //p("Translate: fieldvar\n");
        TR::ExpAndTy expAndTy = this->var->Translate(venv, tenv, level, label);
        TY::Ty *ty = expAndTy.ty;
        TR::Exp *exp = expAndTy.exp;
        if (ty->kind != TY::Ty::RECORD) {
            errormsg.Error(this->pos, "not a record type");
            return TR::ExpAndTy(nullptr, TY::IntTy::Instance());
        } else {
            TY::FieldList *fieldList = ((TY::RecordTy *) ty)->fields;
            uint32_t pos = 0;
            while (fieldList) {
                if (fieldList->head->name == this->sym) {
                    TR::ExExp *trExp = new TR::ExExp(
                            new T::MemExp(new T::BinopExp(T::PLUS_OP, exp->UnEx(), new T::ConstExp(pos))));
                    return TR::ExpAndTy(trExp, fieldList->head->ty->ActualTy());
                }
                pos += 8;
                fieldList = fieldList->tail;
            }
            errormsg.Error(this->pos, "field %s doesn't exist", this->sym->Name().c_str());
            return TR::ExpAndTy(nullptr, TY::IntTy::Instance());
        }
    }

    TR::ExpAndTy SubscriptVar::Translate(S::Table<E::EnvEntry> *venv,
                                         S::Table<TY::Ty> *tenv, TR::Level *level,
                                         TEMP::Label *label) const {
        //p("Translate: subscriptvar\n");
        TR::ExpAndTy expAndTy = this->var->Translate(venv, tenv, level, label);
        TY::Ty *ty = expAndTy.ty;
        if (ty->kind != TY::Ty::ARRAY) {
            errormsg.Error(this->pos, "array type required");
            return TR::ExpAndTy(nullptr, TY::IntTy::Instance());
        } else {
            TR::ExpAndTy expAndTy1 = this->subscript->Translate(venv, tenv, level, label);
            TY::Ty *ty1 = expAndTy1.ty;
            if (ty1->kind != TY::Ty::INT) {
                errormsg.Error(this->pos, "index type is not int");
                return TR::ExpAndTy(nullptr, TY::IntTy::Instance());
            }
            TR::ExExp *exExp = new TR::ExExp(
                    new T::MemExp(new T::BinopExp(T::PLUS_OP, expAndTy.exp->UnEx(), new T::BinopExp(T::MUL_OP, expAndTy1.exp->UnEx(), new T::ConstExp(8)))));
            return TR::ExpAndTy(exExp, ((TY::ArrayTy *) ty)->ty->ActualTy());
        }
    }

    TR::ExpAndTy VarExp::Translate(S::Table<E::EnvEntry> *venv,
                                   S::Table<TY::Ty> *tenv, TR::Level *level,
                                   TEMP::Label *label) const {
        //p("Translate: varexp\n");
        return this->var->Translate(venv, tenv, level, label);
    }

    TR::ExpAndTy NilExp::Translate(S::Table<E::EnvEntry> *venv,
                                   S::Table<TY::Ty> *tenv, TR::Level *level,
                                   TEMP::Label *label) const {
        //p("Translate: nilexp\n");
        return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::NilTy::Instance());
    }

    TR::ExpAndTy IntExp::Translate(S::Table<E::EnvEntry> *venv,
                                   S::Table<TY::Ty> *tenv, TR::Level *level,
                                   TEMP::Label *label) const {
        //p("Translate: intexp\n");
        return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(this->i)), TY::IntTy::Instance());
    }

    TR::ExpAndTy StringExp::Translate(S::Table<E::EnvEntry> *venv,
                                      S::Table<TY::Ty> *tenv, TR::Level *level,
                                      TEMP::Label *label) const {
        //p("Translate: stringexp\n")
        TEMP::Label *label_ = TEMP::NewLabel();
        F::FragList *f = TR::fragList->tail;
        F::Frag *frag = new F::StringFrag(label_, this->s);
        TR::addFrag(frag);
        return TR::ExpAndTy(new TR::ExExp(new T::NameExp(label_)), TY::StringTy::Instance());
    }

    TR::ExpAndTy CallExp::Translate(S::Table<E::EnvEntry> *venv,
                                    S::Table<TY::Ty> *tenv, TR::Level *level,
                                    TEMP::Label *label) const {
        //p("Translate: callexp\n");
        E::EnvEntry *funEntry = venv->Look(this->func);
        T::ExpList *expList_ = new T::ExpList(nullptr, nullptr), *tail = expList_;
        if (!funEntry || funEntry->kind != E::EnvEntry::FUN) {
            errormsg.Error(this->pos, "undefined function %s", this->func->Name().c_str());
            return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
        } else {
            TY::TyList *formals = ((E::FunEntry *) funEntry)->formals;
            ExpList *expList;
            for (expList = this->args; expList && formals; expList = expList->tail, formals = formals->tail) {
                TR::ExpAndTy expAndTy = expList->head->Translate(venv, tenv, level, label);
                TY::Ty *ty = expAndTy.ty;
                if (!formals->head->IsSameType(ty)) {
                    errormsg.Error(expList->head->pos, "para type mismatch");
                }
                tail = tail->tail = new T::ExpList(expAndTy.exp->UnEx(), nullptr);
            }
            if (expList) {
                errormsg.Error(expList->head->pos, "too many params in function %s", this->func->Name().c_str());
            }
            if (formals) {
                errormsg.Error(this->pos, "too few params");
            }
        }
        T::Exp *frame = new T::TempExp(F::FP());
        if (((E::FunEntry *) funEntry)->level == TR::Outermost()) {
            expList_->head = frame;
            return TR::ExpAndTy(new TR::ExExp(F::ExternalCall(TEMP::LabelString(this->func), expList_)),
                                ((E::FunEntry *) funEntry)->result->ActualTy());
        }
        while (((E::FunEntry *) funEntry)->level->parent != level) {
            frame = level->frame->formals->head->toExp(frame);
            level = level->parent;
        }
        expList_->head = frame;
        return TR::ExpAndTy(new TR::ExExp(new T::CallExp(new T::NameExp(((E::FunEntry *) funEntry)->label), expList_)),
                            ((E::FunEntry *) funEntry)->result->ActualTy());
    }

    TR::ExpAndTy OpExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
        //p("Translate: opexp\n");
        TR::ExpAndTy expAndTy1 = this->left->Translate(venv, tenv, level, label);
        TR::ExpAndTy expAndTy2 = this->right->Translate(venv, tenv, level, label);
        TY::Ty *ty1 = expAndTy1.ty;
        TY::Ty *ty2 = expAndTy2.ty;
        if (this->oper == A::PLUS_OP || this->oper == A::MINUS_OP || this->oper == A::TIMES_OP ||
            this->oper == A::DIVIDE_OP) {
            if (ty1->kind != TY::Ty::INT) {
                errormsg.Error(this->left->pos, "integer required");
            }
            if (ty2->kind != TY::Ty::INT) {
                errormsg.Error(this->right->pos, "integer required");
            }
        } else {
            if (!ty1->IsSameType(ty2)) {
                errormsg.Error(this->right->pos, "same type required");
            }
        }
        T::BinOp binOp;
        T::RelOp relOp;
        T::Stm *stm;
        if (expAndTy1.exp->UnEx()->kind == T::Exp::NAME && expAndTy2.exp->UnEx()->kind == T::Exp::NAME) {
            T::Exp *call = F::ExternalCall("stringEqual", new T::ExpList(level->frame->formals->head->toExp(new T::TempExp(F::FP())), new T::ExpList(expAndTy1.exp->UnEx(), new T::ExpList(expAndTy2.exp->UnEx(),
                                                                                                           nullptr))));
            switch (oper) {
                case EQ_OP:
                    stm = new T::CjumpStm(T::EQ_OP, call, new T::ConstExp(1), nullptr, nullptr);
                    break;
                case NEQ_OP:
                    stm = new T::CjumpStm(T::EQ_OP, call, new T::ConstExp(0), nullptr, nullptr);
                    break;
                default:
                    assert(0);
            }
        } else {
            switch (oper) {
                case PLUS_OP:
                    binOp = T::PLUS_OP;
                    return TR::ExpAndTy(
                            new TR::ExExp(new T::BinopExp(binOp, expAndTy1.exp->UnEx(), expAndTy2.exp->UnEx())),
                            TY::IntTy::Instance());
                case MINUS_OP:
                    binOp = T::MINUS_OP;
                    return TR::ExpAndTy(
                            new TR::ExExp(new T::BinopExp(binOp, expAndTy1.exp->UnEx(), expAndTy2.exp->UnEx())),
                            TY::IntTy::Instance());
                case TIMES_OP:
                    binOp = T::MUL_OP;
                    return TR::ExpAndTy(
                            new TR::ExExp(new T::BinopExp(binOp, expAndTy1.exp->UnEx(), expAndTy2.exp->UnEx())),
                            TY::IntTy::Instance());
                case DIVIDE_OP:
                    binOp = T::DIV_OP;
                    return TR::ExpAndTy(
                            new TR::ExExp(new T::BinopExp(binOp, expAndTy1.exp->UnEx(), expAndTy2.exp->UnEx())),
                            TY::IntTy::Instance());
                case EQ_OP:
                    relOp = T::EQ_OP;
                    break;
                case NEQ_OP:
                    relOp = T::NE_OP;
                    break;
                case GE_OP:
                    relOp = T::GE_OP;
                    break;
                case GT_OP:
                    relOp = T::GT_OP;
                    break;
                case LT_OP:
                    relOp = T::LT_OP;
                    break;
                case LE_OP:
                    relOp = T::LE_OP;
                    break;
            }
            stm = new T::CjumpStm(relOp, expAndTy1.exp->UnEx(), expAndTy2.exp->UnEx(), nullptr, nullptr);
        }
        TR::PatchList *trues = new TR::PatchList(&(((T::CjumpStm *) stm)->true_label), nullptr);
        TR::PatchList *falses = new TR::PatchList(&(((T::CjumpStm *) stm)->false_label), nullptr);
        return TR::ExpAndTy(new TR::CxExp(trues, falses, stm), TY::IntTy::Instance());
    }

    TR::ExpAndTy RecordExp::Translate(S::Table<E::EnvEntry> *venv,
                                      S::Table<TY::Ty> *tenv, TR::Level *level,
                                      TEMP::Label *label) const {
        //p("Translate: recordexp\n")

        TY::Ty *ty = tenv->Look(this->typ);
        int count = 0;
        TEMP::Temp *r = TEMP::Temp::NewTemp();
        // TODO: REMOVE ADDTEMP
        T::Stm *stm_ = new T::SeqStm(nullptr, nullptr);
        T::StmList *stmList = new T::StmList(nullptr, nullptr), *tail = stmList;
        if (!ty) {
            errormsg.Error(this->pos, "undefined type %s", this->typ->Name().c_str());
            return TR::ExpAndTy(nullptr, TY::IntTy::Instance());
        } else if (ty->ActualTy()->kind != TY::Ty::RECORD) {
            errormsg.Error(this->pos, "not a record type");
            return TR::ExpAndTy(nullptr, ty->ActualTy());
        } else {
            TY::FieldList *fieldList = ((TY::RecordTy *) ty)->fields;
            EFieldList *eFieldList = this->fields;
            while (eFieldList) {
                TR::ExpAndTy expAndTy = eFieldList->head->exp->Translate(venv, tenv, level, label);
                TY::Ty *ty1 = expAndTy.ty;
                if (!ty1->IsSameType(fieldList->head->ty)) {
                    errormsg.Error(this->pos, "record type unmatched");
                }
                fieldList = fieldList->tail;
                eFieldList = eFieldList->tail;
                tail = tail->tail = new T::StmList(new T::MoveStm(
                        new T::MemExp(
                                new T::BinopExp(T::PLUS_OP, new T::TempExp(r), new T::ConstExp(count * 8))),
                        expAndTy.exp->UnEx()), nullptr);
                count++;
            }
            stmList = stmList->tail;
            stm_ = TR::stmList2SeqStm(stmList);
            if (fieldList) {
                errormsg.Error(this->pos, "too few efields");
            }
        }
        T::Stm *stm = new T::MoveStm(new T::TempExp(r),
                                     new T::CallExp(new T::NameExp(TEMP::NamedLabel("allocRecord")),
                                                    new T::ExpList(new T::ConstExp(8 * count),
                                                                   nullptr)));
        if (stm_) {
            stm_ = new T::SeqStm(stm, stm_);
        } else {
            stm_ = stm;
        }
        return TR::ExpAndTy(new TR::ExExp(new T::EseqExp(stm_, new T::TempExp(r))), ty->ActualTy());
    }

    TR::ExpAndTy SeqExp::Translate(S::Table<E::EnvEntry> *venv,
                                   S::Table<TY::Ty> *tenv, TR::Level *level,
                                   TEMP::Label *label) const {
        //p("Translate: seqexp\n");
        ExpList *expList = this->seq;
        TY::Ty *ty;
        if (!expList) {
            //p("NO EXPRESSIONLIST??\n")
            return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
        } else {
            TR::ExpAndTy expAndTy = expList->head->Translate(venv, tenv, level, label);
            ty = expAndTy.ty;
            T::Exp *eseqExp = new T::EseqExp(expAndTy.exp->UnNx(), nullptr);
            T::Exp *exp = eseqExp;
            expList = expList->tail;
            while (expList) {
                expAndTy = expList->head->Translate(venv, tenv, level, label);
                ty = expAndTy.ty;
                expList = expList->tail;
                if (expList) {
                    ((T::EseqExp *) exp)->exp = new T::EseqExp(expAndTy.exp->UnNx(), nullptr);
                    exp = (((T::EseqExp *) exp)->exp);
                } else {
                    ((T::EseqExp *) exp)->exp = expAndTy.exp->UnEx();
                }
            }
            if (!((T::EseqExp *) eseqExp)->exp) {
                return TR::ExpAndTy(new TR::ExExp(expAndTy.exp->UnEx()), ty);
            }
            return TR::ExpAndTy(new TR::ExExp(eseqExp), ty);
        }
    }

    TR::ExpAndTy AssignExp::Translate(S::Table<E::EnvEntry> *venv,
                                      S::Table<TY::Ty> *tenv, TR::Level *level,
                                      TEMP::Label *label) const {
        //p("Translate: assignexp\n");
        if (var->kind == A::Var::SIMPLE) {
            E::EnvEntry *varEntry = venv->Look(((SimpleVar *) var)->sym);
            if (varEntry->readonly) {
                errormsg.Error(this->pos, "loop variable can't be assigned");
            }
        }
        TR::ExpAndTy expAndTy1 = exp->Translate(venv, tenv, level, label);
        TR::ExpAndTy expAndTy2 = var->Translate(venv, tenv, level, label);
        TY::Ty *ty1 = expAndTy1.ty;
        TY::Ty *ty2 = expAndTy2.ty;
        if (!ty1->IsSameType(ty2)) {
            errormsg.Error(this->pos, "unmatched assign exp");
        }
        return TR::ExpAndTy(new TR::NxExp(new T::MoveStm(expAndTy2.exp->UnEx(), expAndTy1.exp->UnEx())),
                            TY::VoidTy::Instance());
    }

    TR::ExpAndTy IfExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
//        p("Translate: ifexp\n");
        TR::ExpAndTy expAndTy1 = test->Translate(venv, tenv, level, label);
        TR::ExpAndTy expAndTy2 = then->Translate(venv, tenv, level, label);
        TY::Ty *ty1 = expAndTy1.ty;
        TY::Ty *ty2 = expAndTy2.ty;
        TR::Cx cx = expAndTy1.exp->UnCx();
        TEMP::Temp *r = TEMP::Temp::NewTemp();
        TEMP::Label *true_label = TEMP::NewLabel();
        TEMP::Label *false_label = TEMP::NewLabel();
        TEMP::Label *done_label = TEMP::NewLabel();
        TR::do_patch(cx.trues, true_label);
        TR::do_patch(cx.falses, false_label);
        if (elsee) {
            TR::ExpAndTy expAndTy = elsee->Translate(venv, tenv, level, label);
            TY::Ty *ty3 = expAndTy.ty;
            if (!ty3->IsSameType(ty2)) {
                errormsg.Error(this->pos, "then exp and else exp type mismatch");
                return TR::ExpAndTy(nullptr, ty2);
            }
            if (ty2->kind != TY::Ty::VOID) {
                return TR::ExpAndTy(new TR::ExExp(new T::EseqExp(cx.stm, new T::EseqExp(new T::LabelStm(true_label),
                                                                                        new T::EseqExp(new T::MoveStm(
                                                                                                new T::TempExp(r),
                                                                                                expAndTy2.exp->UnEx()),
                                                                                                       new T::EseqExp(
                                                                                                               new T::JumpStm(
                                                                                                                       new T::NameExp(
                                                                                                                               done_label),
                                                                                                                       new TEMP::LabelList(
                                                                                                                               done_label,
                                                                                                                               nullptr)),
                                                                                                               new T::EseqExp(
                                                                                                                       new T::LabelStm(
                                                                                                                               false_label),
                                                                                                                       new T::EseqExp(
                                                                                                                               new T::MoveStm(
                                                                                                                                       new T::TempExp(
                                                                                                                                               r),
                                                                                                                                       expAndTy.exp->UnEx()),
                                                                                                                               new T::EseqExp(
                                                                                                                                       new T::LabelStm(
                                                                                                                                               done_label),
                                                                                                                                       new T::TempExp(
                                                                                                                                               r))))))))),
                                    ty2);
            } else {
                return TR::ExpAndTy(new TR::NxExp(new T::SeqStm(cx.stm, new T::SeqStm(new T::LabelStm(true_label),
                                                                                      new T::SeqStm(
                                                                                              expAndTy2.exp->UnNx(),
                                                                                              new T::SeqStm(
                                                                                                      new T::JumpStm(
                                                                                                              new T::NameExp(
                                                                                                                      done_label),
                                                                                                              new TEMP::LabelList(
                                                                                                                      done_label,
                                                                                                                      nullptr)),
                                                                                                      new T::SeqStm(
                                                                                                              new T::LabelStm(
                                                                                                                      false_label),
                                                                                                              new T::SeqStm(
                                                                                                                      expAndTy.exp->UnNx(),
                                                                                                                      new T::LabelStm(
                                                                                                                              done_label)))))))),
                                    TY::VoidTy::Instance());
            }
        } else {
            if (ty2->kind != TY::Ty::VOID) {
                errormsg.Error(this->pos, "if-then exp's body must produce no value");
                return TR::ExpAndTy(nullptr, ty2);
            }
            done_label = false_label;

            return TR::ExpAndTy(new TR::NxExp(new T::SeqStm(cx.stm, new T::SeqStm(new T::LabelStm(true_label),
                                                                                  new T::SeqStm(expAndTy2.exp->UnNx(),
                                                                                                new T::SeqStm(
                                                                                                        new T::JumpStm(
                                                                                                                new T::NameExp(
                                                                                                                        done_label),
                                                                                                                new TEMP::LabelList(
                                                                                                                        done_label,
                                                                                                                        nullptr)),
                                                                                                        new T::LabelStm(
                                                                                                                done_label)))))),
                                ty2);
        }
    }

    TR::ExpAndTy WhileExp::Translate(S::Table<E::EnvEntry> *venv,
                                     S::Table<TY::Ty> *tenv, TR::Level *level,
                                     TEMP::Label *label) const {
//        p("Translate: whileexp\n");
        TR::ExpAndTy expAndTy = test->Translate(venv, tenv, level, label);
        TEMP::Label *done_label = TEMP::NewLabel();
        TY::Ty *ty = expAndTy.ty;
        // if there is a break
        TR::ExpAndTy expAndTy1 = body->Translate(venv, tenv, level, done_label);
        TY::Ty *ty1 = expAndTy1.ty;
        if (ty1->kind != TY::Ty::VOID) {
            errormsg.Error(pos, "while body must produce no value");
        }
        TR::Cx cx = expAndTy.exp->UnCx();
        TEMP::Label *true_label = TEMP::NewLabel();
        TEMP::Label *begin_label = TEMP::NewLabel();
        TR::do_patch(cx.trues, true_label);
        TR::do_patch(cx.falses, done_label);
        return TR::ExpAndTy(new TR::NxExp(new T::SeqStm(new T::LabelStm(begin_label), new T::SeqStm(cx.stm,
                                                                                                    new T::SeqStm(
                                                                                                            new T::LabelStm(
                                                                                                                    true_label),
                                                                                                            new T::SeqStm(
                                                                                                                    expAndTy1.exp->UnNx(),
                                                                                                                    new T::SeqStm(
                                                                                                                            new T::JumpStm(
                                                                                                                                    new T::NameExp(
                                                                                                                                            begin_label),
                                                                                                                                    new TEMP::LabelList(
                                                                                                                                            begin_label,
                                                                                                                                            nullptr)),
                                                                                                                            new T::LabelStm(
                                                                                                                                    done_label))))))),
                            TY::VoidTy::Instance());
    }

    TR::ExpAndTy ForExp::Translate(S::Table<E::EnvEntry> *venv,
                                   S::Table<TY::Ty> *tenv, TR::Level *level,
                                   TEMP::Label *label) const {
        venv->BeginScope();
        TR::ExpAndTy expAndTy1 = lo->Translate(venv, tenv, level, label);
        TR::ExpAndTy expAndTy2 = hi->Translate(venv, tenv, level, label);
        TY::Ty *ty1 = expAndTy1.ty;
        TY::Ty *ty2 = expAndTy2.ty;
        if (ty1->kind != TY::Ty::INT || ty2->kind != TY::Ty::INT) {
            errormsg.Error(pos, "for exp's range type is not integer");
        }
        TR::Access *access = nullptr;
        E::EnvEntry *varEntry;
        varEntry = venv->Look(var);
        TEMP::Label *false_label = TEMP::NewLabel();
        if (varEntry) {
            access = ((E::VarEntry *) varEntry)->access;
        } else {
            access = TR::Access::AllocLocal(level, true);
            varEntry = new E::VarEntry(access, ty1, true);
            venv->Enter(var, varEntry);
        }
        T::Exp *varexp = ((E::VarEntry *) varEntry)->access->access->toExp(new T::TempExp(F::FP()));
        // if there is a break
        TR::ExpAndTy expAndTy = body->Translate(venv, tenv, level, false_label);
        TY::Ty *ty = expAndTy.ty;
        if (ty->kind != TY::Ty::VOID) {
            errormsg.Error(pos, "body should return no value");
        }
        venv->EndScope();
        TEMP::Label *true_label = TEMP::NewLabel();
        TR::Exp *loexp = expAndTy1.exp;
        TR::Exp *hiexp = expAndTy2.exp;
        TEMP::Temp *hitemp = TEMP::Temp::NewTemp();
        return TR::ExpAndTy(new TR::NxExp(new T::SeqStm(new T::MoveStm(varexp, expAndTy1.exp->UnEx()), new T::SeqStm(
                new T::MoveStm(new T::TempExp(hitemp), hiexp->UnEx()),
                new T::SeqStm(new T::CjumpStm(T::GT_OP, varexp, new T::TempExp(hitemp), false_label, true_label),
                              new T::SeqStm(new T::LabelStm(true_label), new T::SeqStm(expAndTy.exp->UnNx(),
                                                                                       new T::SeqStm(
                                                                                               new T::MoveStm(varexp,
                                                                                                              new T::BinopExp(
                                                                                                                      T::PLUS_OP,
                                                                                                                      varexp,
                                                                                                                      new T::ConstExp(
                                                                                                                              1))),
                                                                                               new T::SeqStm(
                                                                                                       new T::CjumpStm(
                                                                                                               T::LE_OP,
                                                                                                               varexp,
                                                                                                               new T::TempExp(
                                                                                                                       hitemp),
                                                                                                               true_label,
                                                                                                               false_label),
                                                                                                       new T::LabelStm(
                                                                                                               false_label))))))))),
                            TY::VoidTy::Instance());
    }

    TR::ExpAndTy BreakExp::Translate(S::Table<E::EnvEntry> *venv,
                                     S::Table<TY::Ty> *tenv, TR::Level *level,
                                     TEMP::Label *label) const {
        //p("Translate: breakexp\n");
        return TR::ExpAndTy(new TR::NxExp(new T::JumpStm(new T::NameExp(label), new TEMP::LabelList(label, nullptr))),
                            TY::VoidTy::Instance());
    }

    TR::ExpAndTy LetExp::Translate(S::Table<E::EnvEntry> *venv,
                                   S::Table<TY::Ty> *tenv, TR::Level *level,
                                   TEMP::Label *label) const {
        //p("Translate: letexp\n");
        venv->BeginScope();
        tenv->BeginScope();
        DecList *decList = this->decs;
        TR::Exp *exp = nullptr;
        T::StmList *stmList = new T::StmList(nullptr, nullptr), *tail = stmList;
        while (decList) {
            //p("DEclation\n")
            exp = decList->head->Translate(venv, tenv, level, label);
            decList = decList->tail;
            if (!exp) {
                continue;
            }
            tail = tail->tail = new T::StmList(exp->UnNx(), nullptr);
        }
        T::Stm *stm;
        stmList = stmList->tail;
        stm = TR::stmList2SeqStm(stmList);
        TR::ExpAndTy expAndTy = body->Translate(venv, tenv, level, label);
        TY::Ty *ty = expAndTy.ty;
        venv->EndScope();
        tenv->EndScope();
        if (stm) {
            return TR::ExpAndTy(new TR::ExExp(new T::EseqExp(stm, expAndTy.exp->UnEx())), ty);
        } else {
            return TR::ExpAndTy(new TR::ExExp(expAndTy.exp->UnEx()), ty);
        }
    }

    TR::ExpAndTy ArrayExp::Translate(S::Table<E::EnvEntry> *venv,
                                     S::Table<TY::Ty> *tenv, TR::Level *level,
                                     TEMP::Label *label) const {
        //p("Translate: arrayexp\n");
        TY::Ty *ty = tenv->Look(typ)->ActualTy();
        TR::ExpAndTy expAndTy = size->Translate(venv, tenv, level, label);
        // TODO: INIT CAN BE NULL
        TR::ExpAndTy expAndTy1 = init->Translate(venv, tenv, level, label);
        if (!ty) {
            errormsg.Error(pos, "undefined type");
        } else if (ty->kind != TY::Ty::ARRAY) {
            errormsg.Error(pos, "not array type");
        } else {
            TY::Ty *ty1 = expAndTy.ty;
            if (ty1->kind != TY::Ty::INT) {
                errormsg.Error(size->pos, "size not int");
            }
            TY::Ty *ty2 = expAndTy1.ty;
            if (!ty2->IsSameType(((TY::ArrayTy *) ty)->ty)) {
                errormsg.Error(init->pos, "type mismatch");
            }
        }
        return TR::ExpAndTy(new TR::ExExp(
                F::ExternalCall("initArray", new T::ExpList(new T::TempExp(F::FP()), new T::ExpList(expAndTy.exp->UnEx(), new T::ExpList(expAndTy1.exp->UnEx(),
                                                                                                 nullptr))))), ty);
    }

    TR::ExpAndTy VoidExp::Translate(S::Table<E::EnvEntry> *venv,
                                    S::Table<TY::Ty> *tenv, TR::Level *level,
                                    TEMP::Label *label) const {
        //p("Translate: voidexp\n")
        return TR::ExpAndTy(new TR::NxExp(new T::ExpStm(new T::ConstExp(0))), TY::VoidTy::Instance());
    }

    TR::Exp *FunctionDec::Translate(S::Table<E::EnvEntry> *venv,
                                    S::Table<TY::Ty> *tenv, TR::Level *level,
                                    TEMP::Label *label) const {
        FunDecList *funDecList = functions;
        while (funDecList) {
            if (venv->Look(funDecList->head->name)) {
                errormsg.Error(funDecList->head->pos, "two functions have the same name");
                funDecList = funDecList->tail;
                continue;
            }
            TY::TyList *tyList = make_formal_tylist(tenv, funDecList->head->params);
            TEMP::Label *name = TEMP::NewLabel();
            // TODO: FOR RUNTIME FUNCTIONS FIX IT IN LAB6
            if (TR::runtime(funDecList->head->name->Name())) {
                FieldList *params_ = funDecList->head->params;
                while (params_) {
                    params_->head->escape = false;
                    params_ = params_->tail;
                }
            }
            U::BoolList *boolList = make_boollist(funDecList->head->params);
            TR::Level *newLevel = TR::Level::NewLevel(level, name, boolList);
            if (funDecList->head->result) {
                TY::Ty *ty = tenv->Look(funDecList->head->result);
                if (!ty) {
                    errormsg.Error(funDecList->head->pos, "return type not found");
                    funDecList = funDecList->tail;
                    continue;
                }
                venv->Enter(funDecList->head->name, new E::FunEntry(newLevel, name, tyList, ty));
            } else {
                venv->Enter(funDecList->head->name, new E::FunEntry(newLevel, name, tyList, TY::VoidTy::Instance()));
            }
            funDecList = funDecList->tail;
        }
        funDecList = functions;
        while (funDecList) {
            TY::TyList *tyList = make_formal_tylist(tenv, funDecList->head->params);
            venv->BeginScope();
            FieldList *fieldList = funDecList->head->params;
            E::EnvEntry *funEntry = venv->Look(funDecList->head->name);
            TR::Level *newLevel = ((E::FunEntry *) funEntry)->level;
            F::AccessList *accessList = newLevel->frame->formals->tail;
            while (fieldList) {
                venv->Enter(fieldList->head->name,
                            new E::VarEntry(new TR::Access(newLevel, accessList->head), tyList->head));
                fieldList = fieldList->tail;
                tyList = tyList->tail;
                accessList = accessList->tail;
            }
            TR::ExpAndTy expAndTy = funDecList->head->body->Translate(venv, tenv, newLevel, label);
            TY::Ty *ty = expAndTy.ty;
            if (!funDecList->head->result && ty->kind != TY::Ty::VOID) {
                errormsg.Error(funDecList->head->pos, "procedure returns value");
            } else if (((E::FunEntry *) funEntry)->result != ty) {
                errormsg.Error(funDecList->head->pos, "return value doesnt match");
            }
            procEntryExit1(newLevel->frame, expAndTy.exp);
            venv->EndScope();
            funDecList = funDecList->tail;
        }
        return nullptr;
    }

    TR::Exp *VarDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                               TR::Level *level, TEMP::Label *label) const {
        //p("Translate: vardec\n");
        TR::ExpAndTy expAndTy = init->Translate(venv, tenv, level, label);
        TY::Ty *ty = expAndTy.ty;
        if (!typ) {
            if (ty->kind == TY::Ty::NIL) {
                errormsg.Error(pos, "init should not be nil without type specified");
            }
            TR::Access *access = TR::Access::AllocLocal(level, true);
            venv->Enter(var, new E::VarEntry(access, ty));
        } else {
            TY::Ty *ty1 = tenv->Look(typ);
            if (!ty1) {
                errormsg.Error(pos, "undefined type %s", typ->Name().c_str());
            } else {
                ty1 = ty1->ActualTy();
                if (!ty1->IsSameType(ty)) {
                    errormsg.Error(pos, "type mismatch");
                }
            }
            TR::Access *access = TR::Access::AllocLocal(level, this->escape);
            venv->Enter(var, new E::VarEntry(access, ty1));
        }
        A::Var *simpleVar = new A::SimpleVar(pos, this->var);
        TR::ExpAndTy expAndTy1 = simpleVar->Translate(venv, tenv, level, label);
        return new TR::NxExp(new T::MoveStm(expAndTy1.exp->UnEx(), expAndTy.exp->UnEx()));
    }

    TR::Exp *TypeDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                                TR::Level *level, TEMP::Label *label) const {
//        p("Translate: TypeDec\n");
        NameAndTyList *nameAndTyList = types;
        while (nameAndTyList) {
            TY::Ty *ty = tenv->Look(nameAndTyList->head->name);
            if (ty) {
                errormsg.Error(pos, "two types have the same name");
            } else {
                tenv->Enter(nameAndTyList->head->name, TY::NilTy::Instance());
            }
            nameAndTyList = nameAndTyList->tail;
        }
        nameAndTyList = types;
        while (nameAndTyList) {
            TY::Ty *ty = tenv->Look(nameAndTyList->head->name);
            tenv->Set(nameAndTyList->head->name, nameAndTyList->head->ty->SemAnalyze(tenv));
            nameAndTyList = nameAndTyList->tail;
        }
        nameAndTyList = types;
        while (nameAndTyList) {
            TY::Ty *ty = tenv->Look(nameAndTyList->head->name);
            TY::Ty *ty1 = ty;
            while (ty1->kind == TY::Ty::NAME) {
                ty1 = tenv->Look(((TY::NameTy *) ty1)->sym);
                if (ty1 == ty) {
                    errormsg.Error(pos, "illegal type cycle");
                    tenv->Set(nameAndTyList->head->name, TY::IntTy::Instance());
                    break;
                }
            }
            nameAndTyList = nameAndTyList->tail;
        }
        return nullptr;
    }

    TY::Ty *NameTy::Translate(S::Table<TY::Ty> *tenv) const {
        printf("Translate: namty\n");
        fflush(stdout);
        TY::Ty *ty = tenv->Look(this->name);
        if (!ty) {
            errormsg.Error(pos, "undefined type %s", name->Name().c_str());
            return new TY::NameTy(name, ty);
        }
        return new TY::NameTy(name, ty);
    }

    TY::Ty *RecordTy::Translate(S::Table<TY::Ty> *tenv) const {
        printf("Translate: recordty\n");
        fflush(stdout);
        FieldList *fieldList = this->record;
        TY::FieldList *fieldList1 = make_fieldlist(tenv, fieldList);
        TY::FieldList *ret = fieldList1;
        while (fieldList1) {
            if (!fieldList1->head->ty) {
                fieldList1->head->ty = TY::IntTy::Instance();
            }
            TY::Ty *ty = tenv->Look(fieldList->head->typ);
            if (!ty) {
                errormsg.Error(pos, "undefined type %s", fieldList->head->typ->Name().c_str());
                fieldList1->head->ty = TY::IntTy::Instance();
            }
            fieldList = fieldList->tail;
            fieldList1 = fieldList1->tail;
        }
        return new TY::RecordTy(ret);
    }

    TY::Ty *ArrayTy::Translate(S::Table<TY::Ty> *tenv) const {
        printf("Translate: arrayty\n");
        fflush(stdout);
        TY::Ty *ty = tenv->Look(array);
        if (!ty) {
            errormsg.Error(pos, "undefined type %s", array->Name().c_str());
            return new TY::ArrayTy(nullptr);
        } else
            return new TY::ArrayTy(ty);
    }

}  // namespace A
