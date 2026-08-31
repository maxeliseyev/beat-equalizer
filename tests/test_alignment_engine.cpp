#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "SyntheticKit.h"
#include "dsp/AlignmentEngine.h"
#include "dsp/Constants.h"
#include "dsp/LatencyModel.h"

#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;
using beat::test::delaySignal;
using beat::test::impulseTrain;
using beat::test::whiteNoise;

namespace
{
struct Kit
{
    std::vector<std::vector<float>> channels;
    std::vector<const float*> pointers;

    void add(std::vector<float> channel) { channels.push_back(std::move(channel)); }

    const float* const* data()
    {
        pointers.clear();
        for (const auto& channel : channels)
            pointers.push_back(channel.data());
        return pointers.data();
    }

    int numSamples() const { return static_cast<int>(channels.front().size()); }
    int numChannels() const { return static_cast<int>(channels.size()); }
};

constexpr double kSampleRate = 48000.0;
constexpr int kLength = 48000; // 1 s: несколько кадров по 8192 с hop 50 %
} // namespace

TEST_CASE("engine finds per-channel delay and polarity on a synthetic kit")
{
    const auto reference = whiteNoise(kLength, 3);

    Kit kit;
    kit.add(reference);
    kit.add(delaySignal(reference, 5.5f));
    kit.add(delaySignal(reference, 11.3f, true));

    beat::AlignmentEngine engine;
    beat::AnalysisRequest request;
    request.sampleRate = kSampleRate;
    request.reference = 0;

    const auto result = engine.analyze(kit.data(), kit.numChannels(), kit.numSamples(), request);

    REQUIRE(result.status == beat::AnalysisStatus::ok);
    REQUIRE(result.framesLoud > 0);

    REQUIRE(result.channels[0].valid);
    REQUIRE(result.channels[0].tdoaSamples == 0.0f);

    REQUIRE(result.channels[1].valid);
    REQUIRE(result.channels[1].framesUsed >= beat::kAnalysisMinFrames);
    REQUIRE_THAT(result.channels[1].tdoaSamples, WithinAbs(5.5f, 0.1f));
    REQUIRE_FALSE(result.channels[1].invert);

    REQUIRE(result.channels[2].valid);
    REQUIRE_THAT(result.channels[2].tdoaSamples, WithinAbs(11.3f, 0.1f));
    REQUIRE(result.channels[2].invert);
}

TEST_CASE("engine snapshot delays the earliest mic, never asks for a negative delay")
{
    const auto reference = whiteNoise(kLength, 5);

    Kit kit;
    kit.add(reference);
    kit.add(delaySignal(reference, 5.5f));
    kit.add(delaySignal(reference, 11.3f));

    beat::AlignmentEngine engine;
    beat::AnalysisRequest request;
    request.sampleRate = kSampleRate;

    const auto result = engine.analyze(kit.data(), kit.numChannels(), kit.numSamples(), request);
    const auto& snapshot = result.snapshot;

    REQUIRE(result.status == beat::AnalysisStatus::ok);
    REQUIRE(snapshot.numChannels == 3);

    for (int ch = 0; ch < snapshot.numChannels; ++ch)
        REQUIRE(snapshot.delaySamples[ch] >= 0.0f);

    // Опора звучит раньше всех, поэтому её задерживаем на max(TDOA).
    REQUIRE_THAT(snapshot.delaySamples[0], WithinAbs(11.3f, 0.1f));
    REQUIRE_THAT(snapshot.delaySamples[1], WithinAbs(11.3f - 5.5f, 0.1f));
    REQUIRE_THAT(snapshot.delaySamples[2], WithinAbs(0.0f, 0.1f));

    REQUIRE(snapshot.latencySamples
            == beat::LatencyModel::reportedLatency(result.channels[2].tdoaSamples));
}

TEST_CASE("engine reference choice moves the zero, not the relative alignment")
{
    const auto source = whiteNoise(kLength, 8);

    Kit kit;
    kit.add(source);
    kit.add(delaySignal(source, 5.5f));
    kit.add(delaySignal(source, 11.3f));

    beat::AlignmentEngine engine;
    beat::AnalysisRequest request;
    request.sampleRate = kSampleRate;
    request.reference = 2;

    const auto result = engine.analyze(kit.data(), kit.numChannels(), kit.numSamples(), request);

    REQUIRE(result.status == beat::AnalysisStatus::ok);
    REQUIRE(result.reference == 2);
    REQUIRE_THAT(result.channels[0].tdoaSamples, WithinAbs(-11.3f, 0.1f));
    REQUIRE_THAT(result.channels[1].tdoaSamples, WithinAbs(-5.8f, 0.1f));
    REQUIRE_THAT(result.snapshot.delaySamples[2], WithinAbs(0.0f, 0.1f));
    REQUIRE_THAT(result.snapshot.delaySamples[0], WithinAbs(11.3f, 0.1f));
}

TEST_CASE("engine does not lock onto a 20 ms period outside the search window")
{
    const auto reference = impulseTrain(kLength, 960, 100); // период 20 мс
    Kit kit;
    kit.add(reference);
    kit.add(delaySignal(reference, 5.0f));

    beat::AlignmentEngine engine;
    beat::AnalysisRequest request;
    request.sampleRate = kSampleRate;

    const auto result = engine.analyze(kit.data(), kit.numChannels(), kit.numSamples(), request);

    REQUIRE(result.status == beat::AnalysisStatus::ok);
    REQUIRE(result.channels[1].valid);
    REQUIRE_THAT(result.channels[1].tdoaSamples, WithinAbs(5.0f, 0.5f));
}

TEST_CASE("engine says the buffer is too quiet instead of guessing")
{
    Kit kit;
    kit.add(std::vector<float>(kLength, 0.0f));
    kit.add(std::vector<float>(kLength, 0.0f));

    beat::AlignmentEngine engine;
    beat::AnalysisRequest request;
    request.sampleRate = kSampleRate;

    const auto result = engine.analyze(kit.data(), kit.numChannels(), kit.numSamples(), request);

    REQUIRE(result.status == beat::AnalysisStatus::tooQuiet);
    REQUIRE(result.framesTotal > 0);
    REQUIRE(result.framesLoud == 0);
}

TEST_CASE("engine refuses a buffer shorter than one frame")
{
    const auto reference = whiteNoise(1024, 9);
    Kit kit;
    kit.add(reference);
    kit.add(delaySignal(reference, 2.0f));

    beat::AlignmentEngine engine;
    beat::AnalysisRequest request;
    request.sampleRate = kSampleRate;

    const auto result = engine.analyze(kit.data(), kit.numChannels(), kit.numSamples(), request);
    REQUIRE(result.status == beat::AnalysisStatus::notEnoughData);
}

TEST_CASE("engine keeps a silent channel moving with the reference")
{
    const auto reference = whiteNoise(kLength, 13);

    Kit kit;
    kit.add(reference);
    kit.add(delaySignal(reference, 7.0f));
    kit.add(std::vector<float>(kLength, 0.0f));

    beat::AlignmentEngine engine;
    beat::AnalysisRequest request;
    request.sampleRate = kSampleRate;

    const auto result = engine.analyze(kit.data(), kit.numChannels(), kit.numSamples(), request);

    REQUIRE(result.status == beat::AnalysisStatus::ok);
    REQUIRE_FALSE(result.channels[2].valid);
    REQUIRE(result.channels[2].tdoaSamples == 0.0f);
    REQUIRE(result.snapshot.delaySamples[2] == result.snapshot.delaySamples[0]);
}

TEST_CASE("engine delay in milliseconds does not depend on the sample rate")
{
    beat::AlignmentEngine engine;

    auto tdoaMsAt = [&engine](double sampleRate)
    {
        const int length = static_cast<int>(sampleRate);
        const auto reference = whiteNoise(length, 21);
        const float delaySamples = static_cast<float>(0.125e-3 * sampleRate); // 0.125 ms

        Kit kit;
        kit.add(reference);
        kit.add(delaySignal(reference, delaySamples));

        beat::AnalysisRequest request;
        request.sampleRate = sampleRate;

        const auto result = engine.analyze(kit.data(), kit.numChannels(), kit.numSamples(), request);
        REQUIRE(result.status == beat::AnalysisStatus::ok);
        return 1000.0f * result.channels[1].tdoaSamples / static_cast<float>(sampleRate);
    };

    REQUIRE_THAT(tdoaMsAt(48000.0), WithinAbs(0.125f, 0.005f));
    REQUIRE_THAT(tdoaMsAt(96000.0), WithinAbs(0.125f, 0.005f));
}
