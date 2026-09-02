#include "SyntheticKit.h"
#include "dsp/Envelope.h"
#include "dsp/OnsetAnalysis.h"
#include "dsp/PeakPicker.h"

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

std::vector<float> singleMic(const std::vector<int>& hits, int numSamples = 48000)
{
    KitInstrument drum;
    drum.hitSamples = hits;
    drum.decayPerSecond = 25.0f;
    drum.toneHz = 190.0f;
    drum.noiseMix = 0.6f;
    drum.arrivalSamples = { 0.0f };
    drum.gain = { 1.0f };

    KitSpec spec;
    spec.sampleRate = kRate;
    spec.numChannels = 1;
    spec.numSamples = numSamples;
    spec.instruments = { drum };
    spec.noiseFloor = 0.0005f;
    spec.seed = 77;

    return renderKit(spec)[0];
}
} // namespace

TEST_CASE("window and hop are the same length in time at 48 and 96 kHz")
{
    OnsetAnalysisConfig at48;
    at48.sampleRate = 48000.0;
    OnsetAnalysisConfig at96 = at48;
    at96.sampleRate = 96000.0;

    OnsetAnalysis a(at48);
    OnsetAnalysis b(at96);

    CHECK(b.hopSamples() == 2 * a.hopSamples());
    CHECK(b.windowSamples() == Approx(2.0 * a.windowSamples()).margin(2.0));
}

TEST_CASE("flux rises at the hit and stays flat in the decay")
{
    const auto mic = singleMic({ 8000, 24000, 40000 });
    OnsetAnalysisConfig config;
    config.sampleRate = kRate;
    OnsetAnalysis analysis(config);
    const auto features = analysis.run(mic.data(), static_cast<int>(mic.size()));

    REQUIRE(features.numFrames > 100);
    REQUIRE(features.bands.size() == static_cast<size_t>(kOnsetBands));

    const auto frameOf = [&](int sample)
    { return static_cast<int>(std::lround((sample - features.firstSampleOffset)
                                          / features.hopSamples)); };

    const auto peakNear = [&](int sample, int spread)
    {
        float top = 0.0f;
        const int centre = frameOf(sample);
        for (int i = std::max(0, centre - spread);
             i < std::min(features.numFrames, centre + spread);
             ++i)
            top = std::max(top, features.flux[static_cast<size_t>(i)]);

        return top;
    };

    // Затухание идёт без прироста спектра: поток там обязан быть много ниже.
    const float atHit = peakNear(8000, 8);
    const float inDecay = peakNear(14000, 8);
    CHECK(atHit > 4.0f * inDecay);
}

TEST_CASE("peak picker finds every hit and invents none in the tail")
{
    const auto mic = singleMic({ 6000, 18000, 30000, 42000 });
    OnsetAnalysisConfig config;
    config.sampleRate = kRate;
    OnsetAnalysis analysis(config);
    const auto features = analysis.run(mic.data(), static_cast<int>(mic.size()));

    PeakPickConfig picking;
    picking.sampleRate = kRate;
    picking.hopSamples = features.hopSamples;

    const auto peaks = pickPeaks(features.flux, picking);
    CHECK(peaks.size() == 4);

    for (const auto& peak : peaks)
        CHECK(peak.strength() > 1.0f);
}

TEST_CASE("peak picker keeps the louder of two peaks closer than the minimum interval")
{
    std::vector<float> flux(400, 0.01f);
    flux[100] = 1.0f;
    flux[102] = 0.6f;   // тот же удар, дребезг кадра
    flux[300] = 0.9f;

    PeakPickConfig picking;
    picking.sampleRate = kRate;
    picking.hopSamples = 120;   // 2.5 мс на кадр
    picking.minIntervalMs = 12.0f;

    const auto peaks = pickPeaks(flux, picking);
    REQUIRE(peaks.size() == 2);
    CHECK(peaks[0].frame == 100);
    CHECK(peaks[1].frame == 300);
}

TEST_CASE("silence produces no onsets at all")
{
    std::vector<float> quiet(48000, 0.0f);
    OnsetAnalysisConfig config;
    config.sampleRate = kRate;
    OnsetAnalysis analysis(config);
    const auto features = analysis.run(quiet.data(), static_cast<int>(quiet.size()));

    PeakPickConfig picking;
    picking.sampleRate = kRate;
    picking.hopSamples = features.hopSamples;

    CHECK(pickPeaks(features.flux, picking).empty());
}

TEST_CASE("envelope follower rises fast and falls slowly")
{
    std::vector<float> burst(4800, 0.0f);
    for (int i = 480; i < 960; ++i)
        burst[static_cast<size_t>(i)] = 1.0f;

    const auto envelope = followEnvelope(burst.data(), static_cast<int>(burst.size()), kRate);

    // Атака 2 мс: за 5 мс огибающая обязана добраться почти до единицы.
    CHECK(envelope[480 + 240] > 0.9f);
    // Спад 20 мс: через 5 мс после конца она ещё заметно выше нуля.
    CHECK(envelope[960 + 240] > 0.5f);
    CHECK(envelope[960 + 2400] < 0.2f);
}

TEST_CASE("noise floor is the track floor, not the quietest sample")
{
    std::vector<float> signal(10000, 0.02f);
    for (int i = 5000; i < 5100; ++i)
        signal[static_cast<size_t>(i)] = 1.0f;
    signal[42] = 0.0f;

    const float floorLevel = noiseFloorOf(signal);
    CHECK(floorLevel == Approx(0.02f));
}
