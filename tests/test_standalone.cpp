#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "SyntheticKit.h"
#include "dsp/AlignmentEngine.h"
#include "plugin/Exporter.h"
#include "plugin/FilePlayer.h"
#include "plugin/PluginProcessor.h"

#include <memory>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
constexpr double kSampleRate = 48000.0;

juce::AudioBuffer<float> impulsePair(int length, int firstAt, int secondAt)
{
    juce::AudioBuffer<float> buffer(2, length);
    buffer.clear();
    buffer.setSample(0, firstAt, 1.0f);
    buffer.setSample(1, secondAt, 1.0f);
    return buffer;
}

int peakIndex(const juce::AudioBuffer<float>& buffer, int channel)
{
    int best = 0;
    float peak = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float value = std::abs(buffer.getSample(channel, i));
        if (value > peak)
        {
            peak = value;
            best = i;
        }
    }
    return best;
}

juce::File writeTempWav(const juce::AudioBuffer<float>& buffer)
{
    auto file = juce::File::createTempFile("wav");
    REQUIRE(beat::exporter::writeWav(file, buffer, kSampleRate));
    return file;
}
} // namespace

TEST_CASE("offline render lines up two mics from the first sample")
{
    const auto source = impulsePair(2048, 100, 124);

    std::vector<beat::exporter::ChannelSettings> settings(2);
    settings[0].delaySamples = 24.0f; // первый микрофон ждёт второй

    juce::AudioBuffer<float> rendered;
    beat::exporter::renderAligned(source, kSampleRate, settings, rendered);

    REQUIRE(rendered.getNumChannels() == 2);
    REQUIRE(rendered.getNumSamples() > source.getNumSamples());
    REQUIRE(peakIndex(rendered, 0) == peakIndex(rendered, 1));
}

TEST_CASE("offline render applies polarity and keeps the tail")
{
    const auto source = impulsePair(1024, 50, 50);

    std::vector<beat::exporter::ChannelSettings> settings(2);
    settings[1].invert = true;
    settings[1].delaySamples = 10.0f;

    juce::AudioBuffer<float> rendered;
    beat::exporter::renderAligned(source, kSampleRate, settings, rendered);

    REQUIRE(rendered.getSample(0, peakIndex(rendered, 0)) > 0.9f);
    REQUIRE(rendered.getSample(1, peakIndex(rendered, 1)) < -0.9f);
    REQUIRE(rendered.getNumSamples() == 1024 + 10 + beat::kInterpolatorLatencySamples);
}

TEST_CASE("a written wav loads back with the same shape")
{
    const auto source = impulsePair(4096, 200, 260);
    const auto file = writeTempWav(source);

    FilePlayer player;
    REQUIRE(player.load({ file }, kSampleRate).isEmpty());
    REQUIRE(player.numChannels() == 2);
    REQUIRE(player.numSamples() == 4096);
    REQUIRE(player.getDescription().contains("2 ch"));

    file.deleteFile();
}

TEST_CASE("the bench plays only when asked and loops the material")
{
    juce::AudioBuffer<float> source(2, 512);
    source.clear();
    for (int i = 0; i < 512; ++i)
    {
        source.setSample(0, i, 0.5f);
        source.setSample(1, i, -0.5f);
    }

    const auto file = writeTempWav(source);

    FilePlayer player;
    REQUIRE(player.load({ file }, kSampleRate).isEmpty());

    juce::AudioBuffer<float> block(2, 128);
    block.clear();
    REQUIRE_FALSE(player.fill(block, 2));
    REQUIRE(block.getSample(0, 0) == 0.0f);

    player.setPlaying(true);
    REQUIRE(player.isPlaying());

    // Материал короче восьми блоков — стенд обязан играть по кругу, а не стихнуть.
    for (int i = 0; i < 8; ++i)
    {
        REQUIRE(player.fill(block, 2));
        REQUIRE_THAT(block.getSample(0, 0), WithinAbs(0.5f, 1.0e-3f));
        REQUIRE_THAT(block.getSample(1, 0), WithinAbs(-0.5f, 1.0e-3f));
    }

    player.clear();
    REQUIRE_FALSE(player.hasMaterial());
    REQUIRE_FALSE(player.fill(block, 2));

    file.deleteFile();
}

TEST_CASE("the analysis window skips the silence before the first hit")
{
    juce::AudioBuffer<float> source(2, 8192);
    source.clear();
    for (int i = 4096; i < 8192; ++i)
    {
        source.setSample(0, i, 0.4f);
        source.setSample(1, i, 0.4f);
    }

    const auto file = writeTempWav(source);

    FilePlayer player;
    REQUIRE(player.load({ file }, kSampleRate).isEmpty());

    std::vector<float> window(2 * 4096, 0.0f);
    const int read = player.readAnalysisWindow(window.data(), 2, 4096);

    REQUIRE(read == 4096);
    REQUIRE_THAT(window.front(), WithinAbs(0.4f, 1.0e-3f));

    file.deleteFile();
}

TEST_CASE("export writes the aligned kit as a readable wav")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto source = impulsePair(4096, 100, 124);
    const auto input = writeTempWav(source);

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);
    REQUIRE(processor.getFilePlayer().load({ input }, kSampleRate).isEmpty());

    auto* delayParam = processor.getParameters().getParameter(beat::channelParamId(0, "delayMs"));
    delayParam->setValueNotifyingHost(delayParam->convertTo0to1(0.5f)); // 24 сэмпла

    const auto output = juce::File::createTempFile("wav");
    REQUIRE(processor.exportAligned(output).isEmpty());

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(output));
    REQUIRE(reader != nullptr);
    REQUIRE(reader->numChannels == 2);
    REQUIRE(reader->sampleRate == kSampleRate);

    juce::AudioBuffer<float> rendered(2, static_cast<int>(reader->lengthInSamples));
    reader->read(&rendered, 0, rendered.getNumSamples(), 0, true, true);
    REQUIRE(peakIndex(rendered, 0) == peakIndex(rendered, 1));

    reader.reset();
    input.deleteFile();
    output.deleteFile();
}

TEST_CASE("export without material says so instead of writing an empty file")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    const auto file = juce::File::createTempFile("wav");
    REQUIRE(processor.exportAligned(file).isNotEmpty());
    REQUIRE_FALSE(file.existsAsFile());
}

TEST_CASE("saved estimates come back without a new Analyze")
{
    juce::ScopedJuceInitialiser_GUI gui;

    beat::AlignmentEngine::Result result;
    result.status = beat::AnalysisStatus::ok;
    result.numChannels = 2;
    result.reference = 0;
    result.coherenceBefore = 0.55f;
    result.coherenceAfter = 0.91f;
    result.channels[1].tdoaSamples = 24.0f;
    result.channels[1].valid = true;
    result.snapshot = beat::AlignmentSnapshot::identity(2);
    result.snapshot.delaySamples[0] = 24.0f;

    BeatEqualizerAudioProcessor saved;
    saved.enableAllBuses();
    saved.prepareToPlay(kSampleRate, 128);
    saved.applyAnalysisResult(result);

    juce::MemoryBlock state;
    saved.getStateInformation(state);

    BeatEqualizerAudioProcessor restored;
    restored.enableAllBuses();
    restored.prepareToPlay(kSampleRate, 128);
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    REQUIRE_THAT(restored.getCoherenceAfter(), WithinAbs(0.91f, 1.0e-4f));
    REQUIRE(restored.getAnalysisStatus().contains("restored"));

    // Задержка приезжает параметрами, а не блобом.
    REQUIRE_THAT(restored.getParameters()
                     .getRawParameterValue(beat::channelParamId(0, "delayMs"))
                     ->load(),
                 WithinAbs(0.5f, 0.01f));
}

TEST_CASE("the bench row only exists in Standalone")
{
    juce::ScopedJuceInitialiser_GUI gui;

    // wrapperType проставляет обёртка, поэтому притворяемся ею до конструктора.
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    REQUIRE(processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    const auto source = impulsePair(4096, 100, 124);
    const auto input = writeTempWav(source);
    REQUIRE(processor.getFilePlayer().load({ input }, kSampleRate).isEmpty());

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setSize(1040, 796);

    juce::Image image(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics g(image);
        editor->paintEntireComponent(g, true);
    }

    const juce::File dest(BEAT_GUI_TEST_PNG);
    auto benchPng = dest.getSiblingFile("standalone-bench-test.png");
    benchPng.deleteFile();
    juce::FileOutputStream stream(benchPng);
    REQUIRE(stream.openedOk());
    juce::PNGImageFormat png;
    REQUIRE(png.writeImageToStream(image, stream));

    int visibleButtons = 0;
    for (auto* child : editor->getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*>(child))
            if (button->isVisible())
                ++visibleButtons;

    // Analyze плюс три кнопки стенда.
    REQUIRE(visibleButtons == 4);

    input.deleteFile();
}
