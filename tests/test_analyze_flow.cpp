#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "SyntheticKit.h"
#include "dsp/AlignmentEngine.h"
#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 512;
constexpr int kDelaySamples = 24; // 0.5 ms: ch1 звучит позже опоры

// Гоняем через processBlock ту же пару, что и в DSP-тестах: ch1 позже ch0.
void feedKit(BeatEqualizerAudioProcessor& processor, int totalSamples)
{
    const auto source = beat::test::whiteNoise(totalSamples + kDelaySamples, 17);
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;

    for (int start = 0; start + kBlock <= totalSamples; start += kBlock)
    {
        for (int n = 0; n < kBlock; ++n)
        {
            const int i = start + n + kDelaySamples;
            buffer.setSample(0, n, source[static_cast<size_t>(i)]);
            buffer.setSample(1, n, source[static_cast<size_t>(i - kDelaySamples)]);
        }

        processor.processBlock(buffer, midi);
    }
}

beat::AlignmentEngine::Result analyzeRing(const BeatEqualizerAudioProcessor& processor,
                                          beat::AlignmentEngine& engine)
{
    const auto& ring = processor.getAnalysisRing();
    const int window = ring.length();
    std::vector<float> scratch(static_cast<size_t>(2 * window), 0.0f);
    const int available = ring.readLast(scratch.data(), 2, window);

    const float* pointers[2] { scratch.data(), scratch.data() + window };

    beat::AnalysisRequest request;
    request.sampleRate = kSampleRate;
    return engine.analyze(pointers, 2, available, request);
}

float paramValue(BeatEqualizerAudioProcessor& processor, int channel, const juce::String& suffix)
{
    return processor.getParameters().getRawParameterValue(beat::channelParamId(channel, suffix))
        ->load();
}

// Один и тот же синус через плагин: возвращает последний блок канала 0.
std::vector<float> renderWithRotator(float amount, bool abBypass)
{
    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, kBlock);

    auto set = [&processor](const juce::String& id, float value)
    {
        auto* parameter = processor.getParameters().getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };

    set(beat::channelParamId(0, "rotatorHz"), 900.0f);
    set(beat::channelParamId(0, "rotatorAmount"), amount);
    set("global.abBypass", abBypass ? 1.0f : 0.0f);

    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    std::vector<float> last(static_cast<size_t>(kBlock), 0.0f);

    for (int block = 0; block < 16; ++block)
    {
        for (int n = 0; n < kBlock; ++n)
        {
            const float t = static_cast<float>(block * kBlock + n);
            const float x = std::sin(0.09f * t);
            buffer.setSample(0, n, x);
            buffer.setSample(1, n, x);
        }

        processor.processBlock(buffer, midi);
        std::copy_n(buffer.getReadPointer(0), kBlock, last.begin());
    }

    return last;
}

float maxDifference(const std::vector<float>& a, const std::vector<float>& b)
{
    float worst = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
        worst = std::max(worst, std::abs(a[i] - b[i]));
    return worst;
}

float delayParam(BeatEqualizerAudioProcessor& processor, int channel)
{
    return processor.getParameters().getRawParameterValue(beat::channelParamId(channel, "delayMs"))
        ->load();
}
} // namespace

TEST_CASE("analysis ring keeps the raw input, so Analyze finds the real offset")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, kBlock);
    feedKit(processor, 3 * 48000 / 2);

    beat::AlignmentEngine engine;
    const auto result = analyzeRing(processor, engine);

    REQUIRE(result.status == beat::AnalysisStatus::ok);
    REQUIRE(result.channels[1].valid);
    REQUIRE_THAT(result.channels[1].tdoaSamples,
                 WithinAbs(static_cast<float>(kDelaySamples), 0.1f));

    processor.applyAnalysisResult(result);

    // ch1 звучит позже опоры, значит задерживаем опору, а ch1 оставляем.
    REQUIRE_THAT(delayParam(processor, 0), WithinAbs(0.5f, 0.01f));
    REQUIRE_THAT(delayParam(processor, 1), WithinAbs(0.0f, 0.01f));
    REQUIRE(processor.getAnalysisStatus().contains("aligned"));

    // Повторный анализ уже выровненного материала не должен «уползать»:
    // буфер хранит вход, а не выход.
    feedKit(processor, 3 * 48000 / 2);
    const auto again = analyzeRing(processor, engine);
    REQUIRE_THAT(again.channels[1].tdoaSamples,
                 WithinAbs(static_cast<float>(kDelaySamples), 0.1f));
}

TEST_CASE("Freeze keeps manual edits: analysis results are not applied")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, kBlock);
    feedKit(processor, 3 * 48000 / 2);

    beat::AlignmentEngine engine;
    const auto result = analyzeRing(processor, engine);
    REQUIRE(result.status == beat::AnalysisStatus::ok);

    auto* freeze = processor.getParameters().getParameter("global.freeze");
    REQUIRE(freeze != nullptr);
    freeze->setValueNotifyingHost(1.0f);
    REQUIRE(processor.isFrozen());

    auto* manual = processor.getParameters().getParameter(beat::channelParamId(1, "delayMs"));
    manual->setValueNotifyingHost(manual->convertTo0to1(3.0f));

    processor.applyAnalysisResult(result);

    REQUIRE_THAT(delayParam(processor, 1), WithinAbs(3.0f, 0.01f));
    REQUIRE(processor.getAnalysisStatus().contains("Frozen"));
}

TEST_CASE("silence reports too quiet instead of aligning to noise")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, kBlock);

    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    for (int block = 0; block < 64; ++block)
    {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }

    beat::AlignmentEngine engine;
    const auto result = analyzeRing(processor, engine);

    REQUIRE(result.status == beat::AnalysisStatus::tooQuiet);

    processor.applyAnalysisResult(result);
    REQUIRE(processor.getAnalysisStatus().contains("Too quiet"));
    REQUIRE_THAT(delayParam(processor, 1), WithinAbs(0.0f, 0.01f));
}

TEST_CASE("Analyze records the coherence it gained")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, kBlock);
    feedKit(processor, 3 * 48000 / 2);

    beat::AlignmentEngine engine;
    const auto result = analyzeRing(processor, engine);
    processor.applyAnalysisResult(result);

    REQUIRE(processor.getCoherenceAfter() > 0.9f);
    REQUIRE(processor.getCoherenceAfter() >= processor.getCoherenceBefore());
    REQUIRE(paramValue(processor, 1, "rotatorHz") > 0.0f);
}

TEST_CASE("rotator parameter reaches the audio path and A/B skips it")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto dry = renderWithRotator(0.0f, false);
    const auto wet = renderWithRotator(1.0f, false);
    const auto bypassed = renderWithRotator(1.0f, true);

    REQUIRE(maxDifference(dry, wet) > 0.1f);
    REQUIRE(maxDifference(dry, bypassed) < 1.0e-5f);
}
