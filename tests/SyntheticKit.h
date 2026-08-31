#pragma once

// Синтетика для DSP-тестов: шум, дробная задержка, импульсный поезд.
// Реальные сессии в tests/ не тащим — они гоняются руками в Standalone.

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace beat::test
{

inline constexpr double kPi = std::numbers::pi;

inline double sinc(double x)
{
    if (std::abs(x) < 1.0e-12)
        return 1.0;

    const double z = kPi * x;
    return std::sin(z) / z;
}

inline std::vector<float> whiteNoise(int n, unsigned seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> x(static_cast<size_t>(n));
    for (auto& sample : x)
        sample = dist(rng);
    return x;
}

inline std::vector<float> delaySignal(const std::vector<float>& x,
                                      float delaySamples,
                                      bool invert = false)
{
    const int n = static_cast<int>(x.size());
    constexpr int taps = 32;
    std::vector<float> y(static_cast<size_t>(n), 0.0f);
    const float sign = invert ? -1.0f : 1.0f;

    for (int i = 0; i < n; ++i)
    {
        const double center = static_cast<double>(i) - static_cast<double>(delaySamples);
        double acc = 0.0;

        for (int t = static_cast<int>(std::floor(center)) - taps;
             t <= static_cast<int>(std::floor(center)) + taps;
             ++t)
        {
            if (t < 0 || t >= n)
                continue;

            const double p = center - static_cast<double>(t);
            if (std::abs(p) > taps)
                continue;

            const double window = 0.5 + 0.5 * std::cos(kPi * p / static_cast<double>(taps));
            acc += static_cast<double>(x[static_cast<size_t>(t)]) * sinc(p) * window;
        }

        y[static_cast<size_t>(i)] = sign * static_cast<float>(acc);
    }

    return y;
}

inline std::vector<float> impulseTrain(int n, int period, int offset)
{
    std::vector<float> x(static_cast<size_t>(n), 0.0f);
    for (int i = offset; i < n; i += period)
        x[static_cast<size_t>(i)] = 1.0f;
    return x;
}

} // namespace beat::test
