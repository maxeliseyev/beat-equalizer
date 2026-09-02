#pragma once

// Синтетика для DSP-тестов: шум, дробная задержка, импульсный поезд.
// Реальные сессии в tests/ не тащим — они гоняются руками в Standalone.

#include <algorithm>
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


// Многоканальный дубль с заранее известными временами и задержками: стенд, на
// котором точность детектора и сверки по микрофонам меряется в сэмплах, а не
// на слух (detector-design 1.9). Реальные сессии сюда не кладём.
struct KitInstrument
{
    std::vector<int> hitSamples;        // моменты удара у источника
    float decayPerSecond = 30.0f;       // e-складок в секунду
    float toneHz = 200.0f;              // тело удара
    float noiseMix = 0.5f;              // доля шума в тембре
    std::vector<float> arrivalSamples;  // приход в каждый канал, дробный
    std::vector<float> gain;            // усиление в канале; 0 — не слышно
};

struct KitSpec
{
    double sampleRate = 48000.0;
    int numChannels = 4;
    int numSamples = 48000;
    std::vector<KitInstrument> instruments;
    float noiseFloor = 0.0f;
    unsigned seed = 1234;
};

// Один удар: шум с телом, затухающий по экспоненте. Барабан этим не описать,
// но времена прихода, спад и просачивание — ровно то, что проверяется.
inline std::vector<float> drumSource(const KitInstrument& instrument,
                                     int numSamples,
                                     double sampleRate,
                                     unsigned seed)
{
    std::vector<float> x(static_cast<size_t>(numSamples), 0.0f);
    std::mt19937 rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    const double decay = std::max(0.1, static_cast<double>(instrument.decayPerSecond));
    const int tail = static_cast<int>(std::ceil(9.21 / decay * sampleRate));

    for (int hit : instrument.hitSamples)
    {
        for (int i = 0; i < tail; ++i)
        {
            const int pos = hit + i;
            if (pos < 0 || pos >= numSamples)
                continue;

            const double t = static_cast<double>(i) / sampleRate;
            const double envelope = std::exp(-decay * t);
            const double tone = std::sin(2.0 * kPi * static_cast<double>(instrument.toneHz) * t);
            const double mix = (1.0 - static_cast<double>(instrument.noiseMix)) * tone
                               + static_cast<double>(instrument.noiseMix) * dist(rng);

            x[static_cast<size_t>(pos)] += static_cast<float>(envelope * mix);
        }
    }

    return x;
}

inline std::vector<std::vector<float>> renderKit(const KitSpec& spec)
{
    std::vector<std::vector<float>> channels(
        static_cast<size_t>(spec.numChannels),
        std::vector<float>(static_cast<size_t>(spec.numSamples), 0.0f));

    unsigned seed = spec.seed;
    for (const auto& instrument : spec.instruments)
    {
        const auto source = drumSource(instrument, spec.numSamples, spec.sampleRate, seed++);

        for (int ch = 0; ch < spec.numChannels; ++ch)
        {
            const auto index = static_cast<size_t>(ch);
            const float gain = index < instrument.gain.size() ? instrument.gain[index] : 0.0f;
            if (gain == 0.0f)
                continue;

            const float arrival = index < instrument.arrivalSamples.size()
                                      ? instrument.arrivalSamples[index]
                                      : 0.0f;

            const auto delayed = delaySignal(source, arrival);
            for (size_t i = 0; i < delayed.size(); ++i)
                channels[index][i] += gain * delayed[i];
        }
    }

    if (spec.noiseFloor > 0.0f)
    {
        std::mt19937 rng(spec.seed ^ 0x9e3779b9u);
        std::normal_distribution<float> dist(0.0f, spec.noiseFloor);
        for (auto& channel : channels)
            for (auto& sample : channel)
                sample += dist(rng);
    }

    return channels;
}

} // namespace beat::test
