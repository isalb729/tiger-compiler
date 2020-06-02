#ifndef TIGER_ESCAPE_ESCAPE_H_
#define TIGER_ESCAPE_ESCAPE_H_

#include "tiger/absyn/absyn.h"
namespace A {
    class Exp;
    class Var;
    class Dec;
}  // namespace A
namespace ESC {
    class EscapeEntry {
    public:
        int depth;
        bool *escape;

        EscapeEntry(int depth, bool *escape) : depth(depth), escape(escape) {}
    };

    void FindEscape(A::Exp *exp, int d);
    void FindEscape(A::Exp *exp);


    void VarEscape(A::Var *, int d);

    void DecEscape(A::Dec *, int d);

    static S::Table<EscapeEntry> *venv;
}  // namespace ESC

#endif