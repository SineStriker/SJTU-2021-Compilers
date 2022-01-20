#ifndef TIGER_FRAME_TEMP_H_
#define TIGER_FRAME_TEMP_H_

#include "tiger/symbol/symbol.h"

#include <list>
#include <utility>

namespace temp {

    using Label = sym::Symbol;

    class LabelFactory {
    public:
        static Label *NewLabel();
        static Label *NamedLabel(std::string_view name);
        static std::string LabelString(Label *s);

    private:
        int label_id_ = 0;
        static LabelFactory label_factory;
    };

    class Temp {
        friend class TempFactory;

    public:
        [[nodiscard]] int Int() const;

    private:
        int num_;
        explicit Temp(int num) : num_(num) {
        }
    };

    class TempFactory {
    public:
        static Temp *NewTemp();

    private:
        int temp_id_ = 100;
        static TempFactory temp_factory;
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
        tab::Table<Temp, std::string> *tab_;
        Map *under_;

        Map() : tab_(new tab::Table<Temp, std::string>()), under_(nullptr) {
        }
        Map(tab::Table<Temp, std::string> *tab, Map *under) : tab_(tab), under_(under) {
        }
    };

    class TempList {
    public:
        explicit TempList(Temp *t) : temp_list_({t}) {
        }
        explicit TempList(std::list<Temp *> list) : temp_list_(std::move(list)) {
        }
        TempList(std::initializer_list<Temp *> list) : temp_list_(list) {
        }
        TempList() = default;
        void Append(Temp *t) {
            temp_list_.push_back(t);
        }
        [[nodiscard]] Temp *NthTemp(int i) const;
        [[nodiscard]] const std::list<Temp *> &GetList() const {
            return temp_list_;
        }
        // add
        void replaceTemp(temp::Temp *oldt, temp::Temp *newt);

        bool Contain(temp::Temp *temp) const;      // 判断链表中是否存在某寄存器
        bool Equal(temp::TempList *another) const; // 判断两个链表是否存储了相同的寄存器

        temp::TempList *Union(temp::TempList *another) const; // 求并集 A+B
        temp::TempList *Diff(temp::TempList *another) const; // 减去传入链表中已存在的元素 A-B

        temp::TempList *Intersect(temp::TempList *another) const; // 求交集 A·B

    private:
        std::list<Temp *> temp_list_;
    };

} // namespace temp

#endif