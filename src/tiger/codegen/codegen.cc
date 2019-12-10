#include "tiger/codegen/codegen.h"
#include <map>

namespace CG {
#define p(s, ...) do { \
  printf(s, ##__VA_ARGS__); \
  fflush(stdout); \
  } while (0);
    static std::string framesize;
    static int pl;

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

//    std::string fs() {
//        return framesize;
//    }
    static std::map<TEMP::Temp *, int> *toff;

    std::string int2str(int i) {
        return i >= 0 ? std::to_string(i) : "-" + std::to_string(-i);
    }

    std::string inframe(int offset) {

        return "(" + framesize + " + " + int2str(pl + offset) + ")" + "(%rsp)";
    }

    // lab5
    bool sigReg(TEMP::Temp *temp) {
        // TODO: CHANGE IT
        return temp == F::FP() || temp == F::RAX() || temp == F::RSP() || temp == F::RBX()
               || temp == F::RCX() || temp == F::RDI() || temp == F::RSI() || temp == F::RDX() || temp == F::RBP();
    }
    //

    //lab5
    std::string inframetemp(TEMP::Temp *temp) {
        return "(" + int2str((toff->at(temp) - 1) * 8 + pl) + ")(%rsp)";
    }

    bool isframetemp(TEMP::Temp *temp) {
        return toff->find(temp) != toff->end();
    }
    //

    AS::InstrList *Codegen(F::Frame *f, T::StmList *stmList) {
        //lab5
        TEMP::TempList *tps = TEMP::tempList(nullptr);
        toff = new std::map<TEMP::Temp *, int>{};
        int cnt = 0;
        TEMP::Map *map = F::regmap();
        for (; tps; tps = tps->tail) {
            if (!map->Look(tps->head)) {
                cnt++;
                toff->insert(std::pair<TEMP::Temp *, int>{tps->head, cnt});
            }
        }
        //
        instrList = new AS::InstrList(nullptr, nullptr);
        tail = instrList;
        framesize = TEMP::LabelString(f->label) + "_framesize";
        pl = 0;
        for (; stmList; stmList = stmList->tail) {
            munchStm(stmList->head);
        }
        // TODO: WTH?
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
            //lab5
            if (isframetemp(head)) {
                emit(new AS::MoveInstr("movq " + inframetemp(head) + ", `d0 #108", new TEMP::TempList(reg, nullptr),
                                       new TEMP::TempList(F::RSP(), nullptr)));
                //
            } else {
                emit(new AS::MoveInstr("movq `s0, `d0 #112", new TEMP::TempList(reg, nullptr),
                                       new TEMP::TempList(head, nullptr)));
            }
            return new TEMP::TempList(reg, munchArgs(pos + 1, args->tail, name));
        } else {
            TEMP::Temp *head = munchExp(args->head);
            //lab5
            if (isframetemp(head)) {
                emit(new AS::MoveInstr("movq " + inframetemp(head) + ", `d0 #120",
                                       new TEMP::TempList(F::RBX(), nullptr),
                                       new TEMP::TempList(F::RSP(), nullptr)));
                emit(new AS::MoveInstr("movq `s0, " + std::to_string(8 * (pos + 1)) + "(%rsp) #123", nullptr,
                                       new TEMP::TempList(F::RBX(), new TEMP::TempList(F::RSP(),
                                                                                       nullptr))));
                //
            } else {
                emit(new AS::MoveInstr("movq `s0, " + std::to_string(8 * (pos + 1)) + "(%rsp) #128", nullptr,
                                       new TEMP::TempList(head, new TEMP::TempList(F::RSP(),
                                                                                   nullptr))));
            }
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
        emit(new AS::OperInstr("movq $" + int2str(exp->consti) + ", `d0 #186", new TEMP::TempList(r, nullptr),
                               nullptr, nullptr));
        return r;
    }

    TEMP::Temp *munchCallExp(T::CallExp *exp) {
        TEMP::Temp *r = TEMP::Temp::NewTemp();
        uint len = runtime(((T::NameExp *) (exp->fun))->name->Name()) ? 0 : listLen(exp->args->tail);
        pl += 8 * (len + 1);
        emit(new AS::OperInstr("subq $" + std::to_string(8 * (len + 1)) + ", %rsp #195",
                               new TEMP::TempList(F::RSP(), nullptr),
                               new TEMP::TempList(F::RSP(),
                                                  nullptr),
                               nullptr));
        TEMP::TempList *args = munchArgs(0, exp->args->tail, ((T::NameExp *) (exp->fun))->name->Name());
        if (exp->args->head->kind == T::Exp::TEMP) {
            emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #202", new TEMP::TempList(r, nullptr),
                                   new TEMP::TempList(F::RSP(),
                                                      nullptr), nullptr));
            emit(new AS::OperInstr("movq `s0, (%rsp) #205", nullptr,
                                   new TEMP::TempList(r, new TEMP::TempList(F::RSP(), nullptr)), nullptr));
        } else {
            TEMP::Temp *head = munchExp(exp->args->head);
            //lab5
            if (isframetemp(head)) {
                emit(new AS::MoveInstr("movq " + inframetemp(head) + ", `d0 #211",
                                       new TEMP::TempList(F::RBX(), nullptr),
                                       new TEMP::TempList(F::RSP(), nullptr)));
                emit(new AS::OperInstr("movq `s0, (%rsp) #214", nullptr,
                                       new TEMP::TempList(F::RBX(), new TEMP::TempList(F::RSP(), nullptr)), nullptr));
                //
            } else {
                emit(new AS::OperInstr("movq `s0, (%rsp) #218", nullptr,
                                       new TEMP::TempList(head, new TEMP::TempList(F::RSP(), nullptr)), nullptr));
            }
        }
        // TODO: CHANGE IN LAB6
        if (!runtime(((T::NameExp *) (exp->fun))->name->Name())) {
            emit(new AS::OperInstr("callq " + TEMP::LabelString(((T::NameExp *) exp->fun)->name) + " #224", nullptr,
                                   nullptr, nullptr));

        } else {
            emit(new AS::OperInstr("callq " + TEMP::LabelString(((T::NameExp *) exp->fun)->name) + " #228", nullptr,
                                   nullptr, nullptr));
        }
        emit(new AS::OperInstr("addq $" + std::to_string(8 * (len + 1)) + ", %rsp #231",
                               new TEMP::TempList(F::RSP(), nullptr),
                               new TEMP::TempList(F::RSP(),
                                                  nullptr),
                               nullptr));
        pl -= 8 * (len + 1);
        assert(pl == 0);
        return F::RV();
    }

    TEMP::Temp *munchNameExp(T::NameExp *exp) {
        TEMP::Temp *temp = TEMP::Temp::NewTemp();
        emit(new AS::OperInstr("leaq " + TEMP::LabelString(exp->name) + "(%rip), `d0 #243",
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
            std::string cst = int2str(((T::ConstExp *) ((T::BinopExp *) exp->exp)->right)->consti);
            if (left == F::FP()) {
                emit(new AS::OperInstr(
                        "movq " + inframe(((T::ConstExp *) ((T::BinopExp *) exp->exp)->right)->consti) + ", `d0 #262",
                        new TEMP::TempList(r, nullptr),
                        new TEMP::TempList(left, nullptr),
                        nullptr));
            }
                //lab5
            else if (isframetemp(left)) {
                emit(new AS::MoveInstr("movq " + inframetemp(left) + ", `d0 #269",
                                       new TEMP::TempList(F::RBX(), nullptr),
                                       new TEMP::TempList(F::RSP(), nullptr)));
                emit(new AS::OperInstr(
                        "movq " + cst + "(`s0), `d0 #273",
                        new TEMP::TempList(r, nullptr), new TEMP::TempList(F::RBX(), nullptr), nullptr));
                //
            } else {
                emit(new AS::OperInstr(
                        "movq " + cst + "(`s0), `d0 #278",
                        new TEMP::TempList(r, nullptr), new TEMP::TempList(left, nullptr), nullptr));
            }
        } else if (exp->exp->kind == T::Exp::BINOP && ((T::BinopExp *) exp->exp)->op == T::PLUS_OP
                   && ((T::BinopExp *) exp->exp)->left->kind == T::Exp::CONST) {
            TEMP::Temp *right = munchExp(((T::BinopExp *) exp->exp)->right);
            std::string cst = int2str(((T::ConstExp *) ((T::BinopExp *) exp->exp)->left)->consti);
            if (right == F::FP()) {
                emit(new AS::OperInstr(
                        "movq " + inframe(((T::ConstExp *) ((T::BinopExp *) exp->exp)->left)->consti) + ", `d0 #287",
                        new TEMP::TempList(r, nullptr), new TEMP::TempList(right, nullptr), nullptr));
            }            //lab5
            else if (isframetemp(right)) {
                emit(new AS::MoveInstr("movq " + inframetemp(right) + ", `d0 #291",
                                       new TEMP::TempList(F::RBX(), nullptr),
                                       new TEMP::TempList(F::RSP(), nullptr)));
                emit(new AS::OperInstr(
                        "movq " + cst + "(`s0), `d0 #295", new TEMP::TempList(r, nullptr),
                        new TEMP::TempList(F::RBX(), nullptr),
                        nullptr));
                //
            } else {
                emit(new AS::OperInstr(
                        "movq " + cst + "(`s0), `d0 #298", new TEMP::TempList(r, nullptr),
                        new TEMP::TempList(right, nullptr),
                        nullptr));
            }
        } else if (exp->exp->kind == T::Exp::CONST) {
            // wth??
            std::string cst = int2str(((T::ConstExp *) exp->exp)->consti);
            emit(new AS::OperInstr("movq ($" + cst + "), `d0 #308", new TEMP::TempList(r, nullptr),
                                   nullptr, nullptr));
        } else {
            TEMP::Temp *expt = munchExp(exp->exp);
            if (expt == F::FP()) {
                emit(new AS::OperInstr("movq " + inframe(0) + ", `d0 #313", new TEMP::TempList(r, nullptr),
                                       new TEMP::TempList(expt, nullptr), nullptr));
            }//lab5
            else if (isframetemp(expt)) {
                emit(new AS::MoveInstr("movq " + inframetemp(expt) + ", `d0 #317",
                                       new TEMP::TempList(r, nullptr),
                                       new TEMP::TempList(F::RSP(), nullptr)));
                //
            } else {
                emit(new AS::OperInstr("movq (`s0), `d0 #323", new TEMP::TempList(r, nullptr),
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
                if (exp->right->kind == T::Exp::CONST) {
                    TEMP::Temp *left = munchExp(exp->left);
                    if (left == F::FP()) {
                        emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #338", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
                    } else if (isframetemp(left)) {
                        emit(new AS::MoveInstr("movq " + inframetemp(left) + ", `d0 #341",
                                               new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(F::RSP(), nullptr)));
                        //
                    } else {
                        emit(new AS::MoveInstr("movq `s0, `d0 #345", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
                    }
                    std::string cst = int2str(((T::ConstExp *) exp->right)->consti);
                    emit(new AS::OperInstr("addq $" + cst + ", `d0 #349",
                                           new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(r,
                                                              nullptr),
                                           nullptr));
                } else if (exp->left->kind == T::Exp::CONST) {
                    TEMP::Temp *right = munchExp(exp->right);
                    if (right == F::FP()) {
                        emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #357", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(right, nullptr)));
                    }//lab5
                    else if (isframetemp(right)) {
                        emit(new AS::MoveInstr("movq " + inframetemp(right) + ", `d0 #361",
                                               new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(F::RSP(), nullptr)));
                        //
                    } else {
                        emit(new AS::MoveInstr("movq `s0, `d0 #366", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(right, nullptr)));
                    }
                    std::string cst = int2str(((T::ConstExp *) exp->left)->consti);
                    emit(new AS::OperInstr("addq $" + cst + ", `d0 #370",
                                           new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(r, nullptr), nullptr));
                } else {
                    TEMP::Temp *left = munchExp(exp->left);
                    if (!sigReg(left) && !isframetemp(left)) {
                        emit(new AS::OperInstr("pushq `s0", new TEMP::TempList(F::RSP(), nullptr),
                                               new TEMP::TempList(left,
                                                                  nullptr), nullptr));
                        pl += 8;
                    }
                    TEMP::Temp *right = munchExp(exp->right);
                    if (!sigReg(left) && !isframetemp(left)) {
                        emit(new AS::OperInstr("popq `d0", new TEMP::TempList(F::RCX(),
                                                                              new TEMP::TempList(F::RSP(), nullptr)),
                                               new TEMP::TempList(F::RSP(), nullptr), nullptr));
                        left = F::RCX();
                        pl -= 8;
                    }
                    if (left == F::FP()) {
                        emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #390", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left,
                                                                  nullptr)));
                    }//lab5
                    else if (isframetemp(left)) {
                        emit(new AS::MoveInstr("movq " + inframetemp(left) + ", `d0 #395",
                                               new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(F::RSP(), nullptr)));
                        //
                    } else {
                        emit(new AS::MoveInstr("movq `s0, `d0 #400", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left,
                                                                  nullptr)));
                    }
                    if (right == F::FP()) {
                        emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #405",
                                               new TEMP::TempList(F::RBX(), nullptr),
                                               new TEMP::TempList(right,
                                                                  nullptr)));
                        emit(new AS::OperInstr("addq `s1, `d0 #409", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(r, new TEMP::TempList(F::RBX(),
                                                                                        nullptr)),
                                               nullptr));
                    }//lab5
                    else if (isframetemp(right)) {
                        emit(new AS::MoveInstr("movq " + inframetemp(right) + ", `d0 #415",
                                               new TEMP::TempList(F::RBX(), nullptr),
                                               new TEMP::TempList(F::RSP(), nullptr)));

                        emit(new AS::OperInstr("addq `s1, `d0 #419", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(r, new TEMP::TempList(F::RBX(),
                                                                                        nullptr)),
                                               nullptr));
                        //
                    } else {
                        emit(new AS::OperInstr("addq `s1, `d0 #425", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(r, new TEMP::TempList(right,
                                                                                        nullptr)),
                                               nullptr));
                    }
                }
                return r;
            }

            case T::MINUS_OP: {
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                if (exp->right->kind == T::Exp::CONST) {
                    TEMP::Temp *left = munchExp(exp->left);

                    if (left == F::FP()) {
                        emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #440", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left,
                                                                  nullptr)));
                    } //lab5
                    else if (isframetemp(left)) {
                        emit(new AS::MoveInstr("movq " + inframetemp(left) + ", `d0 #445",
                                               new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(F::RSP(), nullptr)));
                        //
                    } else {
                        emit(new AS::MoveInstr("movq `s0, `d0 #449", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left,
                                                                  nullptr)));
                    }
                    std::string cst = int2str(((T::ConstExp *) exp->right)->consti);
                    emit(new AS::OperInstr("subq $" + cst + ", `d0 #455",
                                           new TEMP::TempList(r, nullptr), nullptr, nullptr));

                } else {
                    TEMP::Temp *left = munchExp(exp->left);
                    if (!sigReg(left) && !isframetemp(left)) {
                        emit(new AS::OperInstr("pushq `s0 #461", new TEMP::TempList(F::RSP(), nullptr),
                                               new TEMP::TempList(left,
                                                                  nullptr), nullptr));
                        pl += 8;
                    }
                    TEMP::Temp *right = munchExp(exp->right);
                    if (!sigReg(left) && !isframetemp(left)) {
                        emit(new AS::OperInstr("popq `d0 #468", new TEMP::TempList(F::RCX(),
                                                                              new TEMP::TempList(F::RSP(), nullptr)),
                                               new TEMP::TempList(F::RSP(), nullptr), nullptr));
                        left = F::RCX();
                        pl -= 8;
                    }

                    if (left == F::FP()) {
                        emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #476", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
                    }//lab5
                    else if (isframetemp(left)) {
                        emit(new AS::MoveInstr("movq " + inframetemp(left) + ", `d0 #480",
                                               new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(F::RSP(), nullptr)));
                        //
                    } else {
                        emit(new AS::MoveInstr("movq `s0, `d0 #485", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(left, nullptr)));
                    }

                    if (right == F::FP()) {
                        emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #490",
                                               new TEMP::TempList(F::RBX(), nullptr),
                                               new TEMP::TempList(right, nullptr)));
                        emit(new AS::OperInstr("subq `s1, `d0 #491", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(r, new TEMP::TempList(F::RBX(),
                                                                                        nullptr)), nullptr));
                    }//lab5
                    else if (isframetemp(right)) {
                        emit(new AS::MoveInstr("movq " + inframetemp(right) + ", `d0 #498",
                                               new TEMP::TempList(F::RBX(), nullptr),
                                               new TEMP::TempList(F::RSP(), nullptr)));
                        emit(new AS::OperInstr("subq `s1, `d0 #501", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(r, new TEMP::TempList(F::RBX(),
                                                                                        nullptr)), nullptr));
                        //
                    } else {
                        emit(new AS::OperInstr("subq `s1, `d0 #506", new TEMP::TempList(r, nullptr),
                                               new TEMP::TempList(r, new TEMP::TempList(right,
                                                                                        nullptr)), nullptr));
                    }
                }
                return r;
            }

            case T::MUL_OP: {
                // result stored in RAX() RDX(), trashed
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                TEMP::Temp *left = munchExp(exp->left);
                if (!sigReg(left) && !isframetemp(left)) {
                    emit(new AS::OperInstr("pushq `s0 #519", new TEMP::TempList(F::RSP(), nullptr), new TEMP::TempList(left,
                                                                                                                  nullptr),
                                           nullptr));
                    pl += 8;
                }
                TEMP::Temp *right = munchExp(exp->right);
                if (!sigReg(left) && !isframetemp(left)) {
                    emit(new AS::OperInstr("popq `d0 #526", new TEMP::TempList(F::RCX(),
                                                                          new TEMP::TempList(F::RSP(), nullptr)),
                                           new TEMP::TempList(F::RSP(), nullptr), nullptr));
                    left = F::RCX();
                    pl -= 8;
                }
                if (isframetemp(left)) {
                    emit(new AS::MoveInstr("movq " + inframetemp(left) + ", `d0 #533",
                                           new TEMP::TempList(F::RBX(), nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr)));
                    left = F::RBX();
                    //
                }
                if (isframetemp(right)) {
                    emit(new AS::MoveInstr("movq " + inframetemp(right) + ", `d0 #540",
                                           new TEMP::TempList(F::RCX(), nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr)));
                    right = F::RCX();
                    //
                }
                emit(new AS::MoveInstr("movq `s0, `d0 #546", new TEMP::TempList(F::RAX(), nullptr),
                                       new TEMP::TempList(left,
                                                          nullptr)));
                emit(new AS::OperInstr("imulq `s0 #549",
                                       new TEMP::TempList(F::RDX(), new TEMP::TempList(F::RAX(), nullptr)),
                                       new TEMP::TempList(right, new TEMP::TempList(F::RAX(),
                                                                                    nullptr)),
                                       nullptr));
                emit(new AS::MoveInstr("movq `s0, `d0 #555", new TEMP::TempList(r, nullptr),
                                       new TEMP::TempList(F::RAX(),
                                                          nullptr)));
                return r;
            }

            case T::DIV_OP: {
                TEMP::Temp *r = TEMP::Temp::NewTemp();
                TEMP::Temp *left = munchExp(exp->left);
                if (!sigReg(left) && !isframetemp(left)) {
                    emit(new AS::OperInstr("pushq `s0 #564", new TEMP::TempList(F::RSP(), nullptr), new TEMP::TempList(left,
                                                                                                                  nullptr),
                                           nullptr));
                    pl += 8;
                }
                TEMP::Temp *right = munchExp(exp->right);
                if (!sigReg(left) && !isframetemp(left)) {
                    emit(new AS::OperInstr("popq `d0 #571", new TEMP::TempList(F::RCX(),
                                                                          new TEMP::TempList(F::RSP(), nullptr)),
                                           new TEMP::TempList(F::RSP(), nullptr), nullptr));
                    left = F::RCX();
                    pl -= 8;
                }
                //lab5
                if (isframetemp(left)) {
                    emit(new AS::MoveInstr("movq " + inframetemp(left) + ", `d0 #579",
                                           new TEMP::TempList(F::RBX(), nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr)));
                    left = F::RBX();
                    //
                }
                //lab5
                if (isframetemp(right)) {
                    emit(new AS::MoveInstr("movq " + inframetemp(right) + ", `d0 #587",
                                           new TEMP::TempList(F::RCX(), nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr)));
                    right = F::RCX();
                    //
                }
                emit(new AS::MoveInstr("movq `s0, `d0 #593", new TEMP::TempList(F::RAX(), nullptr),
                                       new TEMP::TempList(left,
                                                          nullptr)));
                emit(new AS::OperInstr("cqto #596", new TEMP::TempList(F::RAX(), new TEMP::TempList(F::RDX(), nullptr)),
                                       new TEMP::TempList(F::RAX(), nullptr), nullptr));
                emit(new AS::OperInstr("idivq `s0 #598",
                                       new TEMP::TempList(F::RAX(), new TEMP::TempList(F::RDX(), nullptr)),
                                       new TEMP::TempList(right, new TEMP::TempList(F::RAX(),
                                                                                    new TEMP::TempList(F::RDX(),
                                                                                                       nullptr))),
                                       new AS::Targets(nullptr)));
                emit(new AS::MoveInstr("movq `s0, `d0 #604", new TEMP::TempList(r, nullptr),
                                       new TEMP::TempList(F::RAX(),
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
        emit(new AS::OperInstr("jmp `j0 #624", nullptr, nullptr, new AS::Targets(stm->jumps)));

    }

    void munchCJumpStm(T::CjumpStm *stm) {
        TEMP::Temp *left = munchExp(stm->left);
        if (!sigReg(left) && !isframetemp(left)) {
            emit(new AS::OperInstr("pushq `s0 #631", new TEMP::TempList(F::RSP(), nullptr), new TEMP::TempList(left,
                                                                                                          nullptr),
                                   nullptr));
            pl += 8;
        }
        TEMP::Temp *right = munchExp(stm->right);
        if (!sigReg(left) && !isframetemp(left)) {
            emit(new AS::OperInstr("popq `d0 #638", new TEMP::TempList(F::RBX(),
                                                                  new TEMP::TempList(F::RSP(), nullptr)),
                                   new TEMP::TempList(F::RSP(), nullptr), nullptr));
            left = F::RBX();
            pl -= 8;
        }
        //lab5
        if (isframetemp(left)) {
            emit(new AS::MoveInstr("movq " + inframetemp(left) + ", `d0 #646", new TEMP::TempList(F::RBX(), nullptr),
                                   new TEMP::TempList(F::RSP(), nullptr)));
            //
        } else {
            if (left != F::RBX()) {
                emit(new AS::MoveInstr("movq `s0, `d0 #651", new TEMP::TempList(F::RBX(), nullptr), new TEMP::TempList(left,

                                                                                                                  nullptr)));
            }
        }
        //lab5
        if (isframetemp(right)) {
            emit(new AS::MoveInstr("movq " + inframetemp(right) + ", `d0 #658", new TEMP::TempList(F::RCX(), nullptr),
                                   new TEMP::TempList(F::RSP(), nullptr)));
            //
        } else {
            emit(new AS::MoveInstr("movq `s0, `d0 #662", new TEMP::TempList(F::RCX(), nullptr),
                                   new TEMP::TempList(right,
                                                      nullptr)));
        }
        emit(new AS::OperInstr("cmpq `s0, `s1 #666", nullptr,
                               new TEMP::TempList(F::RCX(), new TEMP::TempList(F::RBX(), nullptr)),
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
        emit(new AS::OperInstr(jop + " `j0 #702", nullptr, nullptr, new AS::Targets(new TEMP::LabelList(stm->true_label,
                                                                                                        new TEMP::LabelList(
                                                                                                                stm->false_label,
                                                                                                                nullptr)))));

    }

    void munchMoveStm(T::MoveStm *stm) {
        T::Exp::Kind dstk = stm->dst->kind;
        T::Exp::Kind srck = stm->src->kind;
        // reg -> reg
        // mem -> reg
        // reg -> mem
        // imm -> reg
        // imm -> mem
        /*
        TEMP::Temp *src = munchExp(stm->src);
        TEMP::Temp *dst = munchExp(stm->dst);
        //lab5
        if (isframetemp(src)) {
            emit(new AS::MoveInstr("movq " + inframetemp(src) + ", `d0 #494",
                                   new TEMP::TempList(F::RBX(), nullptr),
                                   new TEMP::TempList(F::RSP(), nullptr)));
            src = F::RBX();
            //
        } else if (src == F::FP()) {
            emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #465",
                                   new TEMP::TempList(F::RBX(), nullptr),
                                   new TEMP::TempList(src,
                                                      nullptr)));
            src = F::RBX();
        }

        if (isframetemp(dst)) {
            emit(new AS::MoveInstr("movq `s0, " + inframetemp(dst) + " #667",
                                   nullptr,
                                   new TEMP::TempList(src, new TEMP::TempList(F::RSP(), nullptr))));
        } else if (dst == F::FP()) {
            emit(new AS::MoveInstr("movq `s0, " + inframe(0) + " #671",
                                   nullptr,
                                   new TEMP::TempList(src, new TEMP::TempList(F::RSP(), nullptr))));
        } else {
            emit(new AS::MoveInstr("movq `s0, `d0 #675",
                                   new TEMP::TempList(dst, nullptr),
                                   new TEMP::TempList(src,
                                                      nullptr)));
        }
        */

        if (dstk == T::Exp::TEMP) {
            TEMP::Temp *src = munchExp(stm->src);
            if (!sigReg(src) && !isframetemp(src)) {
                emit(new AS::OperInstr("pushq `s0 #754", new TEMP::TempList(F::RSP(), nullptr),
                        new TEMP::TempList(src, nullptr), nullptr));
                pl += 8;
            }
            TEMP::Temp *dst = ((T::TempExp *) stm->dst)->temp;
            if (!sigReg(src) && !isframetemp(src)) {
                emit(new AS::OperInstr("popq `d0 #760", new TEMP::TempList(F::RBX(),
                                                                      new TEMP::TempList(F::RSP(), nullptr)),
                                       new TEMP::TempList(F::RSP(), nullptr), nullptr));
                src = F::RBX();
                pl -= 8;
            }
            if (src == F::FP()) {
                //lab5
                emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #768",
                                       new TEMP::TempList(F::RBX(), nullptr),
                                       new TEMP::TempList(src,
                                                          nullptr)));
                src = F::RBX();
                //
//                emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #465",
//                                       new TEMP::TempList(((T::TempExp *) stm->dst)->temp, nullptr),
//                                       new TEMP::TempList(src,
//                                                          nullptr)));
            }  //lab5
            else if (isframetemp(src)) {
                emit(new AS::MoveInstr("movq " + inframetemp(src) + ", `d0 #780",
                                       new TEMP::TempList(F::RBX(), nullptr),
                                       new TEMP::TempList(F::RSP(), nullptr)));
                src = F::RBX();
                //
            }
//            else {
//                emit(new AS::MoveInstr("movq `s0,`d0 #470",
//                                       new TEMP::TempList(((T::TempExp *) stm->dst)->temp, nullptr),
//                                       new TEMP::TempList(src,
//                                                          nullptr)));
//            }
            if (dst == F::FP()) {
                assert(0);
            } else if (isframetemp(dst)) {
                emit(new AS::MoveInstr("movq `s0, " + inframetemp(dst) + " #795",
                                       nullptr,
                                       new TEMP::TempList(src, new TEMP::TempList(F::RSP(), nullptr))));
            } else {
                emit(new AS::MoveInstr("movq `s0, `d0 #799",
                                       new TEMP::TempList(dst, nullptr),
                                       new TEMP::TempList(src,
                                                          nullptr)));
            }
        } else if (dstk == T::Exp::MEM) {
            T::Exp *exp = ((T::MemExp *) stm->dst)->exp;
            if (exp->kind == T::Exp::BINOP
                && ((T::BinopExp *) exp)->op == T::PLUS_OP
                && ((T::BinopExp *) exp)->right->kind == T::Exp::CONST) {
                TEMP::Temp *src = munchExp(stm->src);
                if (!sigReg(src) && !isframetemp(src)) {
                    emit(new AS::OperInstr("pushq `s0 #811", new TEMP::TempList(F::RSP(), nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                    pl += 8;
                }
                TEMP::Temp *left = munchExp(((T::BinopExp *) exp)->left);
                if (!sigReg(src) && !isframetemp(src)) {
                    emit(new AS::OperInstr("popq `d0 #817", new TEMP::TempList(F::RBX(),
                                                                          new TEMP::TempList(F::RSP(), nullptr)),
                                           new TEMP::TempList(F::RSP(), nullptr), nullptr));
                    src = F::RBX();
                    pl -= 8;
                }
                std::string cst = int2str(((T::ConstExp *) (((T::BinopExp *) exp)->right))->consti);
                if (src == F::FP()) {
                    emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #825",
                                           new TEMP::TempList(F::RBX(), nullptr),
                                           new TEMP::TempList(src,
                                                              nullptr)));
                    src = F::RBX();
                } else if (isframetemp(src)) {
                    emit(new AS::MoveInstr("movq " + inframetemp(src) + ", `d0 #831",
                                           new TEMP::TempList(F::RBX(), nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr)));
                    src = F::RBX();
                }

                if (left == F::FP()) {
                    emit(new AS::OperInstr(
                            "movq `s1, " + inframe(((T::ConstExp *) (((T::BinopExp *) exp)->right))->consti) + " #839",
                            nullptr, new TEMP::TempList(left, new TEMP::TempList(src, nullptr)), nullptr));
                }// lab5
                else if (isframetemp(left)) {
                    emit(new AS::MoveInstr("movq " + inframetemp(left) + ", `d0 #843",
                                           new TEMP::TempList(F::RCX(), nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr)));
                    emit(new AS::OperInstr(
                            "movq `s1, (" + cst + ")(%rcx) #847",
                            nullptr, new TEMP::TempList(F::RCX(), new TEMP::TempList(src, nullptr)), nullptr));
                    //
                } else {
                    emit(new AS::OperInstr(
                            "movq `s1, " + cst + "(`s0) #852",
                            nullptr, new TEMP::TempList(left, new TEMP::TempList(src, nullptr)), nullptr));
                }
                return;
            } else if (exp->kind == T::Exp::BINOP
                       && ((T::BinopExp *) exp)->op == T::PLUS_OP
                       && ((T::BinopExp *) exp)->left->kind == T::Exp::CONST) {
//                TEMP::Temp *right = munchExp(((T::BinopExp *) exp)->left);
//                TEMP::Temp *src = munchExp(stm->src);
//                std::string cst = int2str(((T::ConstExp *) (((T::BinopExp *) exp)->left))->consti);
//                if (right == F::FP()) {
//                    emit(new AS::OperInstr(
//                            "movq `s1, " + inframe(0) + " #762", nullptr,
//                            new TEMP::TempList(right, new TEMP::TempList(src, nullptr)), nullptr));
//                } else {
//                    emit(new AS::OperInstr(
//                            "movq `s1, " + cst + "(`s0) #766", nullptr,
//                            new TEMP::TempList(right, new TEMP::TempList(src, nullptr)), nullptr));
//                }
                TEMP::Temp *src = munchExp(stm->src);
                if (!sigReg(src) && !isframetemp(src)) {
                    emit(new AS::OperInstr("pushq `s0 #873", new TEMP::TempList(F::RSP(), nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                    pl += 8;
                }
                TEMP::Temp *right = munchExp(((T::BinopExp *) exp)->right);
                if (!sigReg(src) && !isframetemp(src)) {
                    emit(new AS::OperInstr("popq `d0 #879", new TEMP::TempList(F::RBX(),
                                                                          new TEMP::TempList(F::RSP(), nullptr)),
                                           new TEMP::TempList(F::RSP(), nullptr), nullptr));
                    src = F::RBX();
                    pl -= 8;
                }
                std::string cst = int2str(((T::ConstExp *) (((T::BinopExp *) exp)->left))->consti);
                if (src == F::FP()) {
                    emit(new AS::MoveInstr("leaq " + inframe(0) + ", `d0 #887",
                                           new TEMP::TempList(F::RBX(), nullptr),
                                           new TEMP::TempList(src,
                                                              nullptr)));
                    src = F::RBX();
                } else if (isframetemp(src)) {
                    emit(new AS::MoveInstr("movq " + inframetemp(src) + ", `d0 #893",
                                           new TEMP::TempList(F::RBX(), nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr)));
                    src = F::RBX();
                }

                if (right == F::FP()) {
                    emit(new AS::OperInstr(
                            "movq `s1, " + inframe(((T::ConstExp *) (((T::BinopExp *) exp)->left))->consti) + " #901",
                            nullptr, new TEMP::TempList(right, new TEMP::TempList(src, nullptr)), nullptr));
                }// lab5
                else if (isframetemp(right)) {
                    emit(new AS::MoveInstr("movq " + inframetemp(right) + ", `d0 #905",
                                           new TEMP::TempList(F::RCX(), nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr)));
                    emit(new AS::OperInstr(
                            "movq `s1, (" + cst + ")(%rcx) #909",
                            nullptr, new TEMP::TempList(F::RCX(), new TEMP::TempList(src, nullptr)), nullptr));
                    //
                } else {
                    emit(new AS::OperInstr(
                            "movq `s1, " + cst + "(`s0) #914",
                            nullptr, new TEMP::TempList(right, new TEMP::TempList(src, nullptr)), nullptr));
                }
                return;
            } else if (stm->src->kind == T::Exp::MEM) {
                TEMP::Temp *r = F::RBX();
                TEMP::Temp *src = munchExp(((T::MemExp *) stm->src)->exp);
                if (!sigReg(src) && !isframetemp(src)) {
                    emit(new AS::OperInstr("pushq `s0 #922", new TEMP::TempList(F::RSP(), nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                    pl += 8;
                }
                TEMP::Temp *dst = munchExp(exp);
                if (!sigReg(src) && !isframetemp(src)) {
                    emit(new AS::OperInstr("popq `d0 #928", new TEMP::TempList(F::RBX(),
                                                                          new TEMP::TempList(F::RSP(), nullptr)),
                                           new TEMP::TempList(F::RSP(), nullptr), nullptr));
                    src = F::RBX();
                    pl -= 8;
                }

                if (src == F::FP()) {
                    emit(new AS::OperInstr("movq " + inframe(0) + ", `d0 #936", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                } else if (isframetemp(src)) {
                    emit(new AS::OperInstr("movq " + inframetemp(src) + ", `d0 #939",
                                           new TEMP::TempList(F::RCX(), nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr), nullptr));
                    emit(new AS::MoveInstr("movq (%rcx), %rbx #942", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(F::RCX(),
                                                              nullptr)));
                } else {
                    emit(new AS::OperInstr("movq (`s0), `d0 #946", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                }

                if (dst == F::FP()) {
                    emit(new AS::OperInstr("movq `s0, " + inframe(0) + " #951", nullptr,
                                           new TEMP::TempList(r, new TEMP::TempList(dst, nullptr)),
                                           nullptr));
                } else if (isframetemp(dst)) {
                    emit(new AS::OperInstr("movq " + inframetemp(dst) + ", `d0 #955",
                                           new TEMP::TempList(F::RCX(), nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr), nullptr));
                    emit(new AS::OperInstr("movq `s0, (%rcx) #958", nullptr,
                                           new TEMP::TempList(r, new TEMP::TempList(F::RCX(), nullptr)),
                                           nullptr));
                } else {
                    emit(new AS::OperInstr("movq `s0, (`s1) #962", nullptr,
                                           new TEMP::TempList(r, new TEMP::TempList(dst, nullptr)),
                                           nullptr));
                }

            } else if (exp->kind == T::Exp::CONST) {
                assert(0);
                TEMP::Temp *src = munchExp(stm->src);
                emit(new AS::OperInstr("movq `s0, (" + int2str(((T::ConstExp *) exp)->consti) + ") #970",
                                       nullptr, new TEMP::TempList(src, nullptr), nullptr));
            } else {
                TEMP::Temp *src = munchExp(stm->src);
                if (!sigReg(src) && !isframetemp(src)) {
                    emit(new AS::OperInstr("pushq `s0 #975", new TEMP::TempList(F::RSP(), nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                    pl += 8;
                }
                TEMP::Temp *mexp = munchExp(exp);
                if (!sigReg(src) && !isframetemp(src)) {
                    emit(new AS::OperInstr("popq `d0 #981", new TEMP::TempList(F::RBX(),
                                                                          new TEMP::TempList(F::RSP(), nullptr)),
                                           new TEMP::TempList(F::RSP(), nullptr), nullptr));
                    src = F::RBX();
                    pl -= 8;
                }
                TEMP::Temp *r = F::RBX();
                if (src == F::FP()) {
                    emit(new AS::OperInstr("leaq " + inframe(0) + ", `d0 #989", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(src, nullptr), nullptr));
                } else if (isframetemp(src)) {
                    emit(new AS::OperInstr("movq " + inframetemp(src) + ", `d0 #992", new TEMP::TempList(r, nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr), nullptr));
                } else {
                    r = src;
                }

                if (mexp == F::FP()) {
                    emit(new AS::OperInstr("movq `s0, " + inframe(0) + " #999", nullptr,
                                           new TEMP::TempList(r, new TEMP::TempList(mexp, nullptr)),
                                           nullptr));
                } else if (isframetemp(mexp)) {
                    emit(new AS::OperInstr("movq " + inframetemp(mexp) + ", `d0 #1003",
                                           new TEMP::TempList(F::RCX(), nullptr),
                                           new TEMP::TempList(F::RSP(), nullptr), nullptr));
                    emit(new AS::OperInstr("movq `s0, (%rcx) #1006", nullptr,
                                           new TEMP::TempList(r, new TEMP::TempList(F::RCX(), nullptr)),
                                           nullptr));
                } else {
                    emit(new AS::OperInstr("movq `s0, (`s1) #1010", nullptr,
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