#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "SyntheticKit.h"
#include "dsp/CrossfadeRenderer.h"
#include "dsp/Constants.h"

#include <vector>

using Catch::Approx;
using beat::test::whiteNoise;

namespace
{
constexpr double kSampleRate = 48000.0;

std::vector<const float*> readPointers(const std::vector<std::vector<float>>& channels)
{
    std::vector<const float*> pointers;
    pointers.reserve(channels.size());
    for (const auto& channel : channels)
        pointers.push_back(channel.data());
    return pointers;
}

std::vector<float*> writePointers(std::vector<std::vector<float>>& channels)
{
    std::vector<float*> pointers;
    pointers.reserve(channels.size());
    for (auto& channel : channels)
        pointers.push_back(channel.data());
    return pointers;
}

beat::CrossfadeRenderer::Result render(
    const std::vector<std::vector<float>>& input,
    std::vector<std::vector<float>>& output,
    const std::vector<beat::CrossfadeRenderer::EditPoint>& editPoints,
    beat::CrossfadeRenderer::Options options)
{
    auto in = readPointers(input);
    auto out = writePointers(output);
    beat::CrossfadeRenderer renderer;
    return renderer.render(in.data(), out.data(), options, editPoints);
}

beat::CrossfadeRenderer::EditPoint pointAt(double time)
{
    beat::CrossfadeRenderer::EditPoint point;
    point.timeSamples = time;
    return point;
}
} // namespace

TEST_CASE("crossfade strength zero copies the source bit for bit")
{
    constexpr int samples = 4096;
    std::vector<std::vector<float>> input {
        whiteNoise(samples, 31),
        whiteNoise(samples, 32),
    };
    std::vector<std::vector<float>> output(2, std::vector<float>(samples, 99.0f));

    auto first = pointAt(512.0);
    first.setDelay(0, 0.0f);
    first.setDelay(1, 0.0f);

    auto second = pointAt(2048.0);
    second.joinStartSamples = 1904.0;
    second.joinEndSamples = 2048.0;
    second.setDelay(0, 80.0f);
    second.setDelay(1, 0.0f);

    beat::CrossfadeRenderer::Options options;
    options.sampleRate = kSampleRate;
    options.numChannels = 2;
    options.numSamples = samples;
    options.strength = 0.0f;

    const auto result = render(input, output, { first, second }, options);

    CHECK(result.editPointsUsed == 0);
    CHECK(result.crossfadesRendered == 0);
    CHECK(output == input);
}

TEST_CASE("crossfade renderer uses equal-amplitude weights")
{
    constexpr int samples = 256;
    std::vector<std::vector<float>> input(1, std::vector<float>(samples, 1.0f));
    std::vector<std::vector<float>> output(1, std::vector<float>(samples, 0.0f));

    auto first = pointAt(16.0);
    first.setDelay(0, 0.0f);

    auto second = pointAt(128.0);
    second.joinStartSamples = 80.0;
    second.joinEndSamples = 112.0;
    second.setDelay(0, 0.0f);

    beat::CrossfadeRenderer::Options options;
    options.sampleRate = kSampleRate;
    options.numChannels = 1;
    options.numSamples = samples;

    const auto result = render(input, output, { first, second }, options);

    CHECK(result.crossfadesRendered == 1);
    for (int i = 32; i < samples; ++i)
        CHECK(output[0][static_cast<size_t>(i)] == Approx(1.0f).margin(1.0e-5f));
}

TEST_CASE("crossfade renderer jumps target delay inside the join interval")
{
    constexpr int samples = 256;
    std::vector<std::vector<float>> input(1, std::vector<float>(samples, 0.0f));
    for (int i = 0; i < samples; ++i)
        input[0][static_cast<size_t>(i)] = static_cast<float>(i);

    std::vector<std::vector<float>> output(1, std::vector<float>(samples, 0.0f));

    auto first = pointAt(16.0);
    first.setDelay(0, 0.0f);

    auto second = pointAt(128.0);
    second.joinStartSamples = 100.0;
    second.joinEndSamples = 110.0;
    second.setDelay(0, 10.0f);

    beat::CrossfadeRenderer::Options options;
    options.sampleRate = kSampleRate;
    options.numChannels = 1;
    options.numSamples = samples;

    const auto result = render(input, output, { first, second }, options);

    REQUIRE(result.editPointsUsed == 2);
    CHECK(result.crossfadesRendered == 1);
    CHECK(output[0][99] == Approx(99.0f - beat::kInterpolatorLatencySamples).margin(0.01f));

    const float expectedMiddle =
        0.5f * (105.0f - beat::kInterpolatorLatencySamples)
        + 0.5f * (105.0f - 10.0f - beat::kInterpolatorLatencySamples);
    CHECK(output[0][105] == Approx(expectedMiddle).margin(0.01f));
    CHECK(output[0][110]
          == Approx(110.0f - 10.0f - beat::kInterpolatorLatencySamples).margin(0.01f));
}

TEST_CASE("crossfade renderer keeps base delay for channels missing from an edit point")
{
    constexpr int samples = 512;
    std::vector<std::vector<float>> input(2, std::vector<float>(samples, 0.0f));
    std::vector<std::vector<float>> output(2, std::vector<float>(samples, 0.0f));

    auto first = pointAt(32.0);
    first.setDelay(0, 24.0f);

    beat::CrossfadeRenderer::Options options;
    options.sampleRate = kSampleRate;
    options.numChannels = 2;
    options.numSamples = samples;
    options.baseDelaySamples[1] = 12.0f;

    const auto result = render(input, output, { first }, options);

    CHECK(result.editPointsUsed == 1);
    CHECK(result.finalDelaySamples[0] == Approx(24.0f));
    CHECK(result.finalDelaySamples[1] == Approx(12.0f));
}

TEST_CASE("crossfade renderer skips edit points without a valid join")
{
    constexpr int samples = 512;
    std::vector<std::vector<float>> input(1, std::vector<float>(samples, 0.0f));
    std::vector<std::vector<float>> output(1, std::vector<float>(samples, 0.0f));

    auto first = pointAt(32.0);
    first.setDelay(0, 0.0f);

    auto invalid = pointAt(128.0);
    invalid.joinStartSamples = 160.0;
    invalid.joinEndSamples = 160.0;
    invalid.setDelay(0, 24.0f);

    beat::CrossfadeRenderer::Options options;
    options.sampleRate = kSampleRate;
    options.numChannels = 1;
    options.numSamples = samples;

    const auto result = render(input, output, { first, invalid }, options);

    CHECK(result.editPointsUsed == 1);
    CHECK(result.skippedEditPoints == 1);
    CHECK(result.crossfadesRendered == 0);
    CHECK(result.finalDelaySamples[0] == Approx(0.0f));
}
