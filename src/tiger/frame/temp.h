#ifndef TIGER_FRAME_TEMP_H_
#define TIGER_FRAME_TEMP_H_

#include "tiger/symbol/symbol.h"
namespace TEMP {
    using Label = S::Symbol;

    Label *NewLabel();

    Label *NamedLabel(std::string name);

    const std::string &LabelString(Label *s);

    class Temp {
    public:
        static Temp *NewTemp();

        int Int();

    private:
        int num;

        Temp(int num) : num(num) {}
    };


    class Map {
    public:
        void Enter(Temp *t, std::string *s);

        std::string *Look(Temp *t);

        void DumpMap(FILE *out);

        static Map *Empty();

        static Map *Name();

        static Map *LayerMap(Map *over, Map *under);

    private:
        TAB::Table<Temp, std::string> *tab;
        Map *under;

        Map() : tab(new TAB::Table<Temp, std::string>()), under(nullptr) {}

        Map(TAB::Table<Temp, std::string> *tab, Map *under)
                : tab(tab), under(under) {}
    };

    class TempList {
    public:
        Temp *head;
        TempList *tail;

        TempList(Temp *h, TempList *t) : head(h), tail(t) {}
        void Print();
    };

    class LabelList {
    public:
        Label *head;
        LabelList *tail;

        LabelList(Label *h, LabelList *t) : head(h), tail(t) {}
    };

    TEMP::TempList *tempList(TEMP::Temp *);
    TEMP::TempList *rmdu(TEMP::TempList *);
    TEMP::TempList *unionList(TEMP::TempList *, TEMP::TempList *);
    TEMP::TempList *intersectList(TEMP::TempList *, TEMP::TempList *);
    TEMP::TempList *minusList(TEMP::TempList *, TEMP::TempList *);
    bool equalList(TEMP::TempList *, TEMP::TempList *);
    template<typename t>
    static uint listLen(t *i) {
        uint count = 0;
        while (i) {
            i = i->tail;
            count++;
        }
        return count;
    }
}  // namespace TEMP

#endif