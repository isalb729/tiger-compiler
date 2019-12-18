#include <tiger/util/graph.h>
#include "tiger/regalloc/regalloc.h"

namespace RA {
    template<class T>
    std::set<T *> *UnionSet(std::set<T *> *set1, std::set<T *> *set2) {
        std::set<T *> *set = new std::set<T *>{};
        for (auto item : *set1) {
            set->insert(item);
        }
        for (auto item : *set2) {
            set->insert(item);
        }
        return set;
    }

    template<class T>
    std::set<T *> *IntersectSet(std::set<T *> *set1, std::set<T *> *set2) {
        std::set<T *> *set = new std::set<T *>{};
        for (auto item : *set1) {
            if (set2->find(item) != set2->end()) {
                set->insert(item);
            }
        }
        return set;
    }

    template<class T>
    std::set<T *> *MinusSet(std::set<T *> *set1, std::set<T *> *set2) {
        std::set<T *> *set = new std::set<T *>{};
        for (auto item : *set1) {
            if (!set2->count(item)) {
                set->insert(item);
            }
        }
        return set;
    }

    bool Include(TEMP::TempList *tempList, TEMP::Temp *temp) {
        while (tempList) {
            if (tempList->head == temp) {
                return true;
            }
            tempList = tempList->tail;
        }
        return false;
    }

    void moveReduce(AS::InstrList *il, TEMP::Map *coloring) {
        while (il->tail) {
            if (il->head->kind == AS::Instr::MOVE &&
                coloring->Look(((AS::MoveInstr *) il->head)->src->head) ==
                coloring->Look(((AS::MoveInstr *) il->head)->dst->head)) {
                il->head = il->tail->head;
                il->tail = il->tail->tail;
            } else {
                il = il->tail;
            }
        }
    }

    std::string getName(G::Node<TEMP::Temp> *node) {
        if (isRegister(node->NodeInfo())) {
            return *F::regmap()->Look(node->NodeInfo());
        } else {
            return std::to_string(node->NodeInfo()->Int());
        }
    }

    void Swap(TEMP::TempList *tempList, TEMP::Temp *u, TEMP::Temp *v) {
        while (tempList) {
            if (tempList->head == u) {
                tempList->head = v;
            }
            tempList = tempList->tail;
        }
    }

    //
    bool isRegister(TEMP::Temp *temp) {
        TEMP::TempList *regs = F::regs();
        while (regs) {
            if (regs->head == temp) {
                return true;
            }
            regs = regs->tail;
        }
        return false;
    }

    //
    std::set<Move *> *NodeMoves(G::Node<TEMP::Temp> *node) {
        return IntersectSet(moveList->at(node), UnionSet(activeMoves, workListMoves));
    }

    //
    bool MoveRelated(G::Node<TEMP::Temp> *node) {
        return !NodeMoves(node)->empty();
    }

    //
    bool isAdj(G::Node<TEMP::Temp> *node1, G::Node<TEMP::Temp> *node2) {
        return adjSet->count(std::pair<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *>{node1, node2})
               || adjSet->count(std::pair<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *>{node2, node1});
    }

    //
    void AddEdge(G::Node<TEMP::Temp> *node1, G::Node<TEMP::Temp> *node2) {
        if (!isAdj(node1, node2) && node1 != node2) {
            adjSet->insert(std::pair<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *>{node1, node2});
            adjSet->insert(std::pair<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *>{node2, node1});
            if (!isRegister(node1->NodeInfo())) {
                // only for temporaries
                adjList->at(node1)->insert(node2);
                degree->at(node1) = degree->at(node1) + 1;
            }
            if (!isRegister(node2->NodeInfo())) {
                adjList->at(node2)->insert(node1);
                degree->at(node2) = degree->at(node2) + 1;
            }
        }
    }

    //
    void EnableMoves(std::set<G::Node<TEMP::Temp> *> *set) {
        for (auto item : *set) {
            EnableMoves(item);
        }
    }

    //
    void EnableMoves(G::Node<TEMP::Temp> *item) {
        for (auto item1 : *NodeMoves(item)) {
            if (activeMoves->count(item1)) {
                activeMoves->erase(item1);
                workListMoves->insert(item1);
            }
        }
    }

    //
    std::set<G::Node<TEMP::Temp> *> *Adjacent(G::Node<TEMP::Temp> *node) {
        assert(node);
        return MinusSet(adjList->at(node), UnionSet(selectSet, coalescedNodes));
    }

    //
    void DecrementDegree(G::Node<TEMP::Temp> *node) {
        int d = degree->at(node);
        degree->at(node) = d - 1;
        if (d == 16) {
            // able to simplify or coalesce
            auto adj = Adjacent(node);
            adj->insert(node);
            EnableMoves(adj);
            spillWorkList->erase(node);
            if (MoveRelated(node)) {
                freezeWorkList->insert(node);
            } else {
                simplifyWorkList->insert(node);
            }
        }
    }

    //
    void AddWorkList(G::Node<TEMP::Temp> *node) {
        if (!precolored->count(node) && !MoveRelated(node) && degree->at(node) < 16) {
            // wont freeze
            freezeWorkList->erase(node);
            simplifyWorkList->insert(node);
        }
    }

    //
    bool OK(G::Node<TEMP::Temp> *node1, G::Node<TEMP::Temp> *node2) {
        return degree->at(node1) < 16 || precolored->count(node1) || isAdj(node1, node2);
    }

    //
    bool George(G::Node<TEMP::Temp> *v, G::Node<TEMP::Temp> *u) {
        for (auto it: *Adjacent(v)) {
            if (!OK(it, u)) {
                return false;
            }
        }
        return true;
    }

    //
    bool Conservative(std::set<G::Node<TEMP::Temp> *> *set) {
        int k = 0;
        // count of insignificant nodes
        for (auto item: *set) {
            if (degree->at(item) >= 16) {
                k++;
            }
        }
        return k < 16;
    }

    //
    void Combine(G::Node<TEMP::Temp> *u, G::Node<TEMP::Temp> *v) {
        // v couldn't be register
        // marry
        if (freezeWorkList->count(v)) {
            freezeWorkList->erase(v);
        } else {
            spillWorkList->erase(v);
        }
        // go away
        coalescedNodes->insert(v);
        alias->insert(std::pair<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *>{v, u});
        // share the moves
        moveList->at(u) = UnionSet(moveList->at(u), moveList->at(v));
        EnableMoves(v);
        // share the edges
        for (auto it : *Adjacent(v)) {
            // TODO: DIFFERENT
            if (!isAdj(it, u)) {
                AddEdge(it, u);
                if (!precolored->count(it)) {
                    DecrementDegree(it);
                }
            }
        }
        if (degree->at(u) >= 16 && freezeWorkList->count(u)) {
            freezeWorkList->erase(u);
            spillWorkList->insert(u);
        }
    }

    //
    // free the node, no more coalesce
    void FreezeMoves(G::Node<TEMP::Temp> *u) {
        for (auto item : *NodeMoves(u)) {
            G::Node<TEMP::Temp> *x = item->src;
            G::Node<TEMP::Temp> *y = item->dst, *v;
            if (GetAlias(y) == GetAlias(u)) {
                v = GetAlias(x);
            } else {
                v = GetAlias(y);
            }
            activeMoves->erase(item);
            frozenMoves->insert(item);
            if (NodeMoves(v)->empty() && degree->at(v) < 16) {
                freezeWorkList->erase(v);
                simplifyWorkList->insert(v);
            }
        }
    }

    // max degree
    // TODO: USES + DEFS
    G::Node<TEMP::Temp> *favouriteSpill() {
        int max = 0;
        G::Node<TEMP::Temp> *node = nullptr;
        for (auto item: *spillWorkList) {
            if (degree->at(item) > max) {
                max = degree->at(item);
                node = item;
            }
        }
        return node;
    }

    void init(G::NodeList<TEMP::Temp> *nodes) {
        degree = new std::map<G::Node<TEMP::Temp> *, int>{};
        alias = new std::map<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *>{};
        moveList = new std::map<G::Node<TEMP::Temp> *, std::set<Move *> *>{};
        adjSet = new std::set<std::pair<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *>>{};
        adjList = new std::map<G::Node<TEMP::Temp> *, std::set<G::Node<TEMP::Temp> *> *>{};
        color = new std::map<G::Node<TEMP::Temp> *, TEMP::Temp *>{};
        for (G::NodeList<TEMP::Temp> *nodeList = nodes; nodeList; nodeList = nodeList->tail) {
            degree->insert(std::pair<G::Node<TEMP::Temp> *, int>{nodeList->head, 0});
            moveList->insert(
                    std::pair<G::Node<TEMP::Temp> *, std::set<Move *> *>{nodeList->head, new std::set<Move *>{}});
            adjList->insert(std::pair<G::Node<TEMP::Temp> *, std::set<G::Node<TEMP::Temp> *> *>{nodeList->head,
                                                                                                new std::set<G::Node<TEMP::Temp> *>{}});
            if (isRegister(nodeList->head->NodeInfo())) {
                color->insert(
                        std::pair<G::Node<TEMP::Temp> *, TEMP::Temp *>{nodeList->head, nodeList->head->NodeInfo()});
            }
        }
        // nodes
        precolored = new std::set<G::Node<TEMP::Temp> *>{};
        initial = new std::set<G::Node<TEMP::Temp> *>{};
        simplifyWorkList = new std::set<G::Node<TEMP::Temp> *>{};
        freezeWorkList = new std::set<G::Node<TEMP::Temp> *>{};
        spillWorkList = new std::set<G::Node<TEMP::Temp> *>{};
        coalescedNodes = new std::set<G::Node<TEMP::Temp> *>{};
        coloredNodes = new std::set<G::Node<TEMP::Temp> *>{};
        spilledNodes = new std::set<G::Node<TEMP::Temp> *>{};
        selectSet = new std::set<G::Node<TEMP::Temp> *>{};
        selectStack = new std::stack<G::Node<TEMP::Temp> *>{};
        // moves
        workListMoves = new std::set<Move *>{};
        coalescedMoves = new std::set<Move *>{};
        constrainedMoves = new std::set<Move *>{};
        frozenMoves = new std::set<Move *>{};
        activeMoves = new std::set<Move *>{};
        workListMoves = new std::set<Move *>{};
    }


    LIVE::LiveGraph LivenessAnalysis(F::Frame *f, AS::InstrList *il) {
        G::Graph<AS::Instr> *flowgraph = FG::AssemFlowGraph(il, f);
        LIVE::LiveGraph liveGraph = LIVE::Liveness(flowgraph);
        return liveGraph;
    }

    void Build(LIVE::LiveGraph liveGraph) {
        G::NodeList<TEMP::Temp> *nodeList = liveGraph.graph->Nodes();
        while (nodeList) {
            for (auto it = nodeList->head->Adj(); it; it = it->tail) {
                AddEdge(nodeList->head, it->head);
            }
            if (!isRegister(nodeList->head->NodeInfo())) {
                // build initial
                initial->insert(nodeList->head);
                // build degree
                degree->at(nodeList->head) = nodeList->head->Degree();
            } else {
                // build precolored
                precolored->insert(nodeList->head);
                color->insert(
                        std::pair<G::Node<TEMP::Temp> *, TEMP::Temp *>{nodeList->head, nodeList->head->NodeInfo()});
            }
            nodeList = nodeList->tail;
        }
        // build moves
        LIVE::MoveList *moveList_ = liveGraph.moves;
        // default kind: worklist
        while (moveList_) {
            // TODO: warning REPETITON IN MOVELIST
            Move *src2dst = new Move(moveList_->src, moveList_->dst);
            Move *dst2src = new Move(moveList_->dst, moveList_->src);
            workListMoves->insert(src2dst);
            workListMoves->insert(dst2src);
            moveList->at(moveList_->src)->insert(src2dst);
            moveList->at(moveList_->dst)->insert(dst2src);
            moveList_ = moveList_->tail;
        }

    }

    //
    // status of nodes
    void MakeworkList() {
        // NEVER ERASE IN A FOREACH LOOP!!!!!!!
        for (auto item : *initial) {
            if (degree->at(item) >= 16) {
                spillWorkList->insert(item);
            } else if (MoveRelated(item)) {
                freezeWorkList->insert(item);
            } else {
                simplifyWorkList->insert(item);
            }
        }
        initial->clear();
    }

    //
    G::Node<TEMP::Temp> *GetAlias(G::Node<TEMP::Temp> *node) {
        if (coalescedNodes->count(node)) {
            return GetAlias(alias->at(node));
        } else {
            return node;
        }
    }

    //
    void Simplify() {
        G::Node<TEMP::Temp> *item = *simplifyWorkList->begin();
        assert(item);
        simplifyWorkList->erase(item);
        selectStack->push(item);
        selectSet->insert(item);
        for (auto it: *Adjacent(item)) {
            DecrementDegree(it);
        }
    }

    //
    void Coalesce() {
        Move *move = *workListMoves->begin();
        G::Node<TEMP::Temp> *x = GetAlias(move->src), *u;
        G::Node<TEMP::Temp> *y = GetAlias(move->dst), *v;
        if (precolored->count(y)) {
            u = y;
            v = x;
        } else {
            u = x;
            v = y;
        }
        workListMoves->erase(move);
        if (u == v) {
            // same alias
            coalescedMoves->insert(move);
            AddWorkList(u);
        } else if (precolored->count(v) || isAdj(u, v)) {
            // two registers or interfere
            constrainedMoves->insert(move);
            AddWorkList(u);
            AddWorkList(v);
            // george and briggs
        } else if ((precolored->count(u) && George(v, u)) ||
                   (!precolored->count(u) && Conservative(UnionSet(Adjacent(u), Adjacent(v))))) {
            // coalesce, throw v
            coalescedMoves->insert(move);
            Combine(u, v);
//            p("\n\ncombine: %s, %s\n\n", getName(u).c_str(), getName(v).c_str())
            AddWorkList(u);
        } else {
            activeMoves->insert(move);
        }
    }

    //
    void Freeze() {
        G::Node<TEMP::Temp> *u = *freezeWorkList->begin();
        assert(u);
        freezeWorkList->erase(u);
        simplifyWorkList->insert(u);
        FreezeMoves(u);
    }

    //
    void SelectSpill() {
        G::Node<TEMP::Temp> *m = favouriteSpill();
        spillWorkList->erase(m);
        simplifyWorkList->insert(m);
        FreezeMoves(m);
    }

    //
    void AssignColors() {
        while (!selectStack->empty()) {
            G::Node<TEMP::Temp> *node = selectStack->top();
            selectStack->pop();
            selectSet->erase(node);
            std::set<TEMP::Temp *> *okColor = F::regSet();
            // real adjacent
            for (auto item : *adjList->at(node)) {
                auto set = UnionSet(coloredNodes, precolored);
                // if already colored
                if (set->count(GetAlias(item))) {
                    okColor->erase(color->at(GetAlias(item)));
                }
            }
            if (okColor->empty()) {
                spilledNodes->insert(node);
            } else {
//                p("\ncolor: %s, %s", getName(node).c_str(), F::regmap()->Look(*okColor->begin())->c_str())
                coloredNodes->insert(node);
                color->insert(std::pair<G::Node<TEMP::Temp> *, TEMP::Temp *>{node, *okColor->begin()});
            }
        }
        assert(selectStack->empty());
        assert(selectSet->empty());
        // color the coalesced nodes
        for (auto it: *coalescedNodes) {
            color->insert(std::pair<G::Node<TEMP::Temp> *, TEMP::Temp *>{it, color->at(GetAlias(it))});
        }
    }

    void RewriteProgram(F::X64Frame *frame, AS::InstrList *il) {
        // TODO: CHECK
//        p("\n\nREWRITE!!!!!!!!\n\n")
        assert(!spilledNodes->empty());
        for (auto it : *spilledNodes) {
//            p("\nspill: %s\n", getName(it).c_str())
            frame->allocal(true);
            AS::InstrList *il_ = il;
            while (il_) {
                TEMP::TempList *uses = nullptr, *defs = nullptr;
                if (il_->head->kind == AS::Instr::OPER) {
                    uses = ((AS::OperInstr *) il_->head)->src;
                    defs = ((AS::OperInstr *) il_->head)->dst;
                } else if (il_->head->kind == AS::Instr::MOVE) {
                    uses = ((AS::MoveInstr *) il_->head)->src;
                    defs = ((AS::MoveInstr *) il_->head)->dst;
                }
                AS::Instr *before = nullptr, *after = nullptr;
                if (Include(uses, it->NodeInfo())) {
                    TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
                    Swap(uses, it->NodeInfo(), newTemp);
                    // TODO: WHAT IF IT HAPPENS WHEN PUSHING PARAS
                    before = new AS::OperInstr(
                            "movq (" + TEMP::LabelString(frame->label) + "_framesize - " + std::to_string(frame->size) +
                            ")(%rsp), `d0 # regalloc 401",
                            new TEMP::TempList(newTemp, nullptr), new TEMP::TempList(F::RSP(), nullptr),
                            nullptr);
                }
                if (Include(defs, it->NodeInfo())) {
                    TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
                    Swap(defs, it->NodeInfo(), newTemp);
                    after = new AS::OperInstr("movq `s0, (" + TEMP::LabelString(frame->label) + "_framesize - " +
                                              std::to_string(frame->size) +
                                              ")(%rsp) # regalloc 410", nullptr,
                                              new TEMP::TempList(newTemp, new TEMP::TempList(F::RSP(),
                                                                                             nullptr)),
                                              nullptr);
                }
                if (after) {
                    il_->tail = new AS::InstrList(after, il_->tail);
                }
                if (before) {
                    il_->tail = new AS::InstrList(il_->head, il_->tail);
                    il_->head = before;
                }
                il_ = il_->tail;
            }
        }
    }

    Result RegAlloc(F::Frame *f, AS::InstrList *il) {
//         instruction
//        il->Print(stdout, TEMP::Map::LayerMap(F::regmap(), TEMP::Map::Name()));
//        fflush(stdout);
        LIVE::LiveGraph livegraph = LivenessAnalysis(f, il);
        init(livegraph.graph->Nodes());
        Build(livegraph);
        MakeworkList();
        // liveness
//        for (auto i: *adjList) {
//            if (isRegister(i.first->NodeInfo())) { continue; }
//            p("%s: ", getName(i.first).c_str());
//            for (auto j: *i.second) {
//                p("%s, ", getName(j).c_str())
//            }
//            p("\n")
//        }
        while (!simplifyWorkList->empty() || !workListMoves->empty() ||
               !freezeWorkList->empty() || !spillWorkList->empty()) {
            if (!simplifyWorkList->empty()) {
                Simplify();
            } else if (!workListMoves->empty()) {
                Coalesce();
            } else if (!freezeWorkList->empty()) {
                Freeze();
            } else if (!spillWorkList->empty()) {
                SelectSpill();
            }
        }
        AssignColors();
        if (!spilledNodes->empty()) {
            RewriteProgram((F::X64Frame *) f, il);
            return RegAlloc(f, il);
        }
        Result result = Result();
        G::NodeList<TEMP::Temp> *nodes = livegraph.graph->Nodes();
        TEMP::Map *coloring = TEMP::Map::Empty();
        TEMP::Map *regMap = F::regmap();
        while (nodes) {
            coloring->Enter(nodes->head->NodeInfo(), regMap->Look(color->at(nodes->head)));
            nodes = nodes->tail;
        }
        // TODO : FURTHER REMOVE NONLIVE
        moveReduce(il, coloring);
        result.coloring = coloring;
        result.il = il;
        return result;
    }

}  // namespace RA