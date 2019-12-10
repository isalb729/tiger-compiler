#include "tiger/regalloc/regalloc.h"

namespace RA {
    Result RegAlloc(F::Frame *f, AS::InstrList *il) {
        // TODO: Put your codes here (lab6).
        // TODO: MOVE THIS TO OTHER PLACE
        Result result = Result();
        TEMP::Map *map = TEMP::Map::Empty();
        int i = 8;
        std::string a[16] = {"%rax", "%rsp", "%rdi", "%rbp", "%rsi", "%rdx", "%rcx", "%rbx", "%r8", "%r9", "%r10",
                             "%r11", "%r12", "%r13", "%r14", "%r15"};
        for (TEMP::TempList *tempList_ = TEMP::tempList(nullptr); tempList_; tempList_ = tempList_->tail) {
            map->Enter(tempList_->head, new std::string(a[i]));
            i++;
            if (i > 15) {
                i = 8;
            }
        }
        result.coloring = TEMP::Map::LayerMap(F::regmap(), TEMP::Map::LayerMap(map, TEMP::Map::Name()));
        result.il = il;
        return result;
    }

}  // namespace RA