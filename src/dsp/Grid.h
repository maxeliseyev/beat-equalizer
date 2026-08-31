#pragma once

#include <cmath>

namespace beat::grid
{

// Деления сетки в четвертях. Сетка живёт в PPQ (четвертях), а не в секундах:
// хост отдаёт позицию именно так, и при смене темпа линии не расползаются.
enum class Division
{
    off,
    quarter,
    eighth,
    eighthTriplet,
    sixteenth,
    sixteenthTriplet,
    thirtySecond
};

inline constexpr int kDivisionCount = 7;
inline constexpr int kMaxLines = 96;

struct Line
{
    float position = 0.0f; // 0…1 внутри окна
    bool beat = false;     // попадает на долю (четверть)
    bool bar = false;      // попадает на начало такта
};

inline double stepQuarters(Division division)
{
    switch (division)
    {
        case Division::quarter:
            return 1.0;
        case Division::eighth:
            return 0.5;
        case Division::eighthTriplet:
            return 1.0 / 3.0;
        case Division::sixteenth:
            return 0.25;
        case Division::sixteenthTriplet:
            return 1.0 / 6.0;
        case Division::thirtySecond:
            return 0.125;
        case Division::off:
            break;
    }

    return 0.0;
}

// Длина такта в четвертях. 4/4 — четыре, 6/8 — три.
inline double barQuarters(int numerator, int denominator)
{
    if (numerator <= 0 || denominator <= 0)
        return 4.0;

    return static_cast<double>(numerator) * 4.0 / static_cast<double>(denominator);
}

inline double quartersPerSecond(double bpm) { return bpm / 60.0; }

// Линии сетки внутри окна [start, start + length) в четвертях.
inline int linesInWindow(double startQuarters,
                         double lengthQuarters,
                         double step,
                         double bar,
                         Line* out,
                         int maxLines)
{
    if (out == nullptr || maxLines <= 0 || step <= 0.0 || lengthQuarters <= 0.0)
        return 0;

    constexpr double eps = 1.0e-6;
    auto index = static_cast<long long>(std::ceil(startQuarters / step - eps));
    int count = 0;

    for (; count < maxLines; ++index)
    {
        const double quarters = static_cast<double>(index) * step;
        if (quarters >= startQuarters + lengthQuarters)
            break;

        Line line;
        line.position = static_cast<float>((quarters - startQuarters) / lengthQuarters);
        line.beat = std::abs(quarters - std::round(quarters)) < eps;

        if (line.beat && bar > 0.0)
        {
            const double inBar = quarters - std::floor(quarters / bar + eps) * bar;
            line.bar = std::abs(inBar) < eps;
        }

        out[count++] = line;
    }

    return count;
}

} // namespace beat::grid
