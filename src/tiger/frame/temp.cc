#include "tiger/frame/temp.h"

#include <cstdio>
#include <sstream>

namespace {

    int labels = 0;
    static int temps = 100;
    FILE *outfile;

    void showit(TEMP::Temp *t, std::string *r) {
        fprintf(outfile, "t%d -> %s\n", t->Int(), r->c_str());
    }

}  // namespace

namespace TEMP {
    TEMP::TempList *tempList(TEMP::Temp *temp) {
        static TEMP::TempList *tempList, *tempList_tail;
        if (!temp) {
            return tempList;
        } else {
            if (!tempList) {
                tempList = new TEMP::TempList(temp, nullptr);
                tempList_tail = tempList;
            } else {
                tempList_tail = tempList_tail->tail = new TEMP::TempList(temp, nullptr);
            }
        }
        return tempList;
    }

    Label *NewLabel() {
        char buf[100];
        sprintf(buf, "L%d", labels++);
        return NamedLabel(std::string(buf));
    }

/* The label will be created only if it is not found. */
    Label *NamedLabel(std::string s) { return S::Symbol::UniqueSymbol(s); }

    const std::string &LabelString(Label *s) { return s->Name(); }

    Temp *Temp::NewTemp() {
        Temp *p = new Temp(temps++);
        std::stringstream stream;
        stream << p->num;
        Map::Name()->Enter(p, new std::string(stream.str()));
        tempList(p);
        return p;
    }

    int Temp::Int() { return this->num; }

    Map *Map::Empty() { return new Map(); }

    Map *Map::Name() {
        static Map *m = nullptr;
        if (!m) m = Empty();
        return m;
    }

    Map *Map::LayerMap(Map *over, Map *under) {
        if (over == nullptr)
            return under;
        else
            return new Map(over->tab, LayerMap(over->under, under));
    }

    void Map::Enter(Temp *t, std::string *s) {
        assert(this->tab);
        this->tab->Enter(t, s);
    }

    std::string *Map::Look(Temp *t) {
        std::string *s;
        assert(this->tab);
        s = this->tab->Look(t);
        if (s)
            return s;
        else if (this->under)
            return this->under->Look(t);
        else
            return nullptr;
    }

    void Map::DumpMap(FILE *out) {
        outfile = out;
        this->tab->Dump((void (*)(Temp *, std::string *)) showit);
        if (this->under) {
            fprintf(out, "---------\n");
            this->under->DumpMap(out);
        }
    }



}  // namespace TEMP