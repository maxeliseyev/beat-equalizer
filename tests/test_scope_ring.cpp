#include <catch2/catch_test_macros.hpp>

#include "plugin/ScopeRing.h"

TEST_CASE("scope ring copies the last N samples in time order")
{
    beat::ScopeRing ring;
    float frame[2] {};

    for (int i = 0; i < 10; ++i)
    {
        frame[0] = (float) i;
        frame[1] = (float) i * 0.5f;
        ring.push(2, frame);
    }

    float out[4] {};
    ring.copyLast(0, out, 4);
    REQUIRE(out[0] == 6.0f);
    REQUIRE(out[1] == 7.0f);
    REQUIRE(out[2] == 8.0f);
    REQUIRE(out[3] == 9.0f);
}

TEST_CASE("rising trigger finds the last crossing")
{
    float x[8] = { 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.3f, 0.4f, 0.1f };
    REQUIRE(beat::ScopeRing::findRisingTrigger(x, 8, 0.12f) == 5);
    REQUIRE(beat::ScopeRing::findRisingTrigger(x, 8, 0.9f) == -1);
}
