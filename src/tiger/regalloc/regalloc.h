#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include <tiger/liveness/liveness.h>
#include "tiger/codegen/assem.h"
#include <set>
#include <map>
#include <stack>

namespace RA {
    class Move {
    public:
        enum Kind {
            COALESCED, CONSTRAINED, FROZEN, WORKLIST, ACTIVE
        };
        G::Node<TEMP::Temp> *src, *dst;

        Move(G::Node<TEMP::Temp> *src, G::Node<TEMP::Temp> *dst)
                : src(src), dst(dst), kind(WORKLIST) {}

        Move(G::Node<TEMP::Temp> *src, G::Node<TEMP::Temp> *dst, Kind kind)
                : src(src), dst(dst), kind(kind) {}

        Kind kind;
    };

    class Result {
    public:
        TEMP::Map *coloring;
        AS::InstrList *il;
    };

    Result RegAlloc(F::Frame *f, AS::InstrList *il);

    // nodes
    static std::set<G::Node<TEMP::Temp> *>
    // machine-registers
            *precolored,
    // not processed temporaries
            *initial,
    // able to be simplified, not move related
            *simplifyWorkList,
    // move related, can't be coalesced
            *freezeWorkList,
    // potential spills
            *spillWorkList,
    // actual spills
            *spilledNodes,
    // have been coalesced, alias to record
            *coalescedNodes,
    // nodes successfully colored
            *coloredNodes,
            *selectSet;
    static std::stack<G::Node<TEMP::Temp> *>
    // temporaries removed from graph
            *selectStack;


    // moves
    static std::set<Move *>
    // moves have been coalesced
            *coalescedMoves,
    // src and dst interfere
            *constrainedMoves,
    // already frozen
            *frozenMoves,
    // enabled for coalescing
            *workListMoves,
    // not yet ready for coalescing
            *activeMoves;


    static std::map<G::Node<TEMP::Temp> *, int> *degree;
    // alias for coalesce
    static std::map<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *> *alias;
    // moves associated with the temp
    static std::map<G::Node<TEMP::Temp> *,
            std::set<Move *> *> *moveList;
    static std::set<std::pair<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *>>
            *adjSet;
    static std::map<G::Node<TEMP::Temp> *, std::set<G::Node<TEMP::Temp> *> *> *adjList;
    static std::map<G::Node<TEMP::Temp> *, TEMP::Temp *> *color;
    LIVE::LiveGraph LivenessAnalysis(F::Frame *, AS::InstrList *);

    void init(G::NodeList<TEMP::Temp> *);

    void Build(LIVE::LiveGraph);

    void MakeworkList();

    void Simplify();

    void Coalesce();

    void Freeze();

    void SelectSpill();

    void AssignColors();

    void RewriteProgram(F::X64Frame *, AS::InstrList *);

    bool isRegister(TEMP::Temp *);

    bool MoveRelated(G::Node<TEMP::Temp> *);

    std::set<Move *> *NodeMoves(G::Node<TEMP::Temp> *);

    void AddEdge(G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *);

    void DecrementDegree(G::Node<TEMP::Temp> *);

    bool isAdj(G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *);

    void EnableMoves(std::set<G::Node<TEMP::Temp> *> *);
    void EnableMoves(G::Node<TEMP::Temp> *);

    std::set<G::Node<TEMP::Temp> *> *Adjacent(G::Node<TEMP::Temp> *);

    G::Node<TEMP::Temp> *GetAlias(G::Node<TEMP::Temp> *);

    void AddWorkList(G::Node<TEMP::Temp> *);
    bool OK(G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *);
    bool George(G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *);
    bool Conservative(std::set<G::Node<TEMP::Temp> *> *);
    void Combine(G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *);
    void FreezeMoves(G::Node<TEMP::Temp> *);
    G::Node<TEMP::Temp> *favouriteSpill();
    bool Include(TEMP::TempList *, TEMP::Temp *);
    void Swap(TEMP::TempList *, TEMP::Temp *, TEMP::Temp *);
    std::string getName(G::Node<TEMP::Temp> *);
    void moveReduce(AS::InstrList *, TEMP::Map *);
}  // namespace RA
#endif