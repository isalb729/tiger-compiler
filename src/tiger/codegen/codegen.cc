#include "tiger/codegen/codegen.h"
#include <map>

namespace CG {
#define p(s, ...) do { \
  printf(s, ##__VA_ARGS__); \
  fflush(stdout); \
  } while (0);
    static std::string framesize;

    template<typename t>
    static uint listLen(t *i) {
        uint count = 0;
        while (i) {
            i = i->tail;
            count++;
        }
        return count;
    };


    std::string int2str(int i) {
        return i >= 0 ? std::to_string(i) : "-" + std::to_string(-i);
    }

    std::string inframe(int offset) {
        return "(" + framesize + " + " + int2str(offset) + ")" + "(%rsp)";
    }

    void saveCalleeSaved() {
        TEMP::TempList *calleeSaved = F::calleeSaved();
        TEMP::TempList *callee_ = callee;
        while (calleeSaved) {
            emit(new AS::MoveInstr("movq `s0, `d0 #34", new TEMP::TempList(callee_->head, nullptr),
                                   new TEMP::TempList(calleeSaved->head, nullptr)));
            calleeSaved = calleeSaved->tail;
            callee_ = callee_->tail;
        }
    }

    void saveCallerSaved() {
        TEMP::TempList *callerSaved = F::callerSaved();
        TEMP::TempList *caller_ = caller;
        while (callerSaved) {
            emit(new AS::MoveInstr("movq `s0, `d0 #45", new TEMP::TempList(caller_->head, nullptr),
                                   new TEMP::TempList(callerSaved->head, nullptr)));
            callerSaved = callerSaved->tail;
            caller_ = caller_->tail;
        }
    }


    void restoreCalleeSaved() {
        TEMP::TempList *calleeSaved = F::calleeSaved();
        TEMP::TempList *callee_ = callee;
        while (calleeSaved) {
            emit(new AS::MoveInstr("movq `s0, `d0 #56", new TEMP::TempList(calleeSaved->head, nullptr),
                                   new TEMP::TempList(callee_->head, nullptr)));
            calleeSaved = calleeSaved->tail;
            callee_ = callee_->tail;
        }
    }

    void restoreCallerSaved() {
        TEMP::TempList *callerSaved = F::callerSaved();
        TEMP::TempList *caller_ = caller;
        while (callerSaved) {
            emit(new AS::MoveInstr("movq `s0, `d0 #60", new TEMP::TempList(callerSaved->head, nullptr),
                                   new TEMP::TempList(caller_->head, nullptr)));
            callerSaved = callerSaved->tail;
            caller_ = caller_->tail;
        }
    }


    AS::InstrList *Codegen(F::Frame *f, T::StmList *stmList) {
        // init callee and caller
        TEMP::TempList *tail_;
        callee = nullptr;
        for (auto calleeSaved = F::calleeSaved(); calleeSaved; calleeSaved = calleeSaved->tail) {
            if (!callee) {
                callee = new TEMP::TempList(TEMP::Temp::NewTemp(), nullptr);
                tail_ = callee;
            } else {
                tail_ = tail_->tail = new TEMP::TempList(TEMP::Temp::NewTemp(), nullptr);
            }
        }
        caller = nullptr;
        for (auto callerSaved = F::callerSaved(); callerSaved; callerSaved = callerSaved->tail) {
            if (!caller) {
                caller = new TEMP::TempList(TEMP::Temp::NewTemp(), nullptr);
                tail_ = caller;
            } else {
                tail_ = tail_->tail = new TEMP::TempList(TEMP::Temp::NewTemp(), nullptr);
            }
        }
        instrList = new AS::InstrList(nullptr, nullptr);
        tail = instrList;
        framesize = TEMP::LabelString(f->label) + "_framesize";
        saveCalleeSaved();
        for (; stmList; stmList = stmList->tail) {
            munchStm(stmList->head);
        }
        restoreCalleeSaved();
        return F::F_procEntryExit2(instrList->tail);
    }

    void emit(AS::Instr *instr) {
        tail = tail->tail = new AS::InstrList(instr, nullptr);
    }

    //rdi, rsi, rdx, rcx, r8, r9
    TEMP::TempList *munchArgs(int pos, T::ExpList *args) {
        if (!args) {
            return nullptr;
        }
        if (pos < 6) {
            // TODO: SOME VARIABLES HAVE TO ESCAPE
            TEMP::Temp *head = munchExp(args->head);
            TEMP::Temp *reg = F::nthargs(pos);
            emit(new AS::MoveInstr("movq `s0, `d0 #55", new TEMP::TempList(reg, nullptr),
                                   new TEMP::TempList(head, nullptr)));
            return new TEMP::TempList(reg, munchArgs(pos + 1, args->tail));
        } else {
            // NOT IMPLEMENTED FOR MORE THAN 6 ARGS
            assert(0);
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
        emit(new AS::OperInstr("movq $" + int2str(exp->consti) + ", `d0 #113",
                               new TEMP::TempList(r, nullptr), nullptr, nullptr));
        return r;
    }

    TEMP::Temp *munchCallExp(T::CallExp *exp) {
        TEMP::Temp *r = TEMP::Temp::NewTemp();
        TEMP::TempList *args = munchArgs(0, exp->args->tail);
        // mem means escape, temp not
//        saveArgRegs();
        // static link
        // TODO: REMOVE
        if (exp->args->head->kind == T::Exp::TEMP) {
            emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #123", new TEMP::TempList(r, nullptr),
                                   new TEMP::TempList(F::RSP(),
                                                      nullptr), nullptr));
            emit(new AS::OperInstr("pushq `s0", new TEMP::TempList(F::RSP(), nullptr),
                                   new TEMP::TempList(r, new TEMP::TempList(F::RSP(),
                                                                            nullptr)),
                                   nullptr));
        } else {
            r = munchExp(exp->args->head);
            emit(new AS::OperInstr("pushq `s0", new TEMP::TempList(F::RSP(), nullptr),
                                   new TEMP::TempList(r, new TEMP::TempList(F::RSP(),
                                                                            nullptr)),
                                   nullptr));
        }
        saveCallerSaved();
        // argregs as uses too, or it will be cleared after call
        emit(new AS::OperInstr("callq " + TEMP::LabelString(((T::NameExp *) exp->fun)->name) + " #133",
                               new TEMP::TempList(F::RAX(), F::argRegs()), TEMP::unionList(args, F::argRegs()),
                               nullptr));
        restoreCallerSaved();
//        restoreArgRegs();
        emit(new AS::OperInstr("addq $8, `d0", new TEMP::TempList(F::RSP(), nullptr),
                               new TEMP::TempList(F::RSP(), nullptr),
                               nullptr));
        return F::RV();
    }

    TEMP::Temp *munchNameExp(T::NameExp *exp) {
        TEMP::Temp *r = TEMP::Temp::NewTemp();
        emit(new AS::OperInstr("leaq " + TEMP::LabelString(exp->name) + "(%rip), `d0 #140",
                               new TEMP::TempList(r, nullptr), nullptr, nullptr));
        return r;
    }

    TEMP::Temp *munchTempExp(T::TempExp *exp) {
        return exp->temp;
    }

    TEMP::Temp *munchMemExp(T::MemExp *exp) {
        TEMP::Temp *r = TEMP::Temp::NewTemp();
        // M[Reg + c]
        if (exp->exp->kind == T::Exp::BINOP && ((T::BinopExp *) exp->exp)->op == T::PLUS_OP
            && ((T::BinopExp *) exp->exp)->right->kind == T::Exp::CONST) {
            TEMP::Temp *left = munchExp(((T::BinopExp *) exp->exp)->left);
            std::string cst = int2str(((T::ConstExp *) ((T::BinopExp *) exp->exp)->right)->consti);
            // M[FP + c]
            if (left == F::FP()) {
                emit(new AS::OperInstr(
                        "movq " + inframe(((T::ConstExp *) ((T::BinopExp *) exp->exp)->right)->consti) +
                        ", `d0 #159",
                        new TEMP::TempList(r, nullptr),
                        new TEMP::TempList(left, nullptr),
                        nullptr));
            } else {
                emit(new AS::OperInstr(
                        "movq " + cst + "(`s0), `d0 #162",
                        new TEMP::TempList(r, nullptr), new TEMP::TempList(left, nullptr), nullptr));
            }
            //  M[c + Reg]
        } else if (exp->exp->kind == T::Exp::BINOP && ((T::BinopExp *) exp->exp)->op == T::PLUS_OP
                   && ((T::BinopExp *) exp->exp)->left->kind == T::Exp::CONST) {
            TEMP::Temp *right = munchExp(((T::BinopExp *) exp->exp)->right);
            std::string cst = int2str(((T::ConstExp *) ((T::BinopExp *) exp->exp)->left)->consti);
            // M[c + FP]
            if (right == F::FP()) {
                emit(new AS::OperInstr(
                        "movq " + inframe(((T::ConstExp *) ((T::BinopExp *) exp->exp)->left)->consti) +
                        ", `d0 #176",
                        new TEMP::TempList(r, nullptr), new TEMP::TempList(right, nullptr), nullptr));
            } else {
                emit(new AS::OperInstr(
                        "movq " + cst + "(`s0), `d0 #180", new TEMP::TempList(r, nullptr),
                        new TEMP::TempList(right, nullptr),
                        nullptr));
            }
        } else if (exp->exp->kind == T::Exp::CONST) {
            // M[c]
            std::string cst = int2str(((T::ConstExp *) exp->exp)->consti);
            emit(new AS::OperInstr("movq " + cst + ", `d0 #187", new TEMP::TempList(r, nullptr),
                                   nullptr, nullptr));
        } else {
            // M[Reg]
            TEMP::Temp *expt = munchExp(exp->exp);
            if (expt == F::FP()) {
                emit(new AS::OperInstr("movq " + inframe(0) + ", `d0 #193",
                                       new TEMP::TempList(r, nullptr),
                                       new TEMP::TempList(expt, nullptr), nullptr));
            } else {
                emit(new AS::OperInstr("movq (`s0), `d0 #197", new TEMP::TempList(r, nullptr),
                                       new TEMP::TempList(expt, nullptr), nullptr));
            }
        }
        return r;
    }

    TEMP::Temp *munchBinopExp(T::BinopExp *exp) {
//        PLUS_OP, MINUS_OP, MUL_OP, DIV_OP, AND_OP, OR_OP, LSHIFT_OP, RSHIFT_OP, ARSHIFT_OP, XOR_OP
        switch (exp->op) {
            case T::PLUS_OP: {
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                // Reg + c
                if (exp->right->kind == T::Exp::CONST) {
                    TEMP::Temp *left = munchExp(exp->left);
                    if (left == F::FP()) {
                        emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #213", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr), nullptr));
                    } else {
                        // TODO: REMOVE THIS
                        emit(new AS::MoveInstr("movq `s0, `d0 #216", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
//                        r = left;
                    }
                    std::string cst = int2str(((T::ConstExp *) exp->right)->consti);
                    emit(new AS::OperInstr("addq $" + cst + ", `d0 #222",
                                           new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(r, nullptr), nullptr));
                } else if (exp->left->kind == T::Exp::CONST) {
                    // c + Reg
                    TEMP::Temp *right = munchExp(exp->right);
                    if (right == F::FP()) {
                        emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #229", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(right, nullptr), nullptr));
                    } else {
                        // TODO: REMOVE THIS
                        emit(new AS::MoveInstr("movq `s0, `d0 #233", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(right, nullptr)));
//                        r = right;
                    }
                    std::string cst = int2str(((T::ConstExp *) exp->left)->consti);
                    emit(new AS::OperInstr("addq $" + cst + ", `d0 #238",
                                           new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(r, nullptr), nullptr));
                } else {
                    // Reg + Reg
                    TEMP::Temp *left = munchExp(exp->left);
                    TEMP::Temp *right = munchExp(exp->right);
                    if (left == F::FP()) {
                        emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #245",
                                               new TEMP::TempList(r, nullptr), new TEMP::TempList(left, nullptr),
                                               nullptr));
                    } else {
                        // lab5 bug
                        emit(new AS::MoveInstr("movq `s0, `d0 #233", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
                    }
                    if (right == F::FP()) {
                        TEMP::Temp *l = TEMP::Temp::NewTemp();
                        emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #253",
                                               new TEMP::TempList(l, nullptr),
                                               new TEMP::TempList(right, nullptr), nullptr));
                        emit(new AS::OperInstr("addq `s1, `d0 #256", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(r, new TEMP::TempList(l,
                                                                                        nullptr)), nullptr));
                    } else {
                        emit(new AS::OperInstr("addq `s1, `d0 #260", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(r, new TEMP::TempList(right,
                                                                                        nullptr)), nullptr));
                    }
                }
                return r;
            }

            case T::MINUS_OP: {
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                // Reg - c
                if (exp->right->kind == T::Exp::CONST) {
                    TEMP::Temp *left = munchExp(exp->left);
                    if (left == F::FP()) {
                        emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #274", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr), nullptr));
                    } else {
//                        r = left;
                        emit(new AS::MoveInstr("movq `s0, `d0 #233", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
                    }
                    std::string cst = int2str(((T::ConstExp *) exp->right)->consti);
                    emit(new AS::OperInstr("subq $" + cst + ", `d0 #280",
                                           new TEMP::TempList(r, nullptr), nullptr, nullptr));

                } else {
                    TEMP::Temp *left = munchExp(exp->left);
                    TEMP::Temp *right = munchExp(exp->right);
                    if (left == F::FP()) {
                        emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #287", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr), nullptr));
                    } else {
                        emit(new AS::MoveInstr("movq `s0, `d0 #233", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
                    }
                    if (right == F::FP()) {
                        TEMP::Temp *l = TEMP::Temp::NewTemp();
                        emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #294",
                                               new TEMP::TempList(l, nullptr),
                                               new TEMP::TempList(right, nullptr), nullptr));
                        emit(new AS::OperInstr("subq `s1, `d0 #297", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(r, new TEMP::TempList(l, nullptr)),
                                               nullptr));
                    } else {
                        emit(new AS::OperInstr("subq `s1, `d0 #301", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(r, new TEMP::TempList(right, nullptr)), nullptr));
                    }
                }
                return r;
            }

            case T::MUL_OP: {
                // result stored in RAX() RDX(), trashed
                TEMP::Temp *left = munchExp(exp->left);
                TEMP::Temp *right = munchExp(exp->right);
                emit(new AS::MoveInstr("movq `s0, `d0 #313", new TEMP::TempList(F::RAX(), nullptr),
                                       new TEMP::TempList(left,
                                                          nullptr)));
                emit(new AS::OperInstr("imulq `s0 #316",
                                       new TEMP::TempList(F::RDX(), new TEMP::TempList(F::RAX(), nullptr)),
                                       new TEMP::TempList(right, new TEMP::TempList(F::RAX(),
                                                                                    nullptr)), nullptr));
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                emit(new AS::MoveInstr("movq `s0, `d0 #313", new TEMP::TempList(r, nullptr),
                                       new TEMP::TempList(F::RAX(),
                                                          nullptr)));
                return r;
            }

            case T::DIV_OP: {
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                TEMP::Temp *left = munchExp(exp->left);
                TEMP::Temp *right = munchExp(exp->right);
                emit(new AS::MoveInstr("movq `s0, `d0 #326", new TEMP::TempList(F::RAX(), nullptr),
                                       new TEMP::TempList(left, nullptr)));
                emit(new AS::OperInstr("cqto #328", new TEMP::TempList(F::RAX(),
                                                                       new TEMP::TempList(F::RDX(), nullptr)),
                                       new TEMP::TempList(F::RAX(), nullptr), nullptr));
                emit(new AS::OperInstr("idivq `s0 #331",
                                       new TEMP::TempList(F::RAX(), new TEMP::TempList(F::RDX(), nullptr)),
                                       new TEMP::TempList(right, new TEMP::TempList(F::RAX(),
                                                                                    new TEMP::TempList(F::RDX(),
                                                                                                       nullptr))),
                                       nullptr));
                emit(new AS::MoveInstr("movq `s0, `d0 #326", new TEMP::TempList(r, nullptr),
                                       new TEMP::TempList(F::RAX(), nullptr)));
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
        emit(new AS::OperInstr("jmp `j0 #351", nullptr, nullptr, new AS::Targets(stm->jumps)));

    }

    void munchCJumpStm(T::CjumpStm *stm) {
        TEMP::Temp *left = munchExp(stm->left);
        TEMP::Temp *right = munchExp(stm->right);
        emit(new AS::OperInstr("cmpq `s0, `s1 #361", nullptr,
                               new TEMP::TempList(right, new TEMP::TempList(left, nullptr)),
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
            case T::LT_OP:
                jop = "jl";
                break;
            case T::LE_OP:
                jop = "jle";
                break;
            case T::GE_OP:
                jop = "jge";
                break;
        }
        emit(new AS::OperInstr(jop + " `j0 #397", nullptr, nullptr,
                               new AS::Targets(new TEMP::LabelList(stm->true_label,
                                                                   new TEMP::LabelList(stm->false_label,
                                                                                       nullptr)))));

    }

// TODO: ALLOCAL ESCAPE
    void munchMoveStm(T::MoveStm *stm) {
        T::Exp::Kind dstk = stm->dst->kind;
        T::Exp::Kind srck = stm->src->kind;
        // reg -> reg
        // mem -> reg
        // reg -> mem
        // imm -> reg
        // imm -> mem
        if (dstk == T::Exp::TEMP) {
            TEMP::Temp *src = munchExp(stm->src);
            TEMP::Temp *dst = ((T::TempExp *) stm->dst)->temp;
            if (src == F::FP()) {
                emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #415",
                                       new TEMP::TempList(dst, nullptr),
                                       new TEMP::TempList(src, nullptr), nullptr));
            } else {
                emit(new AS::MoveInstr("movq `s0, `d0 #419",
                                       new TEMP::TempList(dst, nullptr),
                                       new TEMP::TempList(src,
                                                          nullptr)));
            }
        } else if (dstk == T::Exp::MEM) {
            T::Exp *exp = ((T::MemExp *) stm->dst)->exp;
            // M[Reg + c] <- Reg
            if (exp->kind == T::Exp::BINOP
                && ((T::BinopExp *) exp)->op == T::PLUS_OP
                && ((T::BinopExp *) exp)->right->kind == T::Exp::CONST) {
                TEMP::Temp *src = munchExp(stm->src);
                TEMP::Temp *left = munchExp(((T::BinopExp *) exp)->left);
                std::string cst = int2str(((T::ConstExp *) (((T::BinopExp *) exp)->right))->consti);
                if (src == F::FP()) {
                    TEMP::Temp *temp = TEMP::Temp::NewTemp();
                    emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #442",
                                           new TEMP::TempList(temp, nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                    src = temp;
                }
                if (left == F::FP()) {
                    emit(new AS::OperInstr(
                            "movq `s1, " + inframe(((T::ConstExp *) (((T::BinopExp *) exp)->right))->consti) +
                            " #450",
                            nullptr, new TEMP::TempList(left, new TEMP::TempList(src, nullptr)), nullptr));
                } else {
                    emit(new AS::OperInstr(
                            "movq `s1, " + cst + "(`s0) #454",
                            nullptr, new TEMP::TempList(left, new TEMP::TempList(src, nullptr)), nullptr));
                }
                // M[c + Reg] <- Reg
            } else if (exp->kind == T::Exp::BINOP
                       && ((T::BinopExp *) exp)->op == T::PLUS_OP
                       && ((T::BinopExp *) exp)->left->kind == T::Exp::CONST) {
                TEMP::Temp *src = munchExp(stm->src);
                TEMP::Temp *right = munchExp(((T::BinopExp *) exp)->right);
                std::string cst = int2str(((T::ConstExp *) (((T::BinopExp *) exp)->left))->consti);
                if (src == F::FP()) {
                    TEMP::Temp *temp = TEMP::Temp::NewTemp();
                    emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #477",
                                           new TEMP::TempList(temp, nullptr),
                                           new TEMP::TempList(src,
                                                              nullptr), nullptr));
                    src = temp;
                }

                if (right == F::FP()) {
                    emit(new AS::OperInstr(
                            "movq `s1, " + inframe(((T::ConstExp *) (((T::BinopExp *) exp)->left))->consti) +
                            " #486",
                            nullptr, new TEMP::TempList(right, new TEMP::TempList(src, nullptr)), nullptr));
                } else {
                    emit(new AS::OperInstr(
                            "movq `s1, " + cst + "(`s0) #490",
                            nullptr, new TEMP::TempList(right, new TEMP::TempList(src, nullptr)), nullptr));
                }
                // M[Reg] <- M[Reg]
            } else if (stm->src->kind == T::Exp::MEM) {
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                TEMP::Temp *src = munchExp(((T::MemExp *) stm->src)->exp);
                TEMP::Temp *dst = munchExp(exp);
                if (src == F::FP()) {
                    emit(new AS::OperInstr("movq " + inframe(0) + ", `d0 #488", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                } else {
                    emit(new AS::OperInstr("movq (`s0), `d0 #491", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                }
                if (dst == F::FP()) {
                    emit(new AS::OperInstr("movq `s0, " + inframe(0) + " #495", nullptr,
                                           new TEMP::TempList(r, new TEMP::TempList(dst, nullptr)), nullptr));
                } else {
                    emit(new AS::OperInstr("movq `s0, (`s1) #498", nullptr,
                                           new TEMP::TempList(r, new TEMP::TempList(dst, nullptr)), nullptr));
                }
            } else if (exp->kind == T::Exp::CONST) {
                assert(0);
                TEMP::Temp *src = munchExp(stm->src);
                emit(new AS::OperInstr("movq `s0, (" + int2str(((T::ConstExp *) exp)->consti) + ") #504",
                                       nullptr, new TEMP::TempList(src, nullptr), nullptr));
            } else {
                TEMP::Temp *src = munchExp(stm->src);
                TEMP::Temp *mexp = munchExp(exp);
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                if (src == F::FP()) {
                    emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #511", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                } else {
                    r = src;
                }
                if (mexp == F::FP()) {
                    emit(new AS::OperInstr("movq `s0, " + inframe(0) + " #518", nullptr,
                                           new TEMP::TempList(r, new TEMP::TempList(mexp, nullptr)),
                                           nullptr));
                } else {
                    emit(new AS::OperInstr("movq `s0, (`s1) #522", nullptr,
                                           new TEMP::TempList(r, new TEMP::TempList(mexp, nullptr)),
                                           nullptr));
                }
            }
        }
    }

    void munchExpStm(T::ExpStm *stm) {
        munchExp(stm->exp);
    }
}  // namespace CG