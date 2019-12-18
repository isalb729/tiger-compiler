#ifndef TIGER_LIVENESS_LIVENESS_H_
#define TIGER_LIVENESS_LIVENESS_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/util/graph.h"

namespace LIVE {
#define p(s, ...) do { \
  printf(s, ##__VA_ARGS__); \
  fflush(stdout); \
  } while (0);
    static TEMP::TempList *temporaries;
    class MoveList {
    public:
        G::Node<TEMP::Temp> *src, *dst;
        MoveList *tail;

        MoveList(G::Node<TEMP::Temp> *src, G::Node<TEMP::Temp> *dst, MoveList *tail)
                : src(src), dst(dst), tail(tail) {}
    };

    class LiveGraph {
    public:
        G::Graph<TEMP::Temp> *graph;
        MoveList *moves;
    };

    LiveGraph Liveness(G::Graph<AS::Instr> *flowgraph);

    class liveInfo {
    public:
        TEMP::TempList *livein, *liveout;
        TEMP::TempList *defs, *uses;
        bool isMove;
        liveInfo(TEMP::TempList *defs, TEMP::TempList *uses, bool isMove) : defs(defs), uses(uses), isMove(isMove) {
            livein = nullptr;
            liveout = nullptr;
        };
    };

    G::Table<AS::Instr, liveInfo> *makeLiveTable(G::Graph<AS::Instr> *);
    void initRegs(G::Graph<TEMP::Temp> *);
    void initTemporaries(G::Graph<TEMP::Temp> *, G::Graph<AS::Instr> *);
}  // namespace LIVE

#endif