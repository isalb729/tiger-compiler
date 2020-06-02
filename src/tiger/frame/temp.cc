#include "tiger/frame/temp.h"

#include <cstdio>
#include <sstream>
#include <set>

namespace {

    int labels = 0;
    static int temps = 100;
    FILE *outfile;

    void showit(TEMP::Temp *t, std::string *r) {
        fprintf(outfile, "t%d -> %s\n", t->Int(), r->c_str());
    }

}  // namespace

namespace TEMP {
    TEMP::TempList *rmdu(TEMP::TempList *tempList) {
        std::set<TEMP::Temp *> *set = new std::set<TEMP::Temp *>{};
        while (tempList && tempList->head) {
            set->insert(tempList->head);
            tempList = tempList->tail;
        }
        TEMP::TempList *r = nullptr, *tail = nullptr;
        for (auto it : *set) {
            if (!r) {
                r = new TEMP::TempList(it, nullptr);
                tail = r;
            } else {
                tail = tail->tail = new TEMP::TempList(it, nullptr);
            }
        }
        if (!r || !r->head) {
            return nullptr;
        } else {
            return r;
        }
    }

    TEMP::TempList *unionList(TEMP::TempList *tl1, TEMP::TempList *tl2) {
        if (!tl1 || !tl1->head) {
            if (!tl2 || !tl2->head) {
                return nullptr;
            } else {
                return rmdu(tl2);
            }
        } else {
            if (!tl2 || !tl2->head) {
                return rmdu(tl1);
            } else {
                std::set<TEMP::Temp *> *set = new std::set<TEMP::Temp *>{};
                while (tl1) {
                    set->insert(tl1->head);
                    tl1 = tl1->tail;
                }
                while (tl2) {
                    set->insert(tl2->head);
                    tl2 = tl2->tail;
                }
                TEMP::TempList *r = nullptr, *tail;
                for (auto it : *set) {
                    if (!r) {
                        r = new TEMP::TempList(it, nullptr);
                        tail = r;
                    } else {
                        tail = tail->tail = new TEMP::TempList(it, nullptr);
                    }
                }
                return r;
            }
        }
    }

    TEMP::TempList *intersectList(TEMP::TempList *tl1, TEMP::TempList *tl2) {
        if (!tl1 || !tl1->head || !tl2 || !tl2->head) {
            return nullptr;
        }
        TEMP::TempList *r = nullptr, *tail = nullptr;
        while (tl1) {
            TEMP::TempList *tl2_ = tl2;
            while (tl2_) {
                if (tl1->head == tl2_->head) {
                    break;
                }
                tl2_ = tl2_->tail;
            }
            if (tl2_) {
                if (!r) {
                    r = new TEMP::TempList(tl1->head, nullptr);
                    tail = r;
                } else {
                    tail = tail->tail = new TEMP::TempList(tl1->head, nullptr);
                }
            }
            tl1 = tl1->tail;
        }
        return rmdu(r);
    }

    TEMP::TempList *minusList(TEMP::TempList *tl1, TEMP::TempList *tl2) {
        std::set<TEMP::Temp *> *set1 = new std::set<TEMP::Temp *>{};
        std::set<TEMP::Temp *> *set2 = new std::set<TEMP::Temp *>{};
        while (tl1 && tl1->head) {
            set1->insert(tl1->head);
            tl1 = tl1->tail;
        }
        while (tl2 && tl2->head) {
            set2->insert(tl2->head);
            tl2 = tl2->tail;
        }
        TEMP::TempList *r = nullptr, *tail = nullptr;
        for (auto it : *set1) {
            if (set2->find(it) != set2->end()) {
                continue;
            }
            if (!r) {
                r = new TEMP::TempList(it, nullptr);
                tail = r;
            } else {
                tail = tail->tail = new TEMP::TempList(it, nullptr);
            }
        }
        if (!r || !r->head) {
            return nullptr;
        } else {
            return r;
        }
    }

    bool equalList(TEMP::TempList *tl1, TEMP::TempList *tl2) {
        // TODO: RMDU?
        return  listLen(tl1) == listLen(tl2) && listLen(intersectList(tl1, tl2)) == listLen(tl2);
    }
    void TempList::Print() {
        TempList *p = this;
        while (p) {
            printf("%d  ", p->head->Int());
            p = p->tail;
        }
    }
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