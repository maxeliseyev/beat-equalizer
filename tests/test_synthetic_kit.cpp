#include "SyntheticKit.h"
#include "dsp/GccPhat.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

using Catch::Approx;
using namespace beat;
using namespace beat::test;

namespace
{
constexpr double kRate = 48000.0;

// Снейр в четыре микрофона: свой, оверхед, комната; хэт просачивается всюду.
KitSpec snareKit()
{
    KitInstrument snare;
    snare.hitSamples = { 4000, 16000, 28000, 40000 };
    snare.decayPerSecond = 25.0f;
    snare.toneHz = 190.0f;
    snare.noiseMix = 0.6f;
    snare.arrivalSamples = { 0.0f, 3.5f, 240.0f, 960.0f };
    snare.gain = { 1.0f, 0.8f, 0.35f, 0.2f };

    KitInstrument hat;
    hat.hitSamples = { 10000, 22000, 34000 };
    hat.decayPerSecond = 60.0f;
    hat.toneHz = 4000.0f;
    hat.noiseMix = 0.9f;
    hat.arrivalSamples = { 300.0f, 290.0f, 120.0f, 900.0f };
    hat.gain = { 0.05f, 0.05f, 0.5f, 0.15f };

    KitSpec spec;
    spec.sampleRate = kRate;
    spec.numChannels = 4;
    spec.numSamples = 48000;
    spec.instruments = { snare, hat };
    spec.noiseFloor = 0.0005f;
    spec.seed = 2026;
    return spec;
}

int peakIndex(const std::vector<float>& x, int from, int to)
{
    int best = from;
    float top = 0.0f;
    for (int i = from; i < to && i < static_cast<int>(x.size()); ++i)
    {
        const float value = std::abs(x[static_cast<size_t>(i)]);
        if (value > top)
        {
            top = value;
            best = i;
        }
    }
    return best;
}
} // namespace

TEST_CASE("synthetic take puts hits where the spec says")
{
    const auto channels = renderKit(snareKit());
    REQUIRE(channels.size() == 4);
    REQUIRE(channels[0].size() == 48000);

    // Близкий микрофон: пик первого удара стоит на своём месте.
    CHECK(peakIndex(channels[0], 3000, 6000) < 4200);
    CHECK(peakIndex(channels[0], 3000, 6000) >= 4000);

    // Комната отстала на 20 мс и тише.
    const int roomPeak = peakIndex(channels[3], 4000, 6500);
    CHECK(roomPeak > 4900);
    CHECK(roomPeak < 5100);
}

TEST_CASE("gcc-phat recovers the arrivals the take was built from")
{
    // Стенд годится под детектор только если задержки из него достаются с той
    // же точностью, что и на реальном материале.
    auto spec = snareKit();
    spec.instruments.resize(1);   // без просачивания: меряем чистый приход
    const auto channels = renderKit(spec);

    GccPhat gcc(13);
    const int frame = 8192;
    const int start = 2048;
    const int maxLag = maxLagSamples(kDefaultMaxDistanceM, kRate);

    const double expected[] = { 0.0, 3.5, 240.0 };
    for (int ch = 1; ch < 3; ++ch)
    {
        const auto result = gcc.estimate(channels[0].data() + start,
                                         channels[static_cast<size_t>(ch)].data() + start,
                                         frame,
                                         maxLag,
                                         kRate);

        REQUIRE(result.valid);
        CHECK_FALSE(result.invert);
        CHECK(static_cast<double>(result.lagSamples) == Approx(expected[ch]).margin(0.1));
    }
}

TEST_CASE("bleed is quieter than the direct sound and arrives on its own schedule")
{
    const auto channels = renderKit(snareKit());

    // Хэт в близком микрофоне снейра — просачивание: тише прямого во много раз.
    const int hatInClose = peakIndex(channels[0], 10000, 12000);
    const int hatInOverhead = peakIndex(channels[2], 10000, 12000);

    const float closeLevel = std::abs(channels[0][static_cast<size_t>(hatInClose)]);
    const float overheadLevel = std::abs(channels[2][static_cast<size_t>(hatInOverhead)]);
    CHECK(closeLevel * 4.0f < overheadLevel);

    // И приходит по своему расписанию: хэт ближе к оверхеду, чем к близкому.
    CHECK(hatInOverhead < hatInClose);
}

TEST_CASE("rendering the same spec twice gives the same samples")
{
    const auto first = renderKit(snareKit());
    const auto second = renderKit(snareKit());

    REQUIRE(first.size() == second.size());
    for (size_t ch = 0; ch < first.size(); ++ch)
        CHECK(first[ch] == second[ch]);
}
