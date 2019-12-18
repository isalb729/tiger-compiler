#include "tiger/liveness/liveness.h"

namespace LIVE {
    template<class T>
    G::Node<T> *findNode(G::Graph<T> *graph, T *info) {
        G::NodeList<T> *nodeList = graph->Nodes();
        while (nodeList) {
            if (nodeList->head->NodeInfo() == info) {
                return nodeList->head;
            }
            nodeList = nodeList->tail;
        }
        return nullptr;
    }
    // add temps
    void initTemporaries(G::Graph<TEMP::Temp> *graph, G::Graph<AS::Instr> *flowgraph) {
        G::NodeList<AS::Instr> *nodeList = flowgraph->Nodes();
        while (nodeList) {
            temporaries = TEMP::unionList(temporaries, FG::Def(nodeList->head));
            temporaries = TEMP::unionList(temporaries, FG::Use(nodeList->head));
            nodeList = nodeList->tail;
        }
        temporaries = TEMP::minusList(temporaries, F::regs());
        TEMP::TempList *tempList = temporaries;
        while (tempList) {
            graph->NewNode(tempList->head);
            tempList = tempList->tail;
        }
    }
    // all regs interfere
    void initRegs(G::Graph<TEMP::Temp> *graph) {
        TEMP::TempList *regs = F::regs();
        while (regs) {
            G::Node<TEMP::Temp> *node = graph->NewNode(regs->head);
            G::NodeList<TEMP::Temp> *nodeList = graph->Nodes();
            while (nodeList) {
                if (nodeList->head != node) {
                    G::Graph<TEMP::Temp>::AddEdge(nodeList->head, node);
                }
                nodeList = nodeList->tail;
            }
            regs = regs->tail;
        }
    }

    G::Table<AS::Instr, liveInfo> *makeLiveTable(G::Graph<AS::Instr> *flowgraph) {
        G::NodeList<AS::Instr> *nodeList = flowgraph->Nodes();
        G::Table<AS::Instr, liveInfo> *table = new G::Table<AS::Instr, liveInfo>();
        while (nodeList) {
            table->Enter(nodeList->head, new liveInfo(FG::Def(nodeList->head),
                                                      FG::Use(nodeList->head),
                                                      FG::IsMove(nodeList->head)));
            nodeList = nodeList->tail;
        }
        while (true) {
            bool stop = true;
            nodeList = flowgraph->Nodes();
            while (nodeList) {
                // P221
                liveInfo *li = table->Look(nodeList->head);
                // in = use + (out - def)
                TEMP::TempList *in = TEMP::unionList(li->uses, TEMP::minusList(li->liveout, li->defs));

//                nodeList->head->NodeInfo()->Print(stdout, TEMP::Map::Name());
//                 printf("li.uses: ");
//                if (li->uses) li->uses->Print();printf("\n");
//                printf("li.defs: ");
//                if (li->defs) li->defs->Print();printf("\n");
//                printf("li.livein: ");
//                if (li->livein) li->livein->Print();printf("\n");
//                printf("li.liveout: ");
//                if (li->liveout) li->liveout->Print();printf("\n");
//                printf("in: ");
//                if (in) in->Print();printf("\n");

                TEMP::TempList *out = nullptr;
                G::NodeList<AS::Instr> *outnode = nodeList->head->Succ();
                // out = + succ.in
                while (outnode) {
                    out = TEMP::unionList(table->Look(outnode->head)->livein, out);
                    outnode = outnode->tail;
                }
//                printf("out: ");
//                if (out) out->Print();printf("done\n\n");
//                fflush(stdout);
                if (!TEMP::equalList(in, li->livein) || !TEMP::equalList(out, li->liveout)) {
                    stop = false;
                }
                li->livein = in;
                li->liveout = out;
                nodeList = nodeList->tail;
            }
            if (stop) {
                break;
            }
        }
        return table;
    }

    LiveGraph Liveness(G::Graph<AS::Instr> *flowgraph) {
        MoveList *tail = nullptr;
        G::Table<AS::Instr, liveInfo> *liveTab = makeLiveTable(flowgraph);
        G::NodeList<AS::Instr> *nodeList = flowgraph->Nodes();
        LiveGraph liveGraph;
        liveGraph.graph = new G::Graph<TEMP::Temp>();
        liveGraph.moves = nullptr;
        initRegs(liveGraph.graph);
        initTemporaries(liveGraph.graph, flowgraph);
        // interference
        while (nodeList) {
            liveInfo *li = liveTab->Look(nodeList->head);
            TEMP::TempList *liveout = li->liveout;
            TEMP::TempList *defs = li->defs;
            if (li->isMove) {
                assert(TEMP::listLen(li->defs) == 1);
                assert(TEMP::listLen(li->uses) == 1);
                if (!liveGraph.moves) {
                    liveGraph.moves = new MoveList(findNode(liveGraph.graph,
                                                            li->defs->head),
                                                   findNode(liveGraph.graph, li->uses->head),
                                                   nullptr);
                    tail = liveGraph.moves;
                } else {
                    tail = tail->tail = new MoveList(findNode(liveGraph.graph,
                                                              li->defs->head),
                                                     findNode(liveGraph.graph, li->uses->head),
                                                     nullptr);
                }
                while (liveout) {
                    assert(liveout->head);
                    if (liveout->head != defs->head && liveout->head != li->uses->head) {
                        G::Graph<TEMP::Temp>::AddEdge(findNode(liveGraph.graph, liveout->head),
                                                      findNode(liveGraph.graph, defs->head));
                    }
                    liveout = liveout->tail;
                }
            } else {
                while (defs) {
                    liveout = li->liveout;
                    while (liveout) {
                        if (liveout->head != defs->head) {
                            G::Graph<TEMP::Temp>::AddEdge(findNode(liveGraph.graph, liveout->head),
                                                          findNode(liveGraph.graph, defs->head));
                        }
                        liveout = liveout->tail;
                    }
                    defs = defs->tail;
                }
            }
            nodeList = nodeList->tail;
        }
        return liveGraph;
    }

}  // namespace LIVE