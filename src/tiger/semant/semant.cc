#include "tiger/semant/semant.h"
#include "tiger/errormsg/errormsg.h"

extern EM::ErrorMsg errormsg;

using VEnvType = S::Table<E::EnvEntry> *;
using TEnvType = S::Table<TY::Ty> *;
/* var, exp, dec, type*/
/* var including fun*/
/* some problems with lab4*/
static bool flag = true;
namespace {
    static TY::TyList *make_formal_tylist(TEnvType tenv, A::FieldList *params) {
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

    static TY::FieldList *make_fieldlist(TEnvType tenv, A::FieldList *fields) {
        if (fields == nullptr) {
            return nullptr;
        }

        TY::Ty *ty = tenv->Look(fields->head->typ);
        return new TY::FieldList(new TY::Field(fields->head->name, ty),
                                 make_fieldlist(tenv, fields->tail));
    }

}  // namespace

namespace A {

    TY::Ty *SimpleVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                                  int labelcount) const {
        E::EnvEntry *ve = venv->Look(this->sym);
        if (ve && ve->kind == E::EnvEntry::VAR) {
            return ((E::VarEntry *) ve)->ty->ActualTy();
        } else {
            errormsg.Error(this->pos, "undefined variable %s", this->sym->Name().c_str());
            flag = false;
            return TY::IntTy::Instance();
        }
    }

    TY::Ty *FieldVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                                 int labelcount) const {
        TY::Ty *ty = this->var->SemAnalyze(venv, tenv, labelcount);
        if (ty->kind != TY::Ty::RECORD) {
            errormsg.Error(this->pos, "not a record type");
            flag = false;
            return TY::IntTy::Instance();
        } else {
            TY::FieldList *fieldList = ((TY::RecordTy *) ty)->fields;
            while (fieldList) {
                if (fieldList->head->name == this->sym) {
                    return fieldList->head->ty->ActualTy();
                }
                fieldList = fieldList->tail;
            }
            errormsg.Error(this->pos, "field %s doesn't exist", this->sym->Name().c_str());
            flag = false;
            return TY::IntTy::Instance();
        }
    }

    TY::Ty *SubscriptVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                                     int labelcount) const {


        TY::Ty *ty = this->var->SemAnalyze(venv, tenv, labelcount);
        if (ty->kind != TY::Ty::ARRAY) {
            errormsg.Error(this->pos, "array type required");
            flag = false;
            return TY::IntTy::Instance();
        } else {
            flag = true;
            TY::Ty *ty1 = this->subscript->SemAnalyze(venv, tenv, labelcount);
            if (ty1->kind != TY::Ty::INT) {
                if (flag) errormsg.Error(this->pos, "index type is not int");
                flag = false;
                return TY::IntTy::Instance();
            }
            return ((TY::ArrayTy *) ty)->ty->ActualTy();
        }
    }

    TY::Ty *VarExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
        return this->var->SemAnalyze(venv, tenv, labelcount);
    }

    TY::Ty *NilExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
        return TY::NilTy::Instance();
    }

    TY::Ty *IntExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
        return TY::IntTy::Instance();
    }

    TY::Ty *StringExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                                  int labelcount) const {
        return TY::StringTy::Instance();
    }

    TY::Ty *CallExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                                int labelcount) const {
        E::EnvEntry *funEntry = venv->Look(this->func);
        if (!funEntry || funEntry->kind != E::EnvEntry::FUN) {
            errormsg.Error(this->pos, "undefined function %s", this->func->Name().c_str());
            flag = false;
            return TY::VoidTy::Instance();
        } else {
            TY::TyList *formals = ((E::FunEntry *) funEntry)->formals;
            ExpList *expList;
            for (expList = this->args; expList && formals; expList = expList->tail, formals = formals->tail) {
                flag = true;
                TY::Ty *ty = expList->head->SemAnalyze(venv, tenv, labelcount);
                if (!formals->head->IsSameType(ty)) {
                    if (flag) errormsg.Error(expList->head->pos, "para type mismatch");
                    flag = false;
                }

            }
            if (expList) {
                errormsg.Error(expList->head->pos, "too many params in function %s", this->func->Name().c_str());
                flag = false;
            }
            if (formals) {
                if (flag)
                    errormsg.Error(this->pos, "too few params");
                flag = false;
            }
        }
        return ((E::FunEntry *) funEntry)->result->ActualTy();
    }

    TY::Ty *OpExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
        TY::Ty *ty1 = this->left->SemAnalyze(venv, tenv, labelcount);
        TY::Ty *ty2 = this->right->SemAnalyze(venv, tenv, labelcount);
        if (this->oper == A::PLUS_OP || this->oper == A::MINUS_OP || this->oper == A::TIMES_OP ||
            this->oper == A::DIVIDE_OP) {
            flag = true;
            if (ty1->kind != TY::Ty::INT) {
                if (flag) errormsg.Error(this->left->pos, "integer required");
                flag = false;
            }
            if (ty2->kind != TY::Ty::INT) {
                if (flag)
                    errormsg.Error(this->right->pos, "integer required");
                flag = false;
            }
        } else {
            if (!ty1->IsSameType(ty2)) {
                errormsg.Error(this->right->pos, "same type required");
                flag = false;
            }
        }
        return TY::IntTy::Instance();
    }

    TY::Ty *RecordExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                                  int labelcount) const {
        TY::Ty *ty = tenv->Look(this->typ);
        if (!ty) {
            errormsg.Error(this->pos, "undefined type %s", this->typ->Name().c_str());
            flag = false;
            return TY::IntTy::Instance();
        } else if (ty->ActualTy()->kind != TY::Ty::RECORD) {
            errormsg.Error(this->pos, "not a record type");
            flag = false;
            return ty->ActualTy();
        } else {
            TY::FieldList *fieldList = ((TY::RecordTy *) ty)->fields;
            EFieldList *eFieldList = this->fields;
            while (eFieldList) {
                flag = true;
                TY::Ty *ty1 = eFieldList->head->exp->SemAnalyze(venv, tenv, labelcount);
                if (flag && !ty1->IsSameType(fieldList->head->ty)) {
                    errormsg.Error(this->pos, "record type unmatched");
                    flag = false;
                }
                fieldList = fieldList->tail;
                eFieldList = eFieldList->tail;
            }
            if (fieldList) {
                errormsg.Error(this->pos, "too few efields");
                flag = false;
            }
        }

        return ty->ActualTy();
    }

    TY::Ty *SeqExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
        ExpList *expList = this->seq;
        TY::Ty *ty;
        if (!expList) {
            return TY::VoidTy::Instance();
        } else
            while (expList) {
                ty = expList->head->SemAnalyze(venv, tenv, labelcount);
                expList = expList->tail;
            }
        return ty;
    }

    TY::Ty *AssignExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                                  int labelcount) const {
        if (var->kind == A::Var::SIMPLE) {
            E::EnvEntry *varEntry = venv->Look(((SimpleVar *) var)->sym);
            if (varEntry->readonly) {
                errormsg.Error(this->pos, "loop variable can't be assigned");
                flag = false;
            }
        }
        flag = true;
        TY::Ty *ty1 = exp->SemAnalyze(venv, tenv, labelcount);
        TY::Ty *ty2 = var->SemAnalyze(venv, tenv, labelcount);
        if (!ty1->IsSameType(ty2)) {
            if (flag) errormsg.Error(this->pos, "unmatched assign exp");
            flag = false;
        }
        return TY::VoidTy::Instance();
    }

    TY::Ty *IfExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
        TY::Ty *ty1 = test->SemAnalyze(venv, tenv, labelcount);
        TY::Ty *ty2 = then->SemAnalyze(venv, tenv, labelcount);
        if (elsee) {
            TY::Ty *ty3 = elsee->SemAnalyze(venv, tenv, labelcount);
            if (!ty3->IsSameType(ty2)) {
                errormsg.Error(this->pos, "then exp and else exp type mismatch");
            }
        } else if (ty2->kind != TY::Ty::VOID) {
            errormsg.Error(this->pos, "if-then exp's body must produce no value");
        }
        return ty2;
    }

    TY::Ty *WhileExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                                 int labelcount) const {
        TY::Ty *ty = test->SemAnalyze(venv, tenv, labelcount);
        TY::Ty *ty1 = body->SemAnalyze(venv, tenv, labelcount);
        if (ty1->kind != TY::Ty::VOID) {
            errormsg.Error(pos, "while body must produce no value");
        }
        return TY::VoidTy::Instance();
    }

    TY::Ty *ForExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
        venv->BeginScope();
        TY::Ty *ty1 = lo->SemAnalyze(venv, tenv, labelcount);
        TY::Ty *ty2 = hi->SemAnalyze(venv, tenv, labelcount);
        if (ty1->kind != TY::Ty::INT || ty2->kind != TY::Ty::INT) {
            errormsg.Error(pos, "for exp's range type is not integer");
        }
        E::EnvEntry *varEntry = new E::VarEntry(ty1, true);
        venv->Enter(var, varEntry);
        TY::Ty *ty = body->SemAnalyze(venv, tenv, labelcount);
        if (ty->kind != TY::Ty::VOID) {
            errormsg.Error(pos, "body should return no value");
        }
        venv->EndScope();
        return TY::VoidTy::Instance();
    }

    TY::Ty *BreakExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                                 int labelcount) const {
        return TY::VoidTy::Instance();
    }

    TY::Ty *LetExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
        venv->BeginScope();
        tenv->BeginScope();
        DecList *decList = this->decs;
        while (decList) {
            decList->head->SemAnalyze(venv, tenv, labelcount);
            decList = decList->tail;
        }
        TY::Ty *ty = body->SemAnalyze(venv, tenv, labelcount);
        venv->EndScope();
        tenv->EndScope();
        return ty;
    }

    TY::Ty *ArrayExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                                 int labelcount) const {
        TY::Ty *ty = tenv->Look(typ)->ActualTy();
        if (!ty) {
            errormsg.Error(pos, "undefined type");
        } else if (ty->kind != TY::Ty::ARRAY) {
            errormsg.Error(pos, "not array type");
        } else {
            TY::Ty *ty1 = size->SemAnalyze(venv, tenv, labelcount);
            if (ty1->kind != TY::Ty::INT) {
                errormsg.Error(size->pos, "size not int");
            }
            TY::Ty *ty2 = init->SemAnalyze(venv, tenv, labelcount);
            if (!ty2->IsSameType(((TY::ArrayTy *) ty)->ty)) {
                errormsg.Error(init->pos, "type mismatch");
            }
        }
        return ty;
    }

    TY::Ty *VoidExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                                int labelcount) const {
        return TY::VoidTy::Instance();
    }

    void FunctionDec::SemAnalyze(VEnvType venv, TEnvType tenv,
                                 int labelcount) const {
        FunDecList *funDecList = functions;
        while (funDecList) {
            if (venv->Look(funDecList->head->name)) {
                errormsg.Error(funDecList->head->pos, "two functions have the same name");
                funDecList = funDecList->tail;
                continue;
            }
            TY::TyList *tyList = make_formal_tylist(tenv, funDecList->head->params);
            if (funDecList->head->result) {
                TY::Ty *ty = tenv->Look(funDecList->head->result);
                if (!ty) {
                    errormsg.Error(funDecList->head->pos, "return type not found");
                    funDecList = funDecList->tail;
                    continue;
                }
                venv->Enter(funDecList->head->name, new E::FunEntry(tyList, ty));
            } else {
                venv->Enter(funDecList->head->name, new E::FunEntry(tyList, TY::VoidTy::Instance()));
            }
            funDecList = funDecList->tail;
        }
        funDecList = functions;
        while (funDecList) {
            TY::TyList *tyList = make_formal_tylist(tenv, funDecList->head->params);
            venv->BeginScope();
            FieldList *fieldList = funDecList->head->params;
            TY::TyList *tyList1 = tyList;
            while (fieldList) {
                venv->Enter(fieldList->head->name, new E::VarEntry(tyList1->head));
                fieldList = fieldList->tail;
                tyList = tyList->tail;
            }
            TY::Ty *ty = funDecList->head->body->SemAnalyze(venv, tenv, labelcount);
            E::EnvEntry *funEntry = venv->Look(funDecList->head->name);
            // if (((E::FunEntry*)funEntry)->result && !funDecList->head->result) {
            if (!funDecList->head->result && ty->kind != TY::Ty::VOID) {
                errormsg.Error(funDecList->head->pos, "procedure returns value");
            } else if (((E::FunEntry *) funEntry)->result != ty) {
                errormsg.Error(funDecList->head->pos, "return value doesnt match");
            }
            venv->EndScope();
            funDecList = funDecList->tail;
        }
    }

    void VarDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
        TY::Ty *ty = init->SemAnalyze(venv, tenv, labelcount);

        if (!typ) {
            if (ty->kind == TY::Ty::NIL) {
                errormsg.Error(pos, "init should not be nil without type specified");
            }
            venv->Enter(var, new E::VarEntry(ty));
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
            venv->Enter(var, new E::VarEntry(ty1));
        }
    }

    void TypeDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
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

    }

    TY::Ty *NameTy::SemAnalyze(TEnvType tenv) const {
        TY::Ty *ty = tenv->Look(this->name);
        if (!ty) {
            errormsg.Error(pos, "undefined type %s", name->Name().c_str());
            return new TY::NameTy(name, ty);
        }
        return new TY::NameTy(name, ty);
    }

    TY::Ty *RecordTy::SemAnalyze(TEnvType tenv) const {
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

    TY::Ty *ArrayTy::SemAnalyze(TEnvType tenv) const {
        TY::Ty *ty = tenv->Look(array);
        if (!ty) {
            errormsg.Error(pos, "undefined type %s", array->Name().c_str());
            return new TY::ArrayTy(nullptr);
        } else
            return new TY::ArrayTy(ty);
    }

}  // namespace A

namespace SEM {
    void SemAnalyze(A::Exp *root) {
        if (root) root->SemAnalyze(E::BaseVEnv(), E::BaseTEnv(), 0);
    }

}  // namespace SEM