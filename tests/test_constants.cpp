#include <catch2/catch_test_macros.hpp>

#include "dsp/Constants.h"

TEST_CASE("max lag at default 4 m and 48 kHz stays inside a kick period")
{
    const int lag = beat::maxLagSamples(4.0f, 48000.0);
    REQUIRE(lag == 560);
    REQUIRE(lag < static_cast<int>(0.020 * 48000.0));
}

TEST_CASE("max lag scales with sample rate")
{
    const int lag48 = beat::maxLagSamples(4.0f, 48000.0);
    const int lag96 = beat::maxLagSamples(4.0f, 96000.0);
    REQUIRE(lag96 == lag48 * 2);
}

TEST_CASE("non-positive inputs yield zero lag")
{
    REQUIRE(beat::maxLagSamples(0.0f, 48000.0) == 0);
    REQUIRE(beat::maxLagSamples(4.0f, 0.0) == 0);
    REQUIRE(beat::maxLagSeconds(-1.0f) == 0.0f);
}

TEST_CASE("delay line reaches the furthest distance the search window allows")
{
    const float furthestMs = 1000.0f * beat::maxLagSeconds(beat::kMaxDistanceM);
    REQUIRE(beat::kMaxDelayMs >= furthestMs);
}
