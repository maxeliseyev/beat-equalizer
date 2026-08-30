#include <catch2/catch_test_macros.hpp>

#include "dsp/LatencyModel.h"

TEST_CASE("applied delays are non-negative and latency includes interpolator")
{
    const float tdoa[3] = { 0.0f, 5.5f, 2.0f };
    const bool enabled[3] = { true, true, true };
    float applied[3] {};
    int latency = 0;

    beat::LatencyModel::applyTdoa(tdoa, enabled, 3, applied, latency);

    REQUIRE(applied[0] == 5.5f);
    REQUIRE(applied[1] == 0.0f);
    REQUIRE(applied[2] == 3.5f);
    REQUIRE(latency == 6 + beat::kInterpolatorLatencySamples);
}

TEST_CASE("negative TDOA does not produce negative applied delay")
{
    const float tdoa[2] = { -3.0f, 1.0f };
    const bool enabled[2] = { true, true };
    float applied[2] {};
    int latency = 0;

    beat::LatencyModel::applyTdoa(tdoa, enabled, 2, applied, latency);

    REQUIRE(applied[0] >= 0.0f);
    REQUIRE(applied[1] >= 0.0f);
    REQUIRE(applied[0] == 4.0f);
    REQUIRE(applied[1] == 0.0f);
}

TEST_CASE("disabled channels do not set the max TDOA")
{
    const float tdoa[2] = { 0.0f, 12.0f };
    const bool enabled[2] = { true, false };
    float applied[2] {};
    int latency = 0;

    beat::LatencyModel::applyTdoa(tdoa, enabled, 2, applied, latency);

    REQUIRE(applied[0] == 0.0f);
    REQUIRE(applied[1] == 0.0f);
    REQUIRE(latency == beat::kInterpolatorLatencySamples);
}

TEST_CASE("reported latency is ceil(max applied) plus interpolator")
{
    REQUIRE(beat::LatencyModel::reportedLatency(0.0f) == beat::kInterpolatorLatencySamples);
    REQUIRE(beat::LatencyModel::reportedLatency(5.1f) == 6 + beat::kInterpolatorLatencySamples);
}
