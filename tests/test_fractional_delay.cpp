#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/FractionalDelay.h"
#include "dsp/LatencyModel.h"

#include <algorithm>
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
std::vector<float> runImpulse(beat::FractionalDelay& delay, int channel, int length)
{
    std::vector<float> out(static_cast<size_t>(length), 0.0f);
    for (int i = 0; i < length; ++i)
    {
        const float x = (i == 0) ? 1.0f : 0.0f;
        out[static_cast<size_t>(i)] = delay.processSample(channel, x);
    }
    return out;
}

float peakIndex(const std::vector<float>& x)
{
    int best = 0;
    float bestAbs = -1.0f;
    for (int i = 1; i + 1 < static_cast<int>(x.size()); ++i)
    {
        const float a = std::abs(x[static_cast<size_t>(i)]);
        if (a > bestAbs)
        {
            bestAbs = a;
            best = i;
        }
    }

    const float ym1 = std::abs(x[static_cast<size_t>(best - 1)]);
    const float y0 = std::abs(x[static_cast<size_t>(best)]);
    const float yp1 = std::abs(x[static_cast<size_t>(best + 1)]);
    const float denom = ym1 - 2.0f * y0 + yp1;
    float delta = 0.0f;
    if (std::abs(denom) > 1.0e-12f)
        delta = 0.5f * (ym1 - yp1) / denom;
    return static_cast<float>(best) + std::clamp(delta, -1.0f, 1.0f);
}

void settle(beat::FractionalDelay& delay, int channel, int samples)
{
    for (int i = 0; i < samples; ++i)
        delay.processSample(channel, 0.0f);
}
} // namespace

TEST_CASE("integer applied delay shifts an impulse by applied + interpolator")
{
    beat::FractionalDelay delay;
    delay.prepare(48000.0, 1);
    delay.setAppliedDelaySamples(0, 8.0f);
    settle(delay, 0, 4096);

    const auto out = runImpulse(delay, 0, 64);
    const float peak = peakIndex(out);
    const float expected = 8.0f + static_cast<float>(beat::kInterpolatorLatencySamples);
    REQUIRE_THAT(peak, WithinAbs(expected, 0.15f));
}

TEST_CASE("fractional applied delay is accurate to a fraction of a sample")
{
    beat::FractionalDelay delay;
    delay.prepare(48000.0, 1);
    delay.setAppliedDelaySamples(0, 5.5f);
    settle(delay, 0, 4096);

    const auto out = runImpulse(delay, 0, 64);
    const float peak = peakIndex(out);
    const float expected = 5.5f + static_cast<float>(beat::kInterpolatorLatencySamples);
    REQUIRE_THAT(peak, WithinAbs(expected, 0.15f));
}

TEST_CASE("invert flips the impulse polarity")
{
    beat::FractionalDelay delay;
    delay.prepare(48000.0, 1);
    delay.setAppliedDelaySamples(0, 4.0f);
    delay.setInvert(0, true);
    settle(delay, 0, 4096);

    const auto out = runImpulse(delay, 0, 32);
    float minValue = 0.0f;
    for (float sample : out)
        minValue = std::min(minValue, sample);
    REQUIRE(minValue < -0.5f);
}

TEST_CASE("zero applied delay still pays interpolator latency")
{
    beat::FractionalDelay delay;
    delay.prepare(48000.0, 1);
    delay.setAppliedDelaySamples(0, 0.0f);
    settle(delay, 0, 4096);

    const auto out = runImpulse(delay, 0, 32);
    REQUIRE_THAT(peakIndex(out), WithinAbs(static_cast<float>(beat::kInterpolatorLatencySamples), 0.15f));
}

TEST_CASE("dry path delayed by reported latency matches wet max-applied channel")
{
    constexpr float applied = 11.3f;
    const int latency = beat::LatencyModel::reportedLatency(applied);

    beat::FractionalDelay wet;
    wet.prepare(48000.0, 1);
    wet.setAppliedDelaySamples(0, applied);
    settle(wet, 0, 4096);

    beat::FractionalDelay dry;
    dry.prepare(48000.0, 1);
    dry.setAppliedDelaySamples(0, applied);
    settle(dry, 0, 4096);

    const float wetPeak = peakIndex(runImpulse(wet, 0, 64));
    const float dryPeak = peakIndex(runImpulse(dry, 0, 64));
    REQUIRE_THAT(wetPeak, WithinAbs(dryPeak, 0.2f));
    REQUIRE(static_cast<float>(latency) + 0.05f >= wetPeak);
}
