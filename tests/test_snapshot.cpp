#include <catch2/catch_test_macros.hpp>

#include "dsp/AlignmentSnapshot.h"

TEST_CASE("identity snapshot enables only live channels and zeros delays")
{
    const auto snapshot = beat::AlignmentSnapshot::identity(8);

    REQUIRE(snapshot.numChannels == 8);
    REQUIRE(snapshot.reference == 0);
    REQUIRE(snapshot.latencySamples == 0);

    for (int i = 0; i < beat::kMaxChannels; ++i)
    {
        REQUIRE(snapshot.delaySamples[i] == 0.0f);
        REQUIRE_FALSE(snapshot.invert[i]);
        REQUIRE(snapshot.rotatorCoeff[i] == 0.0f);
        REQUIRE(snapshot.rotatorAmount[i] == 0.0f);
        REQUIRE(snapshot.enabled[i] == (i < 8));
    }
}

TEST_CASE("identity snapshot clamps channel count to kMaxChannels")
{
    const auto snapshot = beat::AlignmentSnapshot::identity(128);
    REQUIRE(snapshot.numChannels == beat::kMaxChannels);
    REQUIRE(snapshot.enabled[beat::kMaxChannels - 1]);
}
