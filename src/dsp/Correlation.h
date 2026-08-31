#pragma once

#include <cmath>

namespace beat
{

// Коэффициент корреляции пары: sum(ab) / sqrt(sum(a^2) * sum(b^2)), -1…+1.
// Среднее не вычитаем: аудиокадр уже центрирован, а постоянная составляющая
// в тракте ударных — это или DC-offset, или ошибка выше по цепи.
// stride > 1 прореживает окно: коррелометр рисуется 25 раз в секунду.
inline float correlation(const float* a, const float* b, int count, int stride = 1)
{
    if (a == nullptr || b == nullptr || count <= 0 || stride <= 0)
        return 0.0f;

    double sumAb = 0.0;
    double sumAa = 0.0;
    double sumBb = 0.0;

    for (int i = 0; i < count; i += stride)
    {
        const double x = a[i];
        const double y = b[i];
        sumAb += x * y;
        sumAa += x * x;
        sumBb += y * y;
    }

    const double denom = std::sqrt(sumAa * sumBb);
    if (denom <= 0.0)
        return 0.0f;

    const double r = sumAb / denom;
    return static_cast<float>(std::fmax(-1.0, std::fmin(1.0, r)));
}

} // namespace beat
