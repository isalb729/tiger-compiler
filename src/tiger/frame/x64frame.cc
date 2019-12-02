#include "tiger/frame/frame.h"
#include "tiger/translate/translate.h"
#include <string>

namespace F {


    T::Exp *InFrameAccess::toExp(T::Exp *framePtr) const {
        return new T::MemExp(new T::BinopExp(T::PLUS_OP, framePtr, new T::ConstExp(offset)));
    }

    T::Exp *InRegAccess::toExp(T::Exp *framePtr) const {
        return new T::TempExp(reg);
    }

    X64Frame::X64Frame(TEMP::Label *name, U::BoolList *formalEscapes) : Frame(name) {
        uint32_t offset = 0;
        AccessList *formals_ = new AccessList(nullptr, nullptr), *tail = formals_;
        for (; formalEscapes; formalEscapes = formalEscapes->tail) {
            Access *access;
            if (formalEscapes->head) {
                offset += 4;
                access = new InFrameAccess(offset);
            } else {
                access = new InRegAccess(TEMP::Temp::NewTemp());
            }
            tail = tail->tail = new AccessList(access, nullptr);
        }
        this->formals = formals_->tail;
    }

    Access *X64Frame::allocal(bool escape) {
        if (escape) {
            // TODO: ADD LOCALS OF THE FRAME
            Access *access = new InFrameAccess(-size);
            size += 4;
            return access;
        }
        return new InRegAccess(TEMP::Temp::NewTemp());
    }

    T::Exp *ExternalCall(std::string s, T::ExpList *args) {
        // TODO: ADD "_"
        return new T::CallExp(new T::NameExp(TEMP::NamedLabel(s)), args);
    }



    AS::InstrList *F_procEntryExit2(AS::InstrList *body) {
        return nullptr;
    }

    AS::Proc *F_procEntryExit3(Frame *frame, AS::InstrList *instrList) {
        return nullptr;
    }


}  // namespace F