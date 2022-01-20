#include "tiger/frame/temp.h"

#include <cstdio>
#include <set>
#include <sstream>

namespace temp {

    LabelFactory LabelFactory::label_factory;
    TempFactory TempFactory::temp_factory;

    Label *LabelFactory::NewLabel() {
        char buf[100];
        sprintf(buf, "L%d", label_factory.label_id_++);
        return NamedLabel(std::string(buf));
    }

    /**
     * Get symbol of a label_. The label_ will be created only if it is not found.
     * @param s label_ string
     * @return symbol
     */
    Label *LabelFactory::NamedLabel(std::string_view s) {
        return sym::Symbol::UniqueSymbol(s);
    }

    std::string LabelFactory::LabelString(Label *s) {
        return s->Name();
    }

    Temp *TempFactory::NewTemp() {
        Temp *p = new Temp(temp_factory.temp_id_++);
        std::stringstream stream;
        stream << 't';
        stream << p->num_;
        Map::Name()->Enter(p, new std::string(stream.str()));

        return p;
    }

    int Temp::Int() const {
        return num_;
    }

    Map *Map::Empty() {
        return new Map();
    }

    Map *Map::Name() {
        static Map *m = nullptr;
        if (!m)
            m = Empty();
        return m;
    }

    Map *Map::LayerMap(Map *over, Map *under) {
        if (over == nullptr)
            return under;
        else
            return new Map(over->tab_, LayerMap(over->under_, under));
    }

    void Map::Enter(Temp *t, std::string *s) {
        assert(tab_);
        tab_->Enter(t, s);
    }

    std::string *Map::Look(Temp *t) {
        std::string *s;
        assert(tab_);
        s = tab_->Look(t);
        if (s)
            return s;
        else if (under_)
            return under_->Look(t);
        else
            return nullptr;
    }

    void Map::DumpMap(FILE *out) {
        tab_->Dump([out](temp::Temp *t, std::string *r) {
            fprintf(out, "t%d -> %s\n", t->Int(), r->data());
        });
        if (under_) {
            fprintf(out, "---------\n");
            under_->DumpMap(out);
        }
    }

    void TempList::replaceTemp(temp::Temp *oldt, temp::Temp *newt) {
        std::list<temp::Temp *> new_list;
        for (temp::Temp *t : temp_list_) {
            if (t == oldt) {
                new_list.push_back(newt);
            } else {
                new_list.push_back(t);
            }
        }
        temp_list_ = new_list;
    }

    bool TempList::Contain(temp::Temp *temp) const {
        for (auto t : temp_list_) {
            if (t == temp) {
                return true;
            }
        }
        return false;
    }

    bool TempList::Equal(temp::TempList *another) const {
        if (temp_list_.size() != another->temp_list_.size()) {
            return false;
        }
        for (auto t : temp_list_) {
            if (!another->Contain(t)) {
                return false;
            }
        }
        // 避免重复再比较一次
        for (auto t : another->GetList()) {
            if (!Contain(t)) {
                return false;
            }
        }
        return true;
    }

    temp::TempList *TempList::Union(temp::TempList *another) const {
        auto res = new temp::TempList();
        for (auto t : temp_list_) {
            res->Append(t);
        }
        for (auto t : another->temp_list_) {
            if (!this->Contain(t)) {
                res->Append(t);
            }
        }
        return res;
    }

    temp::TempList *TempList::Diff(temp::TempList *another) const {
        auto res = new temp::TempList();
        for (auto t : temp_list_) {
            if (!another->Contain(t)) {
                res->Append(t);
            }
        }
        return res;
    }

    temp::TempList *TempList::Intersect(temp::TempList *another) const {
        auto res = new temp::TempList();
        for (auto t : another->temp_list_) {
            if (this->Contain(t)) {
                res->Append(t);
            }
        }
        return res;
    }

} // namespace temp