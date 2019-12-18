#include "tiger/escape/escape.h"

namespace ESC {
    void FindEscape(A::Exp *exp) {
        venv = new S::Table<EscapeEntry>();
        FindEscape(exp, 0);
    }

    // p133
    void FindEscape(A::Exp *exp, int d) {
        switch (exp->kind) {
            case A::Exp::VAR:
                VarEscape(((A::VarExp *) exp)->var, d);
                break;
            case A::Exp::CALL:
                for (auto args = ((A::CallExp *) exp)->args; args; args = args->tail) {
                    FindEscape(args->head, d);
                }
                break;
            case A::Exp::OP:
                FindEscape(((A::OpExp *) exp)->left, d);
                FindEscape(((A::OpExp *) exp)->right, d);
                break;
            case A::Exp::RECORD:
                for (auto efields = ((A::RecordExp *) exp)->fields; efields; efields = efields->tail) {
                    FindEscape(efields->head->exp, d);
                }
                break;
            case A::Exp::SEQ:
                for (auto seq = ((A::SeqExp *) exp)->seq; seq; seq = seq->tail) {
                    FindEscape(seq->head, d);
                }
                break;
            case A::Exp::ASSIGN:
                VarEscape(((A::AssignExp *) exp)->var, d);
                FindEscape(((A::AssignExp *) exp)->exp, d);
                break;
            case A::Exp::IF:
                FindEscape(((A::IfExp *) exp)->test, d);
                FindEscape(((A::IfExp *) exp)->then, d);
                if (((A::IfExp *) exp)->elsee) {
                    FindEscape(((A::IfExp *) exp)->elsee, d);
                }
                break;
            case A::Exp::WHILE:
                FindEscape(((A::WhileExp *) exp)->test, d);
                FindEscape(((A::WhileExp *) exp)->body, d);
                break;
            case A::Exp::ARRAY:
                FindEscape(((A::ArrayExp *) exp)->size, d);
                FindEscape(((A::ArrayExp *) exp)->init, d);
                break;
                // scopes
            case A::Exp::FOR:
                FindEscape(((A::ForExp *) exp)->lo, d);
                FindEscape(((A::ForExp *) exp)->hi, d);
                venv->BeginScope();
                venv->Enter(((A::ForExp *) exp)->var, new EscapeEntry(d, new bool(false)));
                FindEscape(((A::ForExp *) exp)->body, d);
                venv->EndScope();
                break;
            case A::Exp::LET:
                venv->BeginScope();
                for (auto decs = ((A::LetExp *) exp)->decs; decs; decs = decs->tail) {
                    DecEscape(decs->head, d);
                }
                FindEscape(((A::LetExp *) exp)->body, d);
                venv->EndScope();
                break;
                // nothing to do
            case A::Exp::BREAK:
            case A::Exp::VOID:
            case A::Exp::NIL:
            case A::Exp::INT:
            case A::Exp::STRING:
                return;
        }
    }

    void VarEscape(A::Var *var, int d) {
        switch (var->kind) {
            case A::Var::SIMPLE: {
                EscapeEntry *escapeEntry = venv->Look(((A::SimpleVar *) var)->sym);
                if (escapeEntry->depth < d) {
//                    printf("\n ESCAPE: %s\n", ((A::SimpleVar *) var)->sym->Name().c_str());fflush(stdout);
                    *(escapeEntry->escape) = true;
                }
                break;
            }
            case A::Var::FIELD:
                VarEscape(((A::FieldVar *) var)->var, d);
                break;
            case A::Var::SUBSCRIPT:
                VarEscape(((A::SubscriptVar *) var)->var, d);
                FindEscape(((A::SubscriptVar *) var)->subscript, d);
        }
    }

    void DecEscape(A::Dec *dec, int d) {
        switch (dec->kind) {
            case A::Dec::TYPE :
                break;
            case A::Dec::VAR :
                ((A::VarDec *) dec)->escape = false;
                venv->Enter(((A::VarDec *) dec)->var, new EscapeEntry(d, &((A::VarDec *) dec)->escape));
                FindEscape(((A::VarDec *) dec)->init, d);
                break;
            case A::Dec::FUNCTION :
                for (auto funcs = ((A::FunctionDec *) dec)->functions; funcs; funcs = funcs->tail) {
                    venv->BeginScope();
                    for (auto params = funcs->head->params; params; params = params->tail) {
                        params->head->escape = false;
                        venv->Enter(params->head->name,
                                new EscapeEntry(d + 1, &params->head->escape));
                    }
                    FindEscape(funcs->head->body, d + 1);
                    venv->EndScope();
                }
        }
    }

}  // namespace ESC