#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "SyntheticKit.h"
#include "dsp/GlideRenderer.h"

#include <algorithm>
#include <vector>

using Catch::Approx;
using Catch::Matchers::WithinAbs;
using beat::test::delaySignal;
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

beat::GlideRenderer::Result render(const std::vector<std::vector<float>>& input,
                                   std::vector<std::vector<float>>& output,
                                   const std::vector<beat::GlideRenderer::EventDelay>& events,
                                   beat::GlideRenderer::Options options)
{
    auto in = readPointers(input);
    auto out = writePointers(output);
    beat::GlideRenderer renderer;
    return renderer.render(in.data(), out.data(), options, events);
}
} // namespace

TEST_CASE("glide strength zero copies the source bit for bit")
{
    constexpr int samples = 4096;
    std::vector<std::vector<float>> input {
        whiteNoise(samples, 1),
        whiteNoise(samples, 2),
    };
    std::vector<std::vector<float>> output(2, std::vector<float>(samples, 99.0f));

    beat::GlideRenderer::EventDelay event;
    event.timeSamples = 1024.0;
    event.referenceChannel = 0;
    event.setDelay(0, 240.0f);
    event.setDelay(1, 0.0f);

    beat::GlideRenderer::Options options;
    options.sampleRate = kSampleRate;
    options.numChannels = 2;
    options.numSamples = samples;
    options.strength = 0.0f;

    const auto result = render(input, output, { event }, options);

    CHECK(result.eventsMeasured == 1);
    CHECK(result.limitedEvents == 0);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < samples; ++i)
            REQUIRE(output[static_cast<size_t>(ch)][static_cast<size_t>(i)]
                    == input[static_cast<size_t>(ch)][static_cast<size_t>(i)]);
}

TEST_CASE("glide delay cannot move faster than the slew limit")
{
    constexpr int samples = 4096;
    std::vector<std::vector<float>> input(2, std::vector<float>(samples, 0.0f));
    std::vector<std::vector<float>> output(2, std::vector<float>(samples, 0.0f));

    beat::GlideRenderer::EventDelay first;
    first.timeSamples = 1000.0;
    first.protectUntilSamples = 1100.0;
    first.setDelay(0, 0.0f);
    first.setDelay(1, 0.0f);

    beat::GlideRenderer::EventDelay second;
    second.timeSamples = 2000.0;
    second.protectUntilSamples = 2100.0;
    second.setDelay(0, 100.0f);
    second.setDelay(1, 0.0f);

    beat::GlideRenderer::Options options;
    options.sampleRate = kSampleRate;
    options.numChannels = 2;
    options.numSamples = samples;
    options.maxSlew = 0.01f;

    const auto result = render(input, output, { first, second }, options);

    REQUIRE(result.events.size() == 2);
    CHECK(result.limitedEvents == 1);
    CHECK(result.events[1].limited);
    CHECK(result.events[1].actualDelaySamples[0] == Approx(8.99f).margin(0.05f));
    CHECK(result.events[1].targetDelaySamples[0] == Approx(100.0f));
    CHECK(result.maxSlewObserved <= options.maxSlew + 1.0e-6f);
}

TEST_CASE("glide reaches a target delay when the decay interval is long enough")
{
    constexpr int samples = 16000;
    std::vector<std::vector<float>> input(2, std::vector<float>(samples, 0.0f));
    std::vector<std::vector<float>> output(2, std::vector<float>(samples, 0.0f));

    beat::GlideRenderer::EventDelay first;
    first.timeSamples = 1000.0;
    first.protectUntilSamples = 1100.0;
    first.setDelay(0, 0.0f);
    first.setDelay(1, 0.0f);

    beat::GlideRenderer::EventDelay second;
    second.timeSamples = 12000.0;
    second.protectUntilSamples = 12100.0;
    second.setDelay(0, 100.0f);
    second.setDelay(1, 0.0f);

    beat::GlideRenderer::Options options;
    options.sampleRate = kSampleRate;
    options.numChannels = 2;
    options.numSamples = samples;
    options.maxSlew = 0.01f;

    const auto result = render(input, output, { first, second }, options);

    REQUIRE(result.events.size() == 2);
    CHECK(result.limitedEvents == 0);
    CHECK_FALSE(result.events[1].limited);
    CHECK(result.events[1].actualDelaySamples[0] == Approx(100.0f).margin(0.05f));
}

TEST_CASE("channels missing from an event keep the base delay")
{
    constexpr int samples = 4096;
    std::vector<std::vector<float>> input(2, std::vector<float>(samples, 0.0f));
    std::vector<std::vector<float>> output(2, std::vector<float>(samples, 0.0f));

    beat::GlideRenderer::EventDelay event;
    event.timeSamples = 1000.0;
    event.setDelay(0, 40.0f);

    beat::GlideRenderer::Options options;
    options.sampleRate = kSampleRate;
    options.numChannels = 2;
    options.numSamples = samples;
    options.baseDelaySamples[1] = 12.0f;

    const auto result = render(input, output, { event }, options);

    REQUIRE(result.events.size() == 1);
    CHECK(result.events[0].targetDelaySamples[0] == Approx(40.0f));
    CHECK(result.events[0].targetDelaySamples[1] == Approx(12.0f));
    CHECK(result.events[0].channelsMeasured == 0);
}

TEST_CASE("event coherence improves when glide delays align a later mic")
{
    constexpr int samples = 16384;
    auto reference = whiteNoise(samples, 7);
    auto later = delaySignal(reference, 24.0f);

    std::vector<std::vector<float>> input {
        reference,
        later,
    };
    std::vector<std::vector<float>> output(2, std::vector<float>(samples, 0.0f));

    beat::GlideRenderer::EventDelay event;
    event.timeSamples = 4096.0;
    event.protectUntilSamples = 4200.0;
    event.referenceChannel = 0;
    event.setDelay(0, 24.0f);
    event.setDelay(1, 0.0f);

    beat::GlideRenderer::Options options;
    options.sampleRate = kSampleRate;
    options.numChannels = 2;
    options.numSamples = samples;

    const auto result = render(input, output, { event }, options);

    REQUIRE(result.events.size() == 1);
    const auto& metric = result.events.front();
    CHECK(metric.channelsMeasured == 1);
    CHECK(metric.coherenceBefore < 0.85f);
    CHECK(metric.coherenceAfter > 0.95f);
    CHECK(metric.coherenceAfter > metric.coherenceBefore + 0.15f);
    CHECK_THAT(metric.actualDelaySamples[0] - metric.actualDelaySamples[1],
               WithinAbs(24.0f, 0.01f));
}
