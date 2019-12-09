#include "tiger/codegen/codegen.h"

namespace CG {
#define p(s, ...) do { \
  printf(s, ##__VA_ARGS__); \
  fflush(stdout); \
  } while (0);
    static std::string framesize;
    static int pushlen;

    template<typename t>
    static uint listLen(t *i) {
        uint count = 0;
        while (i) {
            i = i->tail;
            count++;
        }
        return count;
    };

    bool runtime(std::string name) {
        return name == "initArray" || name == "allocRecord" || name == "stringEqual" || name == "print" ||
               name == "printi" || name == "flush" || name == "ord" || name == "chr" || name == "size" ||
               name == "substring" || name == "concat" || name == "not";
    }

    std::string fs() {
        return framesize + " + " + std::to_string(pushlen);
    }


    AS::InstrList *Codegen(F::Frame *f, T::StmList *stmList) {
        instrList = new AS::InstrList(nullptr, nullptr);
        tail = instrList;
        framesize = TEMP::LabelString(f->label) + "_framesize";
        pushlen = 0;
        for (; stmList; stmList = stmList->tail) {
            munchStm(stmList->head);
        }
        return F::F_procEntryExit2(instrList->tail);
    }

    void emit(AS::Instr *instr) {
        tail = tail->tail = new AS::InstrList(instr, nullptr);
    }

    //rdi, rsi, rdx, rcx, r8, r9
    // TODO: REMOVE NAME IN LAB6
    TEMP::TempList *munchArgs(int pos, T::ExpList *args, std::string name) {
        if (!args) {
            return nullptr;
        }
        // TODO: LAB6!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//        if (pos < 6) {
//            TEMP::Temp *reg = F::nthargs(pos);
//            emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(reg, nullptr),
//                                   new TEMP::TempList(head, nullptr)));
//            return new TEMP::TempList(reg, munchArgs(pos + 1, args->tail));
//        } else {
//
//        }
        if (runtime(name)) {
            TEMP::Temp *head = munchExp(args->head);
            TEMP::Temp *reg = F::nthargs(pos);
            emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(reg, nullptr),
                                   new TEMP::TempList(head, nullptr)));
            return new TEMP::TempList(reg, munchArgs(pos + 1, args->tail, name));
        } else {
            TEMP::Temp *head = munchExp(args->head);
            // TODO: PUSHQ OR ... minus rsp
            emit(new AS::MoveInstr("movq `s0, " + std::to_string(8 * (pos + 1)) + "(%rsp)", nullptr,
                                   new TEMP::TempList(head, new TEMP::TempList(F::RSP(),
                                                                               nullptr))));
            TEMP::TempList *tail_ = munchArgs(pos + 1, args->tail, name);
            return new TEMP::TempList(head, tail_);
        }
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
                break;
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
        TEMP::Temp *r = TEMP::Temp::NewTemp();
        emit(new AS::OperInstr("movq $" + std::to_string(exp->consti) + ", `d0", new TEMP::TempList(r, nullptr),
                               nullptr, nullptr));
        return r;
    }

    TEMP::Temp *munchCallExp(T::CallExp *exp) {
        TEMP::Temp *r = TEMP::Temp::NewTemp();
        uint len = runtime(((T::NameExp *) (exp->fun))->name->Name())? 0: listLen(exp->args->tail);
        pushlen += 8 * (len + 1);
        emit(new AS::OperInstr("subq $" + std::to_string(8 * (len + 1)) + ", %rsp", new TEMP::TempList(F::RSP(), nullptr),
                               new TEMP::TempList(F::RSP(),
                                                  nullptr),
                               nullptr));
        TEMP::TempList *args = munchArgs(0, exp->args->tail, ((T::NameExp *) (exp->fun))->name->Name());
        if (exp->args->head->kind == T::Exp::TEMP) {
            emit(new AS::OperInstr("leaq (" + fs() + ")(%rsp), `d0", new TEMP::TempList(r, nullptr),
                                   new TEMP::TempList(F::RSP(),
                                                      nullptr), nullptr));
            emit(new AS::OperInstr("movq `s0, (%rsp)", nullptr,
                                   new TEMP::TempList(r, new TEMP::TempList(F::RSP(), nullptr)), nullptr));
        } else {
            // TODO: WRONG
            TEMP::Temp *head = munchExp(exp->args->head);
            emit(new AS::OperInstr("movq `s0, (%rsp)", nullptr,
                                   new TEMP::TempList(head, new TEMP::TempList(F::RSP(), nullptr)), nullptr));
        }
        // TODO: REVERSED TAIL
        // TODO: CHANGE IN LAB6
        if (!runtime(((T::NameExp *) (exp->fun))->name->Name())) {
            emit(new AS::OperInstr("callq " + TEMP::LabelString(((T::NameExp *) exp->fun)->name), nullptr,
                                   nullptr, nullptr));

        } else {
            emit(new AS::OperInstr("callq " + TEMP::LabelString(((T::NameExp *) exp->fun)->name), nullptr,
                                   nullptr, nullptr));
        }
        emit(new AS::OperInstr("addq $" + std::to_string(8 * (len + 1)) + ", %rsp", new TEMP::TempList(F::RSP(), nullptr),
                               new TEMP::TempList(F::RSP(),
                                                  nullptr),
                               nullptr));
        pushlen -= 8 * (len + 1);
        // TODO: ADD PUSHED LENGTH
        return F::RV();
    }

    TEMP::Temp *munchNameExp(T::NameExp *exp) {
        TEMP::Temp *temp = TEMP::Temp::NewTemp();
        emit(new AS::OperInstr("leaq " + TEMP::LabelString(exp->name) + "(%rip), `d0",
                               new TEMP::TempList(temp, nullptr),
                               nullptr, nullptr));
        return temp;
    }

    TEMP::Temp *munchTempExp(T::TempExp *exp) {
        return exp->temp;
    }

    TEMP::Temp *munchMemExp(T::MemExp *exp) {
        TEMP::Temp *r = TEMP::Temp::NewTemp();
        if (exp->exp->kind == T::Exp::BINOP && ((T::BinopExp *) exp->exp)->op == T::PLUS_OP
            && ((T::BinopExp *) exp->exp)->right->kind == T::Exp::CONST) {
            TEMP::Temp *left = munchExp(((T::BinopExp *) exp->exp)->left);
            if (left == F::FP()) {
                emit(new AS::OperInstr(
                        "movq (" + std::to_string(((T::ConstExp *) ((T::BinopExp *) exp->exp)->right)->consti) + " + " +
                        fs() + ")"
                                    "(`s0), `d0", new TEMP::TempList(r, nullptr), new TEMP::TempList(left, nullptr),
                        nullptr));
            } else {
                emit(new AS::OperInstr(
                        "movq " + std::to_string(((T::ConstExp *) ((T::BinopExp *) exp->exp)->right)->consti) +
                        "(`s0), `d0", new TEMP::TempList(r, nullptr), new TEMP::TempList(left, nullptr), nullptr));
            }
        } else if (exp->exp->kind == T::Exp::BINOP && ((T::BinopExp *) exp->exp)->op == T::PLUS_OP
                   && ((T::BinopExp *) exp->exp)->left->kind == T::Exp::CONST) {
            TEMP::Temp *right = munchExp(((T::BinopExp *) exp->exp)->right);
            if (right == F::FP()) {
                emit(new AS::OperInstr(
                        "movq " + std::to_string(((T::ConstExp *) ((T::BinopExp *) exp->exp)->left)->consti) + " + " +
                        fs() + ")"
                                    "(`s0), `d0", new TEMP::TempList(r, nullptr), new TEMP::TempList(right, nullptr),
                        nullptr));
            } else {
                emit(new AS::OperInstr(
                        "movq " + std::to_string(((T::ConstExp *) ((T::BinopExp *) exp->exp)->left)->consti) +
                        "(`s0), `d0", new TEMP::TempList(r, nullptr), new TEMP::TempList(right, nullptr), nullptr));
            }
        } else if (exp->exp->kind == T::Exp::CONST) {
            emit(new AS::OperInstr("movq " + std::to_string(((T::ConstExp *) exp->exp)->consti) + ", `d0",
                                   new TEMP::TempList(r,
                                                      nullptr),
                                   nullptr, nullptr));
        } else {
            TEMP::Temp *expt = munchExp(exp->exp);
            if (expt == F::FP()) {
                emit(new AS::OperInstr("movq (" + fs() + ")(`s0), `d0", new TEMP::TempList(r, nullptr),
                                       new TEMP::TempList(expt, nullptr), nullptr));
            } else {
                emit(new AS::OperInstr("movq (`s0), `d0", new TEMP::TempList(r, nullptr),
                                       new TEMP::TempList(expt, nullptr), nullptr));
            }
        }
        return r;
    }

    TEMP::Temp *munchBinopExp(T::BinopExp *exp) {
//        PLUS_OP, MINUS_OP, MUL_OP, DIV_OP, AND_OP, OR_OP, LSHIFT_OP, RSHIFT_OP, ARSHIFT_OP, XOR_OP
//        TEMP::Temp *temp = TEMP::Temp::NewTemp();
//        TEMP::Temp *left = munchExp(exp->left);
//        TEMP::Temp *right = munchExp(exp->right);
        switch (exp->op) {
            case T::PLUS_OP: {
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                if (exp->right->kind == T::Exp::CONST) {
                    TEMP::Temp *left = munchExp(exp->left);
                    if (left == F::FP()) {
                        emit(new AS::MoveInstr("leaq (" + fs() + ")(`s0), `d0", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
                    } else {
                        emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
                    }
                    emit(new AS::OperInstr("addq $" + std::to_string(((T::ConstExp *) exp->right)->consti) + ", `d0",
                                           new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(r,
                                                              nullptr),
                                           nullptr));
                } else if (exp->left->kind == T::Exp::CONST) {
                    TEMP::Temp *right = munchExp(exp->right);
                    if (right == F::FP()) {
                        emit(new AS::MoveInstr("leaq (" + fs() + ")(`s0), `d0", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(right, nullptr)));
                    } else {
                        emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(right, nullptr)));
                    }
                    emit(new AS::OperInstr("addq $" + std::to_string(((T::ConstExp *) exp->left)->consti) + ", `d0",
                                           new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(r,
                                                              nullptr),
                                           nullptr));
                } else {
                    TEMP::Temp *left = munchExp(exp->left);
                    TEMP::Temp *right = munchExp(exp->right);
                    if (left == F::FP()) {
                        emit(new AS::MoveInstr("leaq (" + fs() + ")(`s0), `d0", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left,
                                                                  nullptr)));
                    } else {
                        emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(r, nullptr), new TEMP::TempList(left,
                                                                                                                   nullptr)));
                    }
                    emit(new AS::OperInstr("addq `s1, `d0", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(r, new TEMP::TempList(right,
                                                                                    nullptr)),
                                           nullptr));

                }
                return r;
            }

            case T::MINUS_OP: {
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                if (exp->right->kind == T::Exp::CONST) {
                    TEMP::Temp *left = munchExp(exp->left);
                    if (left == F::FP()) {
                        emit(new AS::MoveInstr("leaq (" + fs() + ")(`s0), `d0", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left,
                                                                  nullptr)));
                    } else {
                        emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(r, nullptr), new TEMP::TempList(left,
                                                                                                                   nullptr)));
                    }
                    emit(new AS::OperInstr("subq $" + std::to_string(((T::ConstExp *) exp->right)->consti) + ", `d0",
                                           new TEMP::TempList(r,
                                                              nullptr),
                                           nullptr, nullptr));
                } else {
                    TEMP::Temp *left = munchExp(exp->left);
                    TEMP::Temp *right = munchExp(exp->right);
                    if (left == F::FP()) {
                        emit(new AS::MoveInstr("leaq (" + fs() + ")(`s0), `d0", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
                    } else {
                        emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
                    }
                    emit(new AS::OperInstr("subq `s1, `d0", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(r, new TEMP::TempList(right,
                                                                                    nullptr)), nullptr));

                }
                return r;
            }

            case T::MUL_OP: {
                // result stored in RAX() RDX(), trashed
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                TEMP::Temp *left = munchExp(exp->left);
                TEMP::Temp *right = munchExp(exp->right);
                emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(F::RAX(), nullptr), new TEMP::TempList(left,
                                                                                                                  nullptr)));
                emit(new AS::OperInstr("imulq `s0", new TEMP::TempList(F::RDX(), new TEMP::TempList(F::RAX(), nullptr)),
                                       new TEMP::TempList(right, new TEMP::TempList(F::RAX(),
                                                                                    nullptr)),
                                       nullptr));
                emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(r, nullptr), new TEMP::TempList(F::RAX(),
                                                                                                           nullptr)));
                return r;
            }

            case T::DIV_OP: {
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                TEMP::Temp *left = munchExp(exp->left);
                TEMP::Temp *right = munchExp(exp->right);
                emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(F::RAX(), nullptr), new TEMP::TempList(left,
                                                                                                                  nullptr)));
                emit(new AS::OperInstr("cqto", new TEMP::TempList(F::RAX(), new TEMP::TempList(F::RDX(), nullptr)),
                                       new TEMP::TempList(F::RAX(), nullptr), nullptr));
                emit(new AS::OperInstr("idivq `s0", new TEMP::TempList(F::RAX(), new TEMP::TempList(F::RDX(), nullptr)),
                                       new TEMP::TempList(right, new TEMP::TempList(F::RAX(),
                                                                                    new TEMP::TempList(F::RDX(),
                                                                                                       nullptr))),
                                       new AS::Targets(nullptr)));
                emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(r, nullptr), new TEMP::TempList(F::RAX(),
                                                                                                           nullptr)));
                return r;
            }
            default:
                assert(0);
        }
    }

    void munchSeqStm(T::SeqStm *stm) {
        munchStm(stm->left);
        munchStm(stm->right);
    }

    void munchLabelStm(T::LabelStm *stm) {
        emit(new AS::LabelInstr(TEMP::LabelString(stm->label), stm->label));
    }

    void munchJumpStm(T::JumpStm *stm) {
        emit(new AS::OperInstr("jmp `j0", nullptr, nullptr, new AS::Targets(stm->jumps)));
    }

    void munchCJumpStm(T::CjumpStm *stm) {
        TEMP::Temp *left = munchExp(stm->left);
        TEMP::Temp *right = munchExp(stm->right);
        emit(new AS::OperInstr("cmpq `s0, `s1", nullptr, new TEMP::TempList(right, new TEMP::TempList(left, nullptr)),
                               nullptr));
        std::string jop;
        switch (stm->op) {
            case T::NE_OP:
                jop = "jne";
                break;
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
                                                                                                   new TEMP::LabelList(
                                                                                                           stm->false_label,
                                                                                                           nullptr)))));

    }

    void munchMoveStm(T::MoveStm *stm) {
        stm->Print(stdout, 4);
        T::Exp::Kind dstk = stm->dst->kind;
        T::Exp::Kind srck = stm->src->kind;
        // reg -> reg
        // mem -> reg
        // reg -> mem
        // imm -> reg
        // imm -> mem
        if (dstk == T::Exp::TEMP) {
            TEMP::Temp *srct = munchExp(stm->src);
            if (srct == F::FP()) {
                emit(new AS::MoveInstr("leaq (" + fs() + ")(`s0),`d0",
                                       new TEMP::TempList(((T::TempExp *) stm->dst)->temp, nullptr),
                                       new TEMP::TempList(srct,
                                                          nullptr)));
            } else {
                emit(new AS::MoveInstr("movq `s0,`d0", new TEMP::TempList(((T::TempExp *) stm->dst)->temp, nullptr),
                                       new TEMP::TempList(srct,
                                                          nullptr)));
            }
        } else if (dstk == T::Exp::MEM) {
            T::Exp *exp = ((T::MemExp *) stm->dst)->exp;
            if (exp->kind == T::Exp::BINOP
                && ((T::BinopExp *) exp)->op == T::PLUS_OP
                && ((T::BinopExp *) exp)->right->kind == T::Exp::CONST) {
                TEMP::Temp *left = munchExp(((T::BinopExp *) exp)->left);
                TEMP::Temp *src = munchExp(stm->src);
                // TODO: TOSTRING DOESNT FIT NEGATIVE
                if (left == F::FP()) {
                    emit(new AS::OperInstr(
                            "movq `s1, (" + std::to_string(((T::ConstExp *) (((T::BinopExp *) exp)->right))->consti) +
                            " + " + fs() + ")" + "(`s0)",
                            nullptr, new TEMP::TempList(left, new TEMP::TempList(src, nullptr)), nullptr));
                } else {
                    emit(new AS::OperInstr(
                            "movq `s1, " + std::to_string(((T::ConstExp *) (((T::BinopExp *) exp)->right))->consti) +
                            "(`s0)",
                            nullptr, new TEMP::TempList(left, new TEMP::TempList(src, nullptr)), nullptr));
                }
                return;
            } else if (exp->kind == T::Exp::BINOP
                       && ((T::BinopExp *) exp)->op == T::PLUS_OP
                       && ((T::BinopExp *) exp)->left->kind == T::Exp::CONST) {
                TEMP::Temp *right = munchExp(((T::BinopExp *) exp)->left);
                TEMP::Temp *src = munchExp(stm->src);
                if (right == F::FP()) {
                    emit(new AS::OperInstr(
                            "movq `s1, (" + std::to_string(((T::ConstExp *) (((T::BinopExp *) exp)->left))->consti) +
                            " + " + fs() + ")" + "(`s0)",
                            nullptr, new TEMP::TempList(right, new TEMP::TempList(src, nullptr)), nullptr));
                } else {
                    emit(new AS::OperInstr(
                            "movq `s1, " + std::to_string(((T::ConstExp *) (((T::BinopExp *) exp)->left))->consti) +
                            "(`s0)",
                            nullptr, new TEMP::TempList(right, new TEMP::TempList(src, nullptr)), nullptr));
                }
                return;
            } else if (stm->src->kind == T::Exp::MEM) {
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                TEMP::Temp *src = munchExp(stm->src);
                TEMP::Temp *dst = munchExp(stm->dst);
                if (src == F::FP()) {
                    emit(new AS::OperInstr("movq (" + fs() + ")(`s0), `d0", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                } else {
                    emit(new AS::OperInstr("movq (`s0), `d0", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                }
                if (dst == F::FP()) {
                    emit(new AS::OperInstr("movq `s0, (" + fs() + ")(`s1)", nullptr,
                                           new TEMP::TempList(r, new TEMP::TempList(dst, nullptr)),
                                           nullptr));
                } else {
                    emit(new AS::OperInstr("movq `s0, (`s1)", nullptr,
                                           new TEMP::TempList(r, new TEMP::TempList(dst, nullptr)),
                                           nullptr));
                }

            } else if (exp->kind == T::Exp::CONST) {
                TEMP::Temp *src = munchExp(stm->src);
                emit(new AS::OperInstr("movq `s0, (" + std::to_string(((T::ConstExp *) exp)->consti) + ")",
                                       nullptr, new TEMP::TempList(src, nullptr), nullptr));
            } else {
                TEMP::Temp *src = munchExp(stm->src);
                TEMP::Temp *mexp = munchExp(exp);
                if (mexp == F::FP()) {
                    emit(new AS::OperInstr("movq `s1, (" + fs() + ")(`s0)", nullptr,
                                           new TEMP::TempList(mexp, new TEMP::TempList(src, nullptr)), nullptr));
                } else {
                    emit(new AS::OperInstr("movq `s1, (`s0)", nullptr,
                                           new TEMP::TempList(mexp, new TEMP::TempList(src, nullptr)), nullptr));
                }
            }
        }
    }

    void munchExpStm(T::ExpStm *stm) {
        munchExp(stm->exp);
    }
}  // namespace CG