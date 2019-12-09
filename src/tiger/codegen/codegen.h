#ifndef TIGER_CODEGEN_CODEGEN_H_
#define TIGER_CODEGEN_CODEGEN_H_

#include "tiger/codegen/assem.h"
#include "tiger/translate/tree.h"

namespace CG {
    static AS::InstrList *instrList, *tail;
    AS::InstrList *Codegen(F::Frame *f, T::StmList *stmList);
    void emit(AS::Instr *);
    TEMP::Temp *munchExp(T::Exp *);
    TEMP::Temp *munchConstExp(T::ConstExp *);
    TEMP::Temp *munchCallExp(T::CallExp *);
    TEMP::Temp *munchNameExp(T::NameExp *);
    TEMP::Temp *munchTempExp(T::TempExp *);
    TEMP::Temp *munchMemExp(T::MemExp *);
    TEMP::Temp *munchBinopExp(T::BinopExp *);
    void munchStm(T::Stm *);
    void munchSeqStm(T::SeqStm *);
    void munchLabelStm(T::LabelStm *);
    void munchJumpStm(T::JumpStm *);
    void munchCJumpStm(T::CjumpStm *);
    void munchMoveStm(T::MoveStm *);
    void munchExpStm(T::ExpStm *);
    TEMP::TempList *munchArgs(int pos, T::ExpList *args, std::string name);
    std::string fs();

}
#endif