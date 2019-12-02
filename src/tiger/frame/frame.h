#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <string>

#include "tiger/codegen/assem.h"
#include "tiger/translate/tree.h"
#include "tiger/util/util.h"

namespace AS {
    class Proc;

    class InstrList;
}
namespace TR {
    class Exp;
}
namespace F {
    class Access;

    class AccessList;

    static TEMP::Temp *FP = TEMP::Temp::NewTemp(), *SP = TEMP::Temp::NewTemp(), *ZERO = TEMP::Temp::NewTemp(), *RV = TEMP::Temp::NewTemp(), *RA = TEMP::Temp::NewTemp();
    static TEMP::Temp *eax = TEMP::Temp::NewTemp(), *edx = TEMP::Temp::NewTemp();

    class Frame {
        // Base class
    public:
        TEMP::Label *label;
        AccessList *formals;
        AccessList *locals;
        uint32_t size;

        Frame(TEMP::Label *name) : label(name) {
        }
    };


    class X64Frame : public Frame {
    public:
        X64Frame(TEMP::Label *, U::BoolList *);

        Access *allocal(bool);
//        TEMP::TempList *tempList;
    };

    class Access {
    public:
        enum Kind {
            INFRAME, INREG
        };

        Kind kind;

        Access(Kind kind) : kind(kind) {}

        virtual T::Exp *toExp(T::Exp *framePtr) const = 0;
    };


    class AccessList {
    public:
        Access *head;
        AccessList *tail;

        AccessList(Access *head, AccessList *tail) : head(head), tail(tail) {}
    };

    class InFrameAccess : public Access {
    public:
        int offset;

        InFrameAccess(int offset) : Access(INFRAME), offset(offset) {}

        T::Exp *toExp(T::Exp *) const override;
    };

    class InRegAccess : public Access {
    public:
        TEMP::Temp *reg;

        InRegAccess(TEMP::Temp *reg) : Access(INREG), reg(reg) {}

        T::Exp *toExp(T::Exp *) const override;
    };

/*
 * Fragments
 */

    class Frag {
    public:
        enum Kind {
            STRING, PROC
        };

        Kind kind;

        Frag(Kind kind) : kind(kind) {}
    };

    class StringFrag : public Frag {
    public:
        TEMP::Label *label;
        std::string str;

        StringFrag(TEMP::Label *label, std::string str)
                : Frag(STRING), label(label), str(str) {}
    };

    class ProcFrag : public Frag {
    public:
        T::Stm *body;
        Frame *frame;

        ProcFrag(T::Stm *body, Frame *frame) : Frag(PROC), body(body), frame(frame) {}
    };

    class FragList {
    public:
        Frag *head;
        FragList *tail;

        FragList(Frag *head, FragList *tail) : head(head), tail(tail) {

        }
    };

    T::Exp *ExternalCall(std::string, T::ExpList *);

    AS::InstrList *F_procEntryExit2(AS::InstrList *body);

    AS::Proc *F_procEntryExit3(Frame *, AS::InstrList *body);

}  // namespace F

#endif