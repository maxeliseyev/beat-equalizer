#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/AnalysisState.h"

#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
beat::AlignmentEngine::Result makeResult()
{
    beat::AlignmentEngine::Result result;
    result.status = beat::AnalysisStatus::ok;
    result.numChannels = 3;
    result.reference = 1;
    result.coherenceBefore = 0.61f;
    result.coherenceAfter = 0.93f;

    for (int ch = 0; ch < 3; ++ch)
    {
        auto& estimate = result.channels[static_cast<size_t>(ch)];
        estimate.tdoaSamples = 1.5f * static_cast<float>(ch) - 2.0f;
        estimate.confidence = 12.0f + static_cast<float>(ch);
        estimate.coherenceBefore = 0.5f;
        estimate.coherenceAfter = 0.9f;
        estimate.rotatorHz = 300.0f + 100.0f * static_cast<float>(ch);
        estimate.rotatorAmount = 0.25f * static_cast<float>(ch);
        estimate.framesUsed = 7 + ch;
        estimate.invert = ch == 2;
        estimate.valid = ch != 1 || true;
    }

    return result;
}
} // namespace

TEST_CASE("analysis state survives a round trip")
{
    const auto original = makeResult();
    const auto blob = beat::serializeAnalysis(original, 48000.0);

    beat::AlignmentEngine::Result restored;
    double sampleRate = 0.0;
    REQUIRE(beat::deserializeAnalysis(blob.data(), blob.size(), 3, restored, sampleRate));

    REQUIRE(sampleRate == 48000.0);
    REQUIRE(restored.numChannels == 3);
    REQUIRE(restored.reference == 1);
    REQUIRE_THAT(restored.coherenceAfter, WithinAbs(0.93f, 1.0e-6f));

    for (int ch = 0; ch < 3; ++ch)
    {
        const auto& a = original.channels[static_cast<size_t>(ch)];
        const auto& b = restored.channels[static_cast<size_t>(ch)];
        REQUIRE_THAT(b.tdoaSamples, WithinAbs(a.tdoaSamples, 1.0e-6f));
        REQUIRE_THAT(b.rotatorHz, WithinAbs(a.rotatorHz, 1.0e-6f));
        REQUIRE_THAT(b.rotatorAmount, WithinAbs(a.rotatorAmount, 1.0e-6f));
        REQUIRE(b.framesUsed == a.framesUsed);
        REQUIRE(b.invert == a.invert);
        REQUIRE(b.valid == a.valid);
    }
}

TEST_CASE("a blob for another channel count is refused, not stretched")
{
    const auto blob = beat::serializeAnalysis(makeResult(), 48000.0);

    beat::AlignmentEngine::Result restored;
    double sampleRate = 0.0;
    REQUIRE_FALSE(beat::deserializeAnalysis(blob.data(), blob.size(), 8, restored, sampleRate));
    REQUIRE(restored.status == beat::AnalysisStatus::idle);
}

TEST_CASE("a foreign version and a truncated blob are refused")
{
    auto blob = beat::serializeAnalysis(makeResult(), 48000.0);

    beat::AlignmentEngine::Result restored;
    double sampleRate = 0.0;

    auto wrongVersion = blob;
    wrongVersion[0] = static_cast<std::uint8_t>(beat::kAnalysisStateVersion + 7);
    REQUIRE_FALSE(
        beat::deserializeAnalysis(wrongVersion.data(), wrongVersion.size(), 3, restored, sampleRate));

    blob.resize(blob.size() / 2);
    REQUIRE_FALSE(beat::deserializeAnalysis(blob.data(), blob.size(), 3, restored, sampleRate));
    REQUIRE_FALSE(beat::deserializeAnalysis(nullptr, 0, 3, restored, sampleRate));
}
