#include <algorithm>
#include <chrono>
#include <iostream>

#include "dllist6.hh"


int main()
{
    
    /* 1. Тип без конструктора по умолчанию */
    DList<NoDefault> nd;
    nd.push_back(NoDefault(3));
    nd.push_back(NoDefault(1));
    nd.push_back(NoDefault(2));
    std::cout << "без дефолтного конструктора: размер " << nd.size() << '\n';

    /* 2. Двунаправленный итератор */
    DList<int> d;
    for (int i = 1; i <= 5; ++i)
        d.push_back(i);

    std::cout << "прямо:";
    for (int v : d) std::cout << ' ' << v;
    std::cout << "\nобратно:";
    for (auto it = d.end(); it != d.begin();) { --it; std::cout << ' ' << *it; }
    std::cout << '\n';

    auto found = std::find(d.begin(), d.end(), 3);
    std::cout << "std::find нашёл 3: " << (found != d.end() ? "да" : "нет") << '\n';

    std::reverse(d.begin(), d.end());
    std::cout << "после std::reverse:";
    for (int v : d) std::cout << ' ' << v;
    std::cout << '\n';

    /* 3. Компаратор снаружи */
    DList<int> sorted([](const int &a, const int &b) { return a > b; });
    for (int v : {3, 1, 4, 1, 5})
        sorted.insert_sorted(v);
    std::cout << "по убыванию:";
    for (int v : sorted) std::cout << ' ' << v;
    std::cout << '\n';

    /* 4. Замер пула */
    {
        DList<int, HeapNodeSource<int>> heap_list;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 100000; ++i) heap_list.push_back(i);
        auto t1 = std::chrono::steady_clock::now();

        DList<int, PoolNodeSource<int>> pool_list;
        auto t2 = std::chrono::steady_clock::now();
        for (int i = 0; i < 100000; ++i) pool_list.push_back(i);
        auto t3 = std::chrono::steady_clock::now();

        std::printf("куча: %.2f мс, пул: %.2f мс\n",
                    std::chrono::duration<double, std::milli>(t1 - t0).count(),
                    std::chrono::duration<double, std::milli>(t3 - t2).count());
    }

    /* 5. Декораторы: сначала строим внутренний объект, потом оборачиваем его */
    DList<int> base;
    StatsDecorator<DList<int>> stats(base);
    ReadOnlyDecorator<DList<int>> ro(base);
    StatsDecorator<ReadOnlyDecorator<DList<int>>> stats_ro(ro);

    stats.push_back(1);
    stats.push_back(2);
    std::cout << "статистика: вставок " << stats.pushes()
              << ", извлечений " << stats.pops() << '\n';

    try {
        stats.push_front(0);
    } catch (...) {
        std::cout << "неожиданно: запись отклонена\n";
    }

    try {
        ro.push_front(0);
    } catch (const std::logic_error &e) {
        std::cout << "ro отклонил запись: " << e.what() << '\n';
    }
    try {
        stats_ro.pop_front();
    } catch (const std::logic_error &e) {
        std::cout << "stats_ro отклонил извлечение: " << e.what() << '\n';
    }
    std::cout << "чтение через декораторы работает: размер " << stats_ro.size() << '\n';
    return 0;
}
