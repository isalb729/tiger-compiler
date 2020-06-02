#include "tiger/frame/frame.h"
#include <string>

namespace F {


    TEMP::Temp *nthargs(uint n) {
        switch (n) {
            case 0:
                return RDI();
            case 1:
                return RSI();
            case 2:
                return RDX();
            case 3:
                return RCX();
            case 4:
                return R8();
            case 5:
                return R9();
            default:
                assert(0);
        }
    }

    TEMP::TempList *argRegs() {
        return new TEMP::TempList(RDI(), new TEMP::TempList(RSI(), new TEMP::TempList(RDX(), new TEMP::TempList(RCX(),
                                                                                                                new TEMP::TempList(
                                                                                                                        R8(),
                                                                                                                        new TEMP::TempList(
                                                                                                                                R9(),
                                                                                                                                nullptr))))));
    }

    TEMP::Temp *RSP() {
        static TEMP::Temp *rsp = TEMP::Temp::NewTemp();
        return rsp;
    }

    TEMP::Temp *RAX() {
        static TEMP::Temp *rax = TEMP::Temp::NewTemp();
        return rax;
    }

    TEMP::Temp *RDX() {
        static TEMP::Temp *rdx = TEMP::Temp::NewTemp();
        return rdx;
    }

    TEMP::Temp *FP() {
        return RSP();
    }

    TEMP::Temp *RV() {
        return RAX();
    }


    TEMP::Temp *RDI() {
        static TEMP::Temp *rdi = TEMP::Temp::NewTemp();
        return rdi;
    }

    TEMP::Temp *RBP() {
        static TEMP::Temp *rbp = TEMP::Temp::NewTemp();
        return rbp;
    }

    TEMP::Temp *RBX() {
        static TEMP::Temp *rbx = TEMP::Temp::NewTemp();
        return rbx;
    }

    TEMP::Temp *RSI() {
        static TEMP::Temp *rsi = TEMP::Temp::NewTemp();
        return rsi;
    }

    TEMP::Temp *RCX() {
        static TEMP::Temp *rcx = TEMP::Temp::NewTemp();
        return rcx;
    }

    TEMP::Temp *R8() {
        static TEMP::Temp *r8 = TEMP::Temp::NewTemp();
        return r8;
    }

    TEMP::Temp *R9() {
        static TEMP::Temp *r9 = TEMP::Temp::NewTemp();
        return r9;
    }

    TEMP::Temp *R10() {
        static TEMP::Temp *r10 = TEMP::Temp::NewTemp();
        return r10;
    }

    TEMP::Temp *R11() {
        static TEMP::Temp *r11 = TEMP::Temp::NewTemp();
        return r11;
    }

    TEMP::Temp *R12() {
        static TEMP::Temp *r12 = TEMP::Temp::NewTemp();
        return r12;
    }

    TEMP::Temp *R13() {
        static TEMP::Temp *r13 = TEMP::Temp::NewTemp();
        return r13;
    }

    TEMP::Temp *R14() {
        static TEMP::Temp *r14 = TEMP::Temp::NewTemp();
        return r14;
    }

    TEMP::Temp *R15() {
        static TEMP::Temp *r15 = TEMP::Temp::NewTemp();
        return r15;
    }

    // %rbx, %rbp, %r12, %r13, %r14, %r15
    TEMP::TempList *calleeSaved() {
        return new TEMP::TempList(RBX(), new TEMP::TempList(RBP(),
                                                            new TEMP::TempList(R12(), new TEMP::TempList(R13(),
                                                                                                         new TEMP::TempList(
                                                                                                                 R14(),
                                                                                                                 new TEMP::TempList(
                                                                                                                         R15(),
                                                                                                                         nullptr))))));
    }

    // %r10, %r11
    // TODO: CALLER SAVED INTERFERENCE LIVE
    TEMP::TempList *callerSaved() {
        return new TEMP::TempList(R10(), new TEMP::TempList(R11(), nullptr));
    }

    TEMP::TempList *regs() {
        return new TEMP::TempList(RBX(), new TEMP::TempList(RBP(),
                                                            new TEMP::TempList(RSP(), new TEMP::TempList(R13(),
                                                                                                         new TEMP::TempList(
                                                                                                                 R14(),
                                                                                                                 new TEMP::TempList(
                                                                                                                         R15(),
                                                                                                                         new TEMP::TempList(
                                                                                                                                 R8(),
                                                                                                                                 new TEMP::TempList(
                                                                                                                                         R9(),
                                                                                                                                         new TEMP::TempList(
                                                                                                                                                 R10(),
                                                                                                                                                 new TEMP::TempList(
                                                                                                                                                         R11(),
                                                                                                                                                         new TEMP::TempList(
                                                                                                                                                                 R12(),
                                                                                                                                                                 new TEMP::TempList(
                                                                                                                                                                         RDI(),
                                                                                                                                                                         new TEMP::TempList(
                                                                                                                                                                                 RDX(),
                                                                                                                                                                                 new TEMP::TempList(
                                                                                                                                                                                         RSI(),
                                                                                                                                                                                         new TEMP::TempList(
                                                                                                                                                                                                 RCX(),
                                                                                                                                                                                                 new TEMP::TempList(
                                                                                                                                                                                                         RAX(),
                                                                                                                                                                                                         nullptr))))))))))))))));
    }

    std::set<TEMP::Temp *> *regSet() {
        std::set<TEMP::Temp *> *set = new std::set<TEMP::Temp *>{};
        TEMP::TempList *regList = regs();
        while (regList) {
            set->insert(regList->head);
            regList = regList->tail;
        }
        return set;
    }

    T::Exp *InFrameAccess::toExp(T::Exp *framePtr) const {
        return new T::MemExp(new T::BinopExp(T::PLUS_OP, framePtr, new T::ConstExp(offset)));
    }

    T::Exp *InRegAccess::toExp(T::Exp *framePtr) const {
        return new T::TempExp(reg);
    }

    // formals include static link
    X64Frame::X64Frame(TEMP::Label *name, U::BoolList *formalEscapes) : Frame(name) {
        uint32_t offset = 8;
        // arg2..arg1..staticlink..return address
        if (formalEscapes) {
            AccessList *formals_ = new AccessList(nullptr, nullptr), *tail = formals_;
            Access *access = new InFrameAccess(offset);
            offset += 8;
            tail = tail->tail = new AccessList(access, nullptr);
            formalEscapes = formalEscapes->tail;
            for (int i = 0; formalEscapes; formalEscapes = formalEscapes->tail) {
                // TODO: FUNDEC, MUNCHARGS
//                assert(!formalEscapes->head);
                // move nth args to new temp
                if (!formalEscapes->head) {
                    assert(i < 6);
                    access = new InRegAccess(TEMP::Temp::NewTemp());
//                    access = new InRegAccess(nthargs(i));
                    i++;
                } else {
                    access = allocal(true);
//                    access = new InFrameAccess(offset);
//                    offset += 8;
                }
                tail = tail->tail = new AccessList(access, nullptr);
            }
            this->formals = formals_->tail;
        }
    }

    Access *X64Frame::allocal(bool escape) {
        if (escape) {
            size += 8;
            Access *access = new InFrameAccess(-size);
            F::AccessList *locals_ = this->locals;
            while (locals_) {
                locals_ = locals_->tail;
            }
            locals_ = new AccessList(access, nullptr);
            return access;
        } else {
            return new InRegAccess(TEMP::Temp::NewTemp());
        }
    }

    T::Exp *ExternalCall(std::string s, T::ExpList *args) {
        return new T::CallExp(new T::NameExp(TEMP::NamedLabel(s)), args);
    }

    AS::InstrList *F_procEntryExit2(AS::InstrList *body) {
        static TEMP::TempList *returnSink = new TEMP::TempList(F::RSP(), new TEMP::TempList(F::RAX(), nullptr));
        static TEMP::TempList *beginSrc = new TEMP::TempList(F::RSP(), nullptr);
        AS::Instr *instr = new AS::OperInstr("", nullptr, returnSink, nullptr);
        AS::InstrList *r = body;
        while (body->tail) {
            body = body->tail;
        }
        body->tail = new AS::InstrList(instr, nullptr);
        instr = new AS::OperInstr("", beginSrc, beginSrc, nullptr);
        r = new AS::InstrList(instr, r);
        return r;
    }

    template<typename t>
    static uint listLen(t *i) {
        uint count = 0;
        while (i) {
            i = i->tail;
            count++;
        }
        return count;
    }

    AS::Proc *F_procEntryExit3(Frame *frame, AS::InstrList *instrList) {
        std::string prolog, epilog, fs;
        prolog = frame->label->Name() + ":\n";
        fs = TEMP::LabelString(frame->label) + "_framesize";
        prolog = prolog + ".set " + fs + ", " + std::to_string(frame->size)
                 + "\nsubq $" + std::to_string(frame->size) + ", %rsp\n";
        epilog = "addq $" + std::to_string(frame->size) + ", %rsp\nretq\n\n";
        return new AS::Proc(prolog, instrList, epilog);
    }

    TEMP::Map *init_temp_map(TEMP::Map *map) {
        map->Enter(F::RSP(), new std::string("%rsp"));
        map->Enter(F::RAX(), new std::string("%rax"));
        map->Enter(F::RDX(), new std::string("%rdx"));
        map->Enter(F::RDI(), new std::string("%rdi"));
        map->Enter(F::RBP(), new std::string("%rbp"));
        map->Enter(F::RBX(), new std::string("%rbx"));
        map->Enter(F::RSI(), new std::string("%rsi"));
        map->Enter(F::RCX(), new std::string("%rcx"));
        map->Enter(F::R8(), new std::string("%r8"));
        map->Enter(F::R9(), new std::string("%r9"));
        map->Enter(F::R10(), new std::string("%r10"));
        map->Enter(F::R11(), new std::string("%r11"));
        map->Enter(F::R12(), new std::string("%r12"));
        map->Enter(F::R13(), new std::string("%r13"));
        map->Enter(F::R14(), new std::string("%r14"));
        map->Enter(F::R15(), new std::string("%r15"));
        return map;
    }

    TEMP::Map *regmap() {
        static TEMP::Map *map = init_temp_map(TEMP::Map::Empty());
        return map;
    }
}  // namespace F
