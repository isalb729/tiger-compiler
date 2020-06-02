#include "tiger/liveness/flowgraph.h"

namespace FG {

    //
    TEMP::TempList *Def(G::Node<AS::Instr> *n) {
        AS::Instr *instr = n->NodeInfo();
        TEMP::TempList *defs = nullptr;
        if (instr->kind == AS::Instr::OPER) {
            defs = ((AS::OperInstr *) instr)->dst;
        } else if (instr->kind == AS::Instr::MOVE) {
            defs = ((AS::MoveInstr *) instr)->dst;
        }
        return defs;
    }

    //
    TEMP::TempList *Use(G::Node<AS::Instr> *n) {
        AS::Instr *instr = n->NodeInfo();
        TEMP::TempList *uses = nullptr;
        if (instr->kind == AS::Instr::OPER) {
            uses = ((AS::OperInstr *) instr)->src;
        } else if (instr->kind == AS::Instr::MOVE) {
            uses = ((AS::MoveInstr *) instr)->src;
        }
        return uses;
    }

    //
    bool IsMove(G::Node<AS::Instr> *n) {
        return n->NodeInfo()->kind == AS::Instr::MOVE;
    }

    //
    G::Graph<AS::Instr> *AssemFlowGraph(AS::InstrList *il, F::Frame *f) {
        G::Graph<AS::Instr> *graph = new G::Graph<AS::Instr>();
        AS::InstrList *instrList = il;
        G::Node<AS::Instr> *node = nullptr, *prev;
        G::NodeList<AS::Instr> *jumpInstrList = nullptr, *tail = nullptr;
        TAB::Table<TEMP::Label, G::Node<AS::Instr>>
                *labelInstr = new TAB::Table<TEMP::Label, G::Node<AS::Instr>>();
        while (instrList) {
            prev = node;
            node = graph->NewNode(instrList->head);
            if (node->NodeInfo()->kind == AS::Instr::OPER &&
                ((AS::OperInstr *) node->NodeInfo())->jumps) {
                // add jump
                if (!jumpInstrList) {
                    jumpInstrList = new G::NodeList<AS::Instr>(node, nullptr);
                    tail = jumpInstrList;
                } else {
                    tail = tail->tail = new G::NodeList<AS::Instr>(node, nullptr);
                }
            }
            if (node->NodeInfo()->kind == AS::Instr::LABEL) {
                // add label
                labelInstr->Enter(((AS::LabelInstr *) node->NodeInfo())->label, node);
            }
            // add edge if prev not jmp
            if (prev && !(prev->NodeInfo()->kind == AS::Instr::OPER &&
                          ((AS::OperInstr *) prev->NodeInfo())->jumps)) {
                G::Graph<AS::Instr>::AddEdge(prev, node);
            }
            instrList = instrList->tail;
        }
        // add edge between jmp and label
        while (jumpInstrList) {
            TEMP::LabelList *labelList =
                    ((AS::OperInstr *) jumpInstrList->head->NodeInfo())->jumps->labels;
            while (labelList) {
                node = labelInstr->Look(labelList->head);
                assert(node);
                G::Graph<AS::Instr>::AddEdge(jumpInstrList->head, node);
                labelList = labelList->tail;
            }
            jumpInstrList = jumpInstrList->tail;
        }
        return graph;
    }
}  // namespace FG
