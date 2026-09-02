#pragma once

#include "doc/Event.h"
#include "doc/Ids.h"

namespace beat::doc
{

// Единственное место, где у удара появляется интервал. Это не «отсчёты
// принадлежат удару», а решение: сюда варп не заходит.
struct Interval
{
    SamplePos start = 0.0;
    SamplePos end = 0.0;

    bool empty() const { return !(end > start); }
    double length() const { return end > start ? end - start : 0.0; }
    bool contains(SamplePos pos) const { return pos >= start && pos < end; }
};

// Объединение атак по всем микрофонам, а не атака опорного канала
// (инвариант 17): карта времени общая, и растяжение в момент, когда оверхед
// ещё в атаке, смажет оверхед, даже если близкий микрофон уже отзвучал.
//
//     [ min_j arrival_j , max_j attackEnd_j ] + запас
//
// Цена — зона длиннее одноканальной на разброс расстояний. Она честная и её
// показывают, а не срезают.
Interval protectedZone(const Event& event,
                       double sampleRate,
                       float marginMs = kProtectedMarginMs);

// Сколько остаётся на растяжение между двумя соседними ударами. Отрицательное
// значение — бюджет кончился: на этом темпе так квантизовать нельзя, и
// инструмент обязан сказать об этом, а не растянуть атаку.
double warpBudget(const Interval& previous, const Interval& next);

} // namespace beat::doc
