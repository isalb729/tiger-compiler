#include "tiger/codegen/codegen.h"

namespace CG {

    AS::InstrList *Codegen(F::Frame *f, T::StmList *stmList) {
        instrList = new AS::InstrList(nullptr, nullptr);
        tail = instrList;
        for (; stmList; stmList = stmList->tail) {
            munchStm(stmList->head);
        }
        return instrList->tail;
    }

    void emit(AS::Instr *instr) {
        tail = tail->tail = new AS::InstrList(instr, nullptr);
    }

    TEMP::TempList *munchArgs(int pos, T::ExpList *args) {
        if (!args) {
            return nullptr;
        }
        // TODO: PUT THEM INTO REGS
        TEMP::TempList *tail_ = munchArgs(pos + 1, args->tail);
        TEMP::Temp *head = munchExp(args->head);
        emit(new AS::OperInstr("pushl `s0", new TEMP::TempList(F::SP, nullptr), new TEMP::TempList(head, new TEMP::TempList(F::SP,
                                                                                                                  nullptr)),
                     nullptr));
        return new TEMP::TempList(head, tail_);
    }

    TEMP::Temp *munchExp(T::Exp *exp) {
        switch (exp->kind) {
            case T::Exp::CONST:
                return munchConstExp((T::ConstExp *) exp);
            case T::Exp::CALL:
                return munchCallExp((T::CallExp *) exp);
            case T::Exp::NAME:
                return munchNameExp((T::NameExp *) exp);
            case T::Exp::ESEQ:
                assert(0);
            case T::Exp::TEMP:
                return munchTempExp((T::TempExp *) exp);
            case T::Exp::MEM:
                return munchMemExp((T::MemExp *) exp);
            case T::Exp::BINOP:
                return munchBinopExp((T::BinopExp *) exp);
            default:
                assert(0);
        }
    }

    void munchStm(T::Stm *stm) {
        switch (stm->kind) {
            case T::Stm::SEQ:
                // TODO: DOES IT EXIST?
                munchSeqStm((T::SeqStm *) stm);
                break;
            case T::Stm::LABEL:
                munchLabelStm((T::LabelStm *) stm);
                break;
            case T::Stm::JUMP:
                munchJumpStm((T::JumpStm *) stm);
                break;
            case T::Stm::CJUMP:
                munchCJumpStm((T::CjumpStm *) stm);
            case T::Stm::MOVE:
                munchMoveStm((T::MoveStm *) stm);
                break;
            case T::Stm::EXP:
                munchExpStm((T::ExpStm *) stm);
                break;
            default:
                assert(0);
        }

    }

    TEMP::Temp *munchConstExp(T::ConstExp *exp) {
        TEMP::Temp *temp = TEMP::Temp::NewTemp();
        emit(new AS::MoveInstr("movl $" + std::to_string(exp->consti) + ", `d0", new TEMP::TempList(temp, nullptr),
                               nullptr));
        return temp;
    }

    TEMP::Temp *munchCallExp(T::CallExp *exp) {
        TEMP::Temp *temp = TEMP::Temp::NewTemp();
        TEMP::TempList *args = munchArgs(0, exp->args);
        // TODO: EXIT ENTRY
        emit(new AS::OperInstr("call " + TEMP::LabelString(((T::NameExp *) exp->fun)->name), nullptr,
                               nullptr, new AS::Targets(nullptr)));
        emit(new AS::MoveInstr("movl `s0, `d0", new TEMP::TempList(temp, nullptr), new TEMP::TempList(F::eax, NULL)));
        return nullptr;
    }

    TEMP::Temp *munchNameExp(T::NameExp *exp) {
        TEMP::Temp *temp = TEMP::Temp::NewTemp();
        emit(new AS::MoveInstr("movl $" + TEMP::LabelString(exp->name) + ", `d0", new TEMP::TempList(temp, nullptr),
                               nullptr));
        return temp;
    }

    TEMP::Temp *munchTempExp(T::TempExp *exp) {
        return exp->temp;
    }

    TEMP::Temp *munchMemExp(T::MemExp *exp) {
        TEMP::Temp *expt = munchExp(exp->exp);
        TEMP::Temp *temp = TEMP::Temp::NewTemp();
        emit(new AS::MoveInstr("movl (`s0), `d0", new TEMP::TempList(temp, nullptr),
                               new TEMP::TempList(expt, nullptr)));
        return temp;
    }

    TEMP::Temp *munchBinopExp(T::BinopExp *exp) {
//        PLUS_OP, MINUS_OP, MUL_OP, DIV_OP, AND_OP, OR_OP, LSHIFT_OP, RSHIFT_OP, ARSHIFT_OP, XOR_OP
        TEMP::Temp *temp = TEMP::Temp::NewTemp();
        TEMP::Temp *left = munchExp(exp->left);
        TEMP::Temp *right = munchExp(exp->right);
        switch (exp->op) {
            case T::PLUS_OP:
                emit(new AS::MoveInstr("movl `s0, `d0", new TEMP::TempList(temp, nullptr),
                                       new TEMP::TempList(left, nullptr)));
                emit(new AS::OperInstr("addl `s0, `d0", new TEMP::TempList(temp, nullptr),
                                       new TEMP::TempList(right, new TEMP::TempList(temp,
                                                                                    nullptr)),
                                       new AS::Targets(nullptr)));
                break;
            case T::MINUS_OP:
                emit(new AS::MoveInstr("movl `s0, `d0", new TEMP::TempList(temp, nullptr),
                                       new TEMP::TempList(left, nullptr)));
                emit(new AS::OperInstr("subl `s0, `d0", new TEMP::TempList(temp, nullptr),
                                       new TEMP::TempList(right, new TEMP::TempList(temp,
                                                                                    nullptr)),
                                       new AS::Targets(nullptr)));
                break;
            case T::MUL_OP:
                // result stored in eax edx, trashed
                emit(new AS::MoveInstr("movl `s0, `d0", new TEMP::TempList(F::eax, nullptr), new TEMP::TempList(left,
                                                                                                                nullptr)));
                emit(new AS::OperInstr("imul `s0", new TEMP::TempList(F::edx, new TEMP::TempList(F::eax, nullptr)),
                                       new TEMP::TempList(right, new TEMP::TempList(F::eax,
                                                                                    nullptr)),
                                       new AS::Targets(nullptr)));
                emit(new AS::MoveInstr("movl `s0, `d0", new TEMP::TempList(temp, nullptr), new TEMP::TempList(F::eax,
                                                                                                              nullptr)));
                break;
            case T::DIV_OP:
                emit(new AS::MoveInstr("movl `s0, `d0", new TEMP::TempList(F::eax, nullptr), new TEMP::TempList(left,
                                                                                                                nullptr)));
                emit(new AS::OperInstr("cltd", new TEMP::TempList(F::eax, new TEMP::TempList(F::edx, nullptr)),
                                       new TEMP::TempList(F::eax, nullptr), new AS::Targets(nullptr)));
                emit(new AS::OperInstr("idivl `s0", new TEMP::TempList(F::eax, new TEMP::TempList(F::edx, nullptr)),
                                       new TEMP::TempList(right, new TEMP::TempList(F::eax,
                                                                                    new TEMP::TempList(F::edx,
                                                                                                       nullptr))),
                                       new AS::Targets(nullptr)));
                emit(new AS::MoveInstr("movl `s0, `d0", new TEMP::TempList(temp, nullptr), new TEMP::TempList(F::eax,
                                                                                                              nullptr)));
                break;
            default:
                assert(0);
        }
        return temp;
    }

    void munchSeqStm(T::SeqStm *stm) {

        while (stm->right && stm->right->kind == T::Stm::SEQ) {
            if (stm->left) {
                munchStm(stm->left);
            }
            stm = (T::SeqStm *) stm->right;
        }
        if (stm->right) {
            munchStm(stm->right);
        }
    }

    void munchLabelStm(T::LabelStm *stm) {
        emit(new AS::LabelInstr(TEMP::LabelString(stm->label) + ":", stm->label));
    }

    void munchJumpStm(T::JumpStm *stm) {
        emit(new AS::OperInstr("jmp `j0", nullptr, nullptr, new AS::Targets(stm->jumps)));
    }

    void munchCJumpStm(T::CjumpStm *stm) {
        TEMP::Temp *left = munchExp(stm->left);
        TEMP::Temp *right = munchExp(stm->right);
        emit(new AS::OperInstr("cmp `s0, `s1", nullptr, new TEMP::TempList(right, new TEMP::TempList(left, nullptr)),
                               new AS::Targets(
                                       nullptr)));
        std::string jop;
        switch (stm->op) {
            case T::EQ_OP:
                jop = "je";
                break;
            case T::GT_OP:
                jop = "jg";
                break;
            case T::GE_OP:
                jop = "jge";
                break;
            case T::LT_OP:
                jop = "jl";
                break;
            case T::LE_OP:
                jop = "jle";
                break;
            case T::ULE_OP:
                jop = "jbe";
                break;
            case T::ULT_OP:
                jop = "jb";
                break;
            case T::UGT_OP:
                jop = "ja";
                break;
            case T::UGE_OP:
                jop = "jae";
        }
        emit(new AS::OperInstr(jop + " `j0", nullptr, nullptr, new AS::Targets(new TEMP::LabelList(stm->true_label,
                                                                                                   nullptr))));

    }

    void munchMoveStm(T::MoveStm *stm) {
        T::Exp::Kind dstk = stm->dst->kind;
        T::Exp::Kind srck = stm->src->kind;
        // TODO: ... -> OFFSET(REG)
        // reg -> reg
        // mem -> reg
        // reg -> mem
        // imm -> reg
        // imm -> mem
        if (dstk == T::Exp::TEMP) {
            TEMP::Temp *srct = munchExp(stm->src);
            emit(new AS::MoveInstr("movl `s0,`d0", new TEMP::TempList(((T::TempExp *) stm->dst)->temp, nullptr),
                                   new TEMP::TempList(srct,
                                                      nullptr)));
        } else if (dstk == T::Exp::MEM) {
            TEMP::Temp *srct = munchExp(stm->src);
            TEMP::Temp *dstt = munchExp(((T::MemExp *) stm->dst)->exp);
            // TODO: OPER
            emit(new AS::MoveInstr("movl `s0, (`s1)", nullptr,
                                   new TEMP::TempList(srct, new TEMP::TempList(dstt, nullptr))));
        }
    }

    void munchExpStm(T::ExpStm *stm) {
        munchExp(stm->exp);
    }
}  // namespace CG