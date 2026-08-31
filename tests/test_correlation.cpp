#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "SyntheticKit.h"
#include "dsp/Correlation.h"

#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;
using beat::test::delaySignal;
using beat::test::whiteNoise;

TEST_CASE("correlation is +1 for a copy and -1 for an inverted copy")
{
    const auto x = whiteNoise(4096, 2);
    std::vector<float> flipped(x.size());
    for (size_t i = 0; i < x.size(); ++i)
        flipped[i] = -x[i];

    REQUIRE_THAT(beat::correlation(x.data(), x.data(), 4096), WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(beat::correlation(x.data(), flipped.data(), 4096), WithinAbs(-1.0f, 1.0e-5f));
}

TEST_CASE("independent channels sit near zero")
{
    const auto a = whiteNoise(4096, 3);
    const auto b = whiteNoise(4096, 4);
    REQUIRE_THAT(beat::correlation(a.data(), b.data(), 4096), WithinAbs(0.0f, 0.05f));
}

TEST_CASE("a quarter-period shift kills the correlation of a sine")
{
    std::vector<float> sine(4096);
    std::vector<float> quadrature(4096);
    for (int i = 0; i < 4096; ++i)
    {
        const double w = 2.0 * beat::test::kPi * 64.0 * i / 4096.0;
        sine[static_cast<size_t>(i)] = static_cast<float>(std::sin(w));
        quadrature[static_cast<size_t>(i)] = static_cast<float>(std::cos(w));
    }

    REQUIRE_THAT(beat::correlation(sine.data(), quadrature.data(), 4096), WithinAbs(0.0f, 0.01f));
}

TEST_CASE("a delayed drum mic loses correlation, alignment brings it back")
{
    const auto x = whiteNoise(4096, 7);
    const auto later = delaySignal(x, 24.0f);

    const float raw = beat::correlation(x.data(), later.data(), 4000);
    const float aligned = beat::correlation(x.data(), later.data() + 24, 4000);

    REQUIRE(raw < 0.3f);
    REQUIRE(aligned > 0.95f);
}

TEST_CASE("striding keeps the same answer on a broadband pair")
{
    const auto x = whiteNoise(4096, 11);
    const auto y = delaySignal(x, 3.0f);

    const float full = beat::correlation(x.data(), y.data(), 4096);
    const float strided = beat::correlation(x.data(), y.data(), 4096, 4);
    REQUIRE_THAT(strided, WithinAbs(full, 0.05f));
}

TEST_CASE("silence and bad input report zero, not NaN")
{
    const std::vector<float> silence(1024, 0.0f);
    const auto x = whiteNoise(1024, 13);

    REQUIRE(beat::correlation(silence.data(), x.data(), 1024) == 0.0f);
    REQUIRE(beat::correlation(nullptr, x.data(), 1024) == 0.0f);
    REQUIRE(beat::correlation(x.data(), x.data(), 0) == 0.0f);
}
