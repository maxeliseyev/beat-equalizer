#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "SyntheticKit.h"
#include "dsp/AllpassRotator.h"
#include "dsp/Coherence.h"

#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;
using beat::test::delaySignal;
using beat::test::whiteNoise;

namespace
{
constexpr double kSampleRate = 48000.0;

float coherenceOf(const std::vector<float>& a,
                  const std::vector<float>& b,
                  const beat::Coherence::Transform& transform = {})
{
    beat::Coherence coherence;
    coherence.setPair(a.data(), b.data(), static_cast<int>(a.size()), kSampleRate);
    return coherence.measure(transform);
}
} // namespace

TEST_CASE("identical signals sum to full coherence, inverted ones cancel")
{
    const auto x = whiteNoise(8192, 4);

    REQUIRE_THAT(coherenceOf(x, x), WithinAbs(1.0f, 0.001f));

    beat::Coherence::Transform flipped;
    flipped.invert = true;
    REQUIRE_THAT(coherenceOf(x, x, flipped), WithinAbs(0.0f, 0.001f));
}

TEST_CASE("a delayed copy loses coherence and the transform gets it back")
{
    const auto x = whiteNoise(8192, 6);
    const auto later = delaySignal(x, 24.0f);

    const float raw = coherenceOf(x, later);
    REQUIRE(raw < 0.8f);

    beat::Coherence::Transform aligned;
    aligned.delaySamples = -24.0f; // канал звучит позже, значит двигаем его вперёд
    REQUIRE(coherenceOf(x, later, aligned) > 0.97f);
}

TEST_CASE("polarity and delay together beat either one alone")
{
    const auto x = whiteNoise(8192, 10);
    const auto flippedLater = delaySignal(x, 11.3f, true);

    beat::Coherence::Transform delayOnly;
    delayOnly.delaySamples = -11.3f;

    beat::Coherence::Transform both = delayOnly;
    both.invert = true;

    const float raw = coherenceOf(x, flippedLater);
    const float withDelay = coherenceOf(x, flippedLater, delayOnly);
    const float withBoth = coherenceOf(x, flippedLater, both);

    REQUIRE(withBoth > 0.97f);
    REQUIRE(withBoth > withDelay);
    REQUIRE(withBoth > raw);
}

TEST_CASE("uncorrelated channels sit near the no-gain middle")
{
    const auto a = whiteNoise(8192, 1);
    const auto b = whiteNoise(8192, 2);

    const float value = coherenceOf(a, b);
    REQUIRE(value > 0.4f);
    REQUIRE(value < 0.85f);
}

TEST_CASE("rotator undoes an all-pass smeared channel")
{
    const auto x = whiteNoise(8192, 15);

    // Канал прошёл через ротатор: задержка и полярность его не спасут.
    beat::AllpassRotator rotator;
    rotator.prepare(kSampleRate, 1);
    rotator.setRotation(0, 900.0f, 1.0f);
    rotator.reset();

    std::vector<float> smeared(x.size());
    for (size_t i = 0; i < x.size(); ++i)
        smeared[i] = rotator.processSample(0, x[i]);

    const float raw = coherenceOf(x, smeared);

    beat::Coherence::Transform rotated;
    rotated.rotatorHz = 900.0f;
    rotated.rotatorAmount = 1.0f;
    const float twice = coherenceOf(x, smeared, rotated);

    // Второй проход тем же олпассом не «развернёт» фазу обратно, но метрика
    // обязана видеть разницу между вариантами — иначе перебор бессмыслен.
    REQUIRE(raw < 0.95f);
    REQUIRE(std::abs(twice - raw) > 0.01f);
}

TEST_CASE("an unprepared pair reports zero instead of guessing")
{
    beat::Coherence coherence;
    REQUIRE(coherence.measureRaw() == 0.0f);

    const auto x = whiteNoise(64, 3);
    coherence.setPair(x.data(), x.data(), 8, kSampleRate);
    REQUIRE(coherence.measureRaw() == 0.0f);
}
