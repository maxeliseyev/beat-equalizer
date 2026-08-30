#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/Constants.h"
#include "dsp/GccPhat.h"

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
constexpr double kPi = std::numbers::pi;

double sinc(double x)
{
    if (std::abs(x) < 1.0e-12)
        return 1.0;
    const double z = kPi * x;
    return std::sin(z) / z;
}

std::vector<float> whiteNoise(int n, unsigned seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> x(static_cast<size_t>(n));
    for (auto& sample : x)
        sample = dist(rng);
    return x;
}

std::vector<float> delaySignal(const std::vector<float>& x, float delaySamples, bool invert = false)
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

std::vector<float> impulseTrain(int n, int period, int offset)
{
    std::vector<float> x(static_cast<size_t>(n), 0.0f);
    for (int i = offset; i < n; i += period)
        x[static_cast<size_t>(i)] = 1.0f;
    return x;
}

beat::GccPhat::Result estimateDelay(const std::vector<float>& reference,
                                    const std::vector<float>& signal,
                                    double sampleRate,
                                    float distanceM = beat::kDefaultMaxDistanceM)
{
    beat::GccPhat gcc;
    return gcc.estimate(reference.data(),
                        signal.data(),
                        static_cast<int>(reference.size()),
                        beat::maxLagSamples(distanceM, sampleRate),
                        sampleRate);
}
} // namespace

TEST_CASE("GCC-PHAT finds integer delay of an impulse")
{
    std::vector<float> reference(8192, 0.0f);
    reference[200] = 1.0f;
    const auto signal = delaySignal(reference, 8.0f);

    const auto result = estimateDelay(reference, signal, 48000.0);
    REQUIRE(result.valid);
    REQUIRE_THAT(result.lagSamples, WithinAbs(8.0f, 0.1f));
    REQUIRE_FALSE(result.invert);
}

TEST_CASE("GCC-PHAT sign: delayed signal is later than the reference")
{
    auto reference = whiteNoise(8192, 1);
    const auto earlier = delaySignal(reference, -5.0f);
    const auto later = delaySignal(reference, 5.0f);

    const auto negative = estimateDelay(reference, earlier, 48000.0);
    const auto positive = estimateDelay(reference, later, 48000.0);

    REQUIRE(negative.valid);
    REQUIRE(positive.valid);
    REQUIRE_THAT(negative.lagSamples, WithinAbs(-5.0f, 0.1f));
    REQUIRE_THAT(positive.lagSamples, WithinAbs(5.0f, 0.1f));
}

TEST_CASE("GCC-PHAT subsample delays on broadband noise")
{
    auto reference = whiteNoise(8192, 42);
    beat::GccPhat gcc;
    const int maxLag = beat::maxLagSamples(beat::kDefaultMaxDistanceM, 48000.0);

    for (float delay : { 0.0f, 0.25f, 0.5f, 1.7f, 5.5f, 11.3f })
    {
        const auto signal = delaySignal(reference, delay);
        const auto result = gcc.estimate(reference.data(), signal.data(), 8192, maxLag, 48000.0);
        REQUIRE(result.valid);
        REQUIRE_THAT(result.lagSamples, WithinAbs(delay, 0.1f));
        REQUIRE_FALSE(result.invert);
    }
}

TEST_CASE("GCC-PHAT polarity comes from unweighted correlation, not PHAT")
{
    auto reference = whiteNoise(8192, 7);
    const auto signal = delaySignal(reference, 4.0f, true);

    const auto result = estimateDelay(reference, signal, 48000.0);
    REQUIRE(result.valid);
    REQUIRE_THAT(result.lagSamples, WithinAbs(4.0f, 0.1f));
    REQUIRE(result.invert);
    REQUIRE(result.unweightedAtLag < 0.0f);
}

TEST_CASE("GCC-PHAT ignores a 20 ms period outside the 12 ms window")
{
    constexpr double sampleRate = 48000.0;
    constexpr int period = 960; // 20 ms
    constexpr int trueDelay = 5;
    auto reference = impulseTrain(8192, period, 100);
    const auto signal = delaySignal(reference, static_cast<float>(trueDelay));

    const auto result = estimateDelay(reference, signal, sampleRate);
    REQUIRE(result.valid);
    REQUIRE_THAT(result.lagSamples, WithinAbs(static_cast<float>(trueDelay), 0.5f));
    REQUIRE(std::abs(result.lagSamples) < 400.0f);
}

TEST_CASE("GCC-PHAT max lag scales so 4 m at 96 kHz still finds 5.5 samples")
{
    auto reference = whiteNoise(8192, 99);
    const auto signal = delaySignal(reference, 5.5f);

    const auto result = estimateDelay(reference, signal, 96000.0);
    REQUIRE(result.valid);
    REQUIRE_THAT(result.lagSamples, WithinAbs(5.5f, 0.1f));
}

TEST_CASE("GCC-PHAT rejects empty or zero-lag requests")
{
    beat::GccPhat gcc;
    std::vector<float> x(64, 1.0f);
    auto bad = gcc.estimate(x.data(), x.data(), 64, 0, 48000.0);
    REQUIRE_FALSE(bad.valid);
    bad = gcc.estimate(nullptr, x.data(), 64, 10, 48000.0);
    REQUIRE_FALSE(bad.valid);
}
