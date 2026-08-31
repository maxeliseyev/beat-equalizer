#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "SyntheticKit.h"
#include "dsp/AllpassRotator.h"

#include <cmath>
#include <complex>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
constexpr double kSampleRate = 48000.0;

float energyRatio(float rotatorHz, float amount, float toneHz)
{
    beat::AllpassRotator rotator;
    rotator.prepare(kSampleRate, 1);
    rotator.setRotation(0, rotatorHz, amount);
    rotator.reset();

    const int n = 48000;
    double dry = 0.0;
    double wet = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const float x = std::sin(2.0f * static_cast<float>(beat::test::kPi) * toneHz
                                 * static_cast<float>(i) / static_cast<float>(kSampleRate));
        const float y = rotator.processSample(0, x);

        if (i < 4800) // пропускаем сглаживание коэффициентов
            continue;

        dry += static_cast<double>(x) * x;
        wet += static_cast<double>(y) * y;
    }

    return static_cast<float>(std::sqrt(wet / dry));
}
} // namespace

TEST_CASE("allpass keeps the magnitude and only turns the phase")
{
    for (float toneHz : { 100.0f, 600.0f, 3000.0f, 9000.0f })
        REQUIRE_THAT(energyRatio(600.0f, 1.0f, toneHz), WithinAbs(1.0f, 0.02f));
}

TEST_CASE("amount zero is a bypass")
{
    beat::AllpassRotator rotator;
    rotator.prepare(kSampleRate, 2);
    rotator.setRotation(0, 600.0f, 0.0f);
    rotator.reset();

    for (float x : { 0.3f, -0.7f, 0.15f, 1.0f })
        REQUIRE_THAT(rotator.processSample(0, x), WithinAbs(x, 1.0e-6f));
}

TEST_CASE("rotator phase is -90 degrees at its own frequency")
{
    const float hz = 600.0f;
    const auto atFc = beat::AllpassRotator::response(hz, 1.0f, hz, kSampleRate);
    REQUIRE_THAT(std::arg(atFc), WithinAbs(-0.5f * static_cast<float>(beat::test::kPi), 0.02f));
    REQUIRE_THAT(std::abs(atFc), WithinAbs(1.0f, 1.0e-4f));

    // Ниже частоты вращения фаза почти не сдвинута, выше — стремится к -180.
    REQUIRE(std::arg(beat::AllpassRotator::response(hz, 1.0f, 20.0f, kSampleRate)) > -0.2f);
    REQUIRE(std::arg(beat::AllpassRotator::response(hz, 1.0f, 16000.0f, kSampleRate)) > -3.15f);
    REQUIRE(std::arg(beat::AllpassRotator::response(hz, 1.0f, 16000.0f, kSampleRate)) < -2.7f);
}

TEST_CASE("response matches what the realtime cascade actually does")
{
    const float rotatorHz = 900.0f;
    const float amount = 0.6f;
    const float toneHz = 400.0f;

    beat::AllpassRotator rotator;
    rotator.prepare(kSampleRate, 1);
    rotator.setRotation(0, rotatorHz, amount);
    rotator.reset();

    const int n = 24000;
    std::vector<float> out(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        const float x = std::sin(2.0f * static_cast<float>(beat::test::kPi) * toneHz
                                 * static_cast<float>(i) / static_cast<float>(kSampleRate));
        out[static_cast<size_t>(i)] = rotator.processSample(0, x);
    }

    // Разбираем установившийся отрезок на синус и косинус той же частоты.
    double re = 0.0;
    double im = 0.0;
    const int start = 12000;
    for (int i = start; i < n; ++i)
    {
        const double w = 2.0 * beat::test::kPi * toneHz * i / kSampleRate;
        re += out[static_cast<size_t>(i)] * std::sin(w);
        im += out[static_cast<size_t>(i)] * std::cos(w);
    }

    const auto measured = std::complex<double>(re, im) * (2.0 / (n - start));
    const auto expected = beat::AllpassRotator::response(rotatorHz, amount, toneHz, kSampleRate);

    REQUIRE_THAT(static_cast<float>(std::abs(measured)), WithinAbs(std::abs(expected), 0.02f));
    REQUIRE_THAT(static_cast<float>(std::arg(measured)), WithinAbs(std::arg(expected), 0.05f));
}

TEST_CASE("rotator ignores channels it was never prepared for")
{
    beat::AllpassRotator rotator;
    rotator.prepare(kSampleRate, 2);
    REQUIRE(rotator.processSample(5, 0.5f) == 0.5f);
    REQUIRE(rotator.processSample(-1, 0.5f) == 0.5f);
}
