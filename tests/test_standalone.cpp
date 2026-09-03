#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "RealKit.h"
#include "SyntheticKit.h"
#include "doc/Event.h"
#include "dsp/AlignmentEngine.h"
#include "plugin/ChannelRow.h"
#include "plugin/Exporter.h"
#include "plugin/FilePlayer.h"
#include "plugin/OverviewStrip.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kRealKitRate = 96000.0;

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

int peakIndex(const juce::AudioBuffer<float>& buffer, int channel, int from, int to)
{
    int best = from;
    float peak = 0.0f;
    for (int i = from; i < to && i < buffer.getNumSamples(); ++i)
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

juce::AudioBuffer<float> readWav(const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    REQUIRE(reader != nullptr);

    juce::AudioBuffer<float> rendered((int) reader->numChannels,
                                      (int) reader->lengthInSamples);
    reader->read(&rendered, 0, rendered.getNumSamples(), 0, true, true);
    return rendered;
}

void setParameter(BeatEqualizerAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getParameters().getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

juce::AudioBuffer<float> readRealKitWindow(const std::string& dir,
                                           double startSec,
                                           double lengthSec)
{
    const int samples = static_cast<int>(std::lround(lengthSec * kRealKitRate));
    juce::AudioBuffer<float> buffer(beat::test::kKitMicCount, samples);
    buffer.clear();

    for (int mic = 0; mic < beat::test::kKitMicCount; ++mic)
    {
        const auto data = beat::test::wavRead(
            beat::test::kitPath(dir, mic),
            static_cast<std::int64_t>(std::lround(startSec * kRealKitRate)),
            samples);
        REQUIRE(static_cast<int>(data.size()) == samples);
        buffer.copyFrom(mic, 0, data.data(), samples);
    }

    return buffer;
}

beat::AlignmentEngine::Result analyzeBench(BeatEqualizerAudioProcessor& processor,
                                           int reference)
{
    auto& player = processor.getFilePlayer();
    const int channels = juce::jmin(player.numChannels(), beat::kMaxChannels);
    const int window = beat::AnalysisRing::capacityForSampleRate(kRealKitRate);
    std::vector<float> scratch(static_cast<size_t>(channels) * static_cast<size_t>(window),
                               0.0f);
    const int available = player.readAnalysisWindow(scratch.data(), channels, window);

    std::vector<const float*> pointers(static_cast<size_t>(channels));
    for (int ch = 0; ch < channels; ++ch)
        pointers[static_cast<size_t>(ch)] =
            scratch.data() + static_cast<std::ptrdiff_t>(ch) * window;

    beat::AlignmentEngine engine;
    beat::AnalysisRequest request;
    request.sampleRate = kRealKitRate;
    request.reference = reference;
    return engine.analyze(pointers.data(), channels, available, request);
}

void printExport(const char* label, const juce::File& file)
{
    std::cout << label << ": " << file.getFullPathName() << " ("
              << juce::String(static_cast<double>(file.getSize()) / 1000000.0, 1).toStdString()
              << " MB)\n";
}

void checkRealKitExport(const juce::File& file, int minimumSamples)
{
    beat::test::WavInfo info {};
    REQUIRE(beat::test::wavInfoOf(file.getFullPathName().toStdString(), info));
    CHECK_THAT(info.sampleRate, WithinAbs(kRealKitRate, 1.0e-6));
    CHECK(info.numChannels == beat::test::kKitMicCount);
    CHECK(info.numFrames >= minimumSamples);
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

TEST_CASE("pause keeps the position and rewind sends it back to the start")
{
    // Пила: по значению отсчёта видно, откуда именно продолжили.
    juce::AudioBuffer<float> ramp(1, 2048);
    for (int i = 0; i < 2048; ++i)
        ramp.setSample(0, i, static_cast<float>(i) / 2048.0f);

    const auto file = writeTempWav(ramp);

    FilePlayer player;
    REQUIRE(player.load({ file }, kSampleRate).isEmpty());

    juce::AudioBuffer<float> block(1, 512);
    player.setPlaying(true);
    REQUIRE(player.fill(block, 1));
    REQUIRE(player.getPosition() == 512);

    // Пауза не двигает позицию и не отдаёт материал.
    player.setPlaying(false);
    block.clear();
    REQUIRE_FALSE(player.fill(block, 1));
    REQUIRE(player.getPosition() == 512);

    // Продолжили с того же места.
    player.setPlaying(true);
    REQUIRE(player.fill(block, 1));
    REQUIRE(player.getPosition() == 1024);
    REQUIRE_THAT(block.getSample(0, 0), WithinAbs(512.0f / 2048.0f, 1.0e-3f));

    // Отмотка — снова с начала.
    player.rewind();
    REQUIRE(player.getPosition() == 0);
    REQUIRE(player.fill(block, 1));
    REQUIRE_THAT(block.getSample(0, 0), WithinAbs(0.0f, 1.0e-3f));

    // Перемотка ручкой не вылезает за материал.
    player.setPosition(999999);
    REQUIRE(player.getPosition() == 2047);
    player.setPosition(-5);
    REQUIRE(player.getPosition() == 0);

    file.deleteFile();
}

TEST_CASE("the bench survives a device sample rate change")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    juce::AudioBuffer<float> source(2, 4800); // 100 мс на 48 kHz
    source.clear();
    for (int ch = 0; ch < 2; ++ch)
        source.setSample(ch, 100, 1.0f);

    const auto file = writeTempWav(source);
    auto& player = processor.getFilePlayer();
    REQUIRE(player.load({ file }, kSampleRate).isEmpty());
    REQUIRE(player.numSamples() == 4800);

    // Пользователь переключил устройство на 96 kHz в диалоге Audio…
    processor.prepareToPlay(96000.0, 128);
    REQUIRE(processor.reloadBenchForSampleRate());
    REQUIRE_THAT(player.getSampleRate(), WithinAbs(96000.0, 1.0e-6));
    REQUIRE(player.numSamples() > 9000); // те же 100 мс, вдвое больше отсчётов

    // Частота не менялась — перезагружать нечего.
    REQUIRE_FALSE(processor.reloadBenchForSampleRate());

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

TEST_CASE("glide export uses fresh per-hit delays and reports coherence")
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioBuffer<float> source(2, 16000);
    source.clear();
    source.setSample(0, 1000, 1.0f);
    source.setSample(1, 1010, 1.0f);
    source.setSample(0, 12000, 1.0f);
    source.setSample(1, 12030, 1.0f);

    const auto input = writeTempWav(source);

    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);
    REQUIRE(processor.getFilePlayer().load({ input }, kSampleRate).isEmpty());

    const auto rejected = juce::File::createTempFile("wav");
    REQUIRE(processor.exportGlide(rejected).isNotEmpty());
    REQUIRE_FALSE(rejected.existsAsFile());

    DetectWorker::Result detection;
    detection.generation = processor.getFilePlayer().getGeneration();
    detection.sampleRate = kSampleRate;
    detection.from = 0;
    detection.length = source.getNumSamples();
    detection.valid = true;

    const auto addEvent = [&detection](int referenceSample, int otherSample)
    {
        beat::doc::Event event;
        event.referenceChannel = 0;
        event.timeSamples = static_cast<double>(referenceSample);

        auto& reference = event.channels[0];
        reference.present = true;
        reference.arrivalSamples = static_cast<double>(referenceSample);
        reference.attackEndSamples = static_cast<double>(referenceSample + 8);

        auto& other = event.channels[1];
        other.present = true;
        other.arrivalSamples = static_cast<double>(otherSample);
        other.attackEndSamples = static_cast<double>(otherSample + 8);

        const auto id = detection.document.addEvent(event);
        detection.document.delays().setRaw(id, 0, 0.0);
        detection.document.delays().setRaw(id, 1,
                                           static_cast<double>(otherSample - referenceSample));
    };

    addEvent(1000, 1010);
    addEvent(12000, 12030);
    processor.applyDetection(std::move(detection));
    REQUIRE(processor.canExportGlide());
    CHECK(processor.getGlideStatus().contains("Glide preview @ 100%"));

    auto* strength = processor.getParameters().getParameter("global.glideStrength");
    REQUIRE(strength != nullptr);
    strength->setValueNotifyingHost(strength->convertTo0to1(0.0f));
    REQUIRE(processor.refreshGlidePreview().isEmpty());
    CHECK(processor.getGlideStatus().contains("Glide preview @ 0%"));

    const auto bypassed = juce::File::createTempFile("wav");
    REQUIRE(processor.exportGlide(bypassed).isEmpty());
    const auto dry = readWav(bypassed);
    CHECK(peakIndex(dry, 0, 950, 1100) != peakIndex(dry, 1, 950, 1100));

    strength->setValueNotifyingHost(strength->convertTo0to1(1.0f));
    REQUIRE(processor.refreshGlidePreview().isEmpty());

    const auto output = juce::File::createTempFile("wav");
    REQUIRE(processor.exportGlide(output).isEmpty());
    CHECK(processor.getGlideStatus().contains("Glide exported"));
    CHECK(processor.getGlideStatus().contains("event coherence"));

    const auto rendered = readWav(output);

    CHECK(peakIndex(rendered, 0, 950, 1100) == peakIndex(rendered, 1, 950, 1100));
    CHECK(peakIndex(rendered, 0, 11950, 12100) == peakIndex(rendered, 1, 11950, 12100));

    input.deleteFile();
    bypassed.deleteFile();
    output.deleteFile();
}

TEST_CASE("real kit glide export writes comparable static and strength renders",
          "[.real-kit-export][real-kit]")
{
    const auto dir = beat::test::realKitDir();
    if (dir.empty())
        SKIP("BEAT_REAL_KIT_DIR не задан: реальный кит не лежит в репозитории");

    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioProcessor::setTypeOfNextNewPlugin(
        juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.enableAllBuses();
    processor.prepareToPlay(kRealKitRate, 512);

    constexpr double startSec = 10.0;
    constexpr double lengthSec = 20.0;
    auto window = readRealKitWindow(dir, startSec, lengthSec);

    auto outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("beat-equalizer-real-kit");
    REQUIRE(outputDir.createDirectory());

    const auto input = outputDir.getChildFile("yan9-10s-30s.wav");
    REQUIRE(beat::exporter::writeWav(input, window, kRealKitRate));

    juce::Array<juce::File> files;
    files.add(input);
    REQUIRE(processor.getFilePlayer().load(files, kRealKitRate).isEmpty());

    setParameter(processor, "global.reference", 2.0f); // snare top, 1-based APVTS

    const int reference = beat::test::kSnareTop;
    const auto analysis = analyzeBench(processor, reference);
    REQUIRE(analysis.status == beat::AnalysisStatus::ok);
    processor.applyAnalysisResult(analysis);

    DetectWorker::Request request;
    request.clip = &processor.getFilePlayer().getBuffer();
    request.generation = processor.getFilePlayer().getGeneration();
    request.sampleRate = kRealKitRate;
    request.reference = reference;
    request.from = 0;
    request.length = processor.getFilePlayer().numSamples();

    DetectWorker::Result detection;
    DetectWorker::detect(request, detection);
    REQUIRE(detection.valid);
    processor.applyDetection(std::move(detection));
    REQUIRE(processor.canExportGlide());

    const auto sourceStatus = processor.getSourceDiagnosticStatus();
    CHECK(sourceStatus.contains("src Ch 2"));
    CHECK(sourceStatus.contains("close Ch 3"));
    CHECK(sourceStatus.contains("late Ch"));

    const auto staticFile = outputDir.getChildFile("yan9-static.wav");
    const auto glide50File = outputDir.getChildFile("yan9-glide-50.wav");
    const auto glide100File = outputDir.getChildFile("yan9-glide-100.wav");

    REQUIRE(processor.exportAligned(staticFile).isEmpty());

    auto* strength = processor.getParameters().getParameter("global.glideStrength");
    REQUIRE(strength != nullptr);

    strength->setValueNotifyingHost(strength->convertTo0to1(0.5f));
    REQUIRE(processor.refreshGlidePreview().isEmpty());
    const auto preview50 = processor.getGlideStatus();
    REQUIRE(processor.exportGlide(glide50File).isEmpty());
    const auto export50 = processor.getGlideStatus();

    strength->setValueNotifyingHost(strength->convertTo0to1(1.0f));
    REQUIRE(processor.refreshGlidePreview().isEmpty());
    const auto preview100 = processor.getGlideStatus();
    REQUIRE(processor.exportGlide(glide100File).isEmpty());
    const auto export100 = processor.getGlideStatus();

    checkRealKitExport(staticFile, window.getNumSamples());
    checkRealKitExport(glide50File, window.getNumSamples());
    checkRealKitExport(glide100File, window.getNumSamples());

    std::cout << "\nYAN9 glide export smoke\n";
    std::cout << "source: " << input.getFullPathName().toStdString() << "\n";
    std::cout << "window: " << juce::String(startSec, 1).toStdString() << "-"
              << juce::String(startSec + lengthSec, 1).toStdString() << " s, "
              << beat::test::kKitMicCount << " ch @ 96 kHz\n";
    std::cout << "analyze: " << processor.getAnalysisStatus().toStdString()
              << ", sum coherence "
              << juce::String(juce::roundToInt(100.0f * processor.getCoherenceBefore()))
                     .toStdString()
              << "% -> "
              << juce::String(juce::roundToInt(100.0f * processor.getCoherenceAfter()))
                     .toStdString()
              << "%\n";
    std::cout << "detect: " << processor.getDetectStatus().toStdString() << "\n";
    std::cout << "source: " << sourceStatus.toStdString() << "\n";
    std::cout << "preview 50: " << preview50.toStdString() << "\n";
    std::cout << "export  50: " << export50.toStdString() << "\n";
    std::cout << "preview100: " << preview100.toStdString() << "\n";
    std::cout << "export 100: " << export100.toStdString() << "\n";

    for (int ch = 0; ch < beat::test::kKitMicCount; ++ch)
    {
        const auto id = beat::channelParamId(ch, "delayMs");
        const auto delayMs = processor.getParameters().getRawParameterValue(id)->load();
        std::cout << beat::test::kitNames()[ch] << ": static delay "
                  << juce::String(delayMs, 3).toStdString() << " ms\n";
    }

    printExport("static", staticFile);
    printExport("glide 50", glide50File);
    printExport("glide100", glide100File);
}

TEST_CASE("export without material says so instead of writing an empty file")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    const auto file = juce::File::createTempFile("wav");
    REQUIRE(processor.exportAligned(file).isNotEmpty());
    REQUIRE(processor.exportGlide(file).isNotEmpty());
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

TEST_CASE("each channel keeps the name of the file it came from")
{
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("beat-bench-names");
    dir.deleteRecursively();
    REQUIRE(dir.createDirectory());

    juce::AudioBuffer<float> mono(1, 512);
    mono.clear();
    mono.setSample(0, 100, 1.0f);

    juce::AudioBuffer<float> pair(2, 512);
    pair.clear();
    pair.setSample(0, 100, 1.0f);
    pair.setSample(1, 100, 1.0f);

    const auto kick = dir.getChildFile("kick.wav");
    const auto overheads = dir.getChildFile("overheads.wav");
    REQUIRE(beat::exporter::writeWav(kick, mono, kSampleRate));
    REQUIRE(beat::exporter::writeWav(overheads, pair, kSampleRate));

    FilePlayer player;
    REQUIRE(player.load({ kick, overheads }, kSampleRate).isEmpty());
    REQUIRE(player.numChannels() == 3);

    // Моно-файл подписан как есть, стерео — с номером дорожки.
    REQUIRE(player.getChannelName(0) == "kick");
    REQUIRE(player.getChannelName(1) == "overheads 1");
    REQUIRE(player.getChannelName(2) == "overheads 2");
    REQUIRE(player.getChannelName(3).isEmpty());

    player.clear();
    REQUIRE(player.getChannelName(0).isEmpty());

    dir.deleteRecursively();
}

TEST_CASE("mute does not reach the offline render")
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    const auto source = impulsePair(4096, 100, 124);
    const auto input = writeTempWav(source);
    REQUIRE(processor.getFilePlayer().load({ input }, kSampleRate).isEmpty());

    // Solo/Mute — мониторинг: экспорт обязан отдать весь кит.
    processor.getParameters().getParameter(beat::channelParamId(0, "mute"))
        ->setValueNotifyingHost(1.0f);

    const auto exported = juce::File::createTempFile("wav");
    REQUIRE(processor.exportAligned(exported).isEmpty());

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(exported));
    REQUIRE(reader != nullptr);

    juce::AudioBuffer<float> rendered((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read(&rendered, 0, rendered.getNumSamples(), 0, true, true);
    REQUIRE(rendered.getMagnitude(0, 0, rendered.getNumSamples()) > 0.5f);

    input.deleteFile();
    exported.deleteFile();
}

TEST_CASE("the bench monitor sums the whole kit into the stereo output")
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    // Устройство отдаёт два выхода, на стенде шесть дорожек с разными уровнями.
    REQUIRE(processor.getTotalNumOutputChannels() == 2);

    juce::AudioBuffer<float> kit(6, 4096);
    for (int ch = 0; ch < 6; ++ch)
        for (int i = 0; i < kit.getNumSamples(); ++i)
            kit.setSample(ch, i, 0.1f * static_cast<float>(ch + 1));

    const auto file = writeTempWav(kit);
    REQUIRE(processor.getFilePlayer().load({ file }, kSampleRate).isEmpty());
    processor.getFilePlayer().setPlaying(true);

    juce::AudioBuffer<float> block(2, 128);
    juce::MidiBuffer midi;
    block.clear();
    processor.processBlock(block, midi);

    // Все шесть каналов слышны в обоих выходах: 2.1 на шесть каналов, центр
    // равной мощности. Без монитор-микса на выходах лежали бы 0.1 и 0.2.
    const float expected = (2.1f / 6.0f) * std::cos(0.25f * juce::MathConstants<float>::pi);
    REQUIRE_THAT(block.getSample(0, 100), WithinAbs(expected, 1.0e-3f));
    REQUIRE_THAT(block.getSample(1, 100), WithinAbs(expected, 1.0e-3f));

    file.deleteFile();
}

TEST_CASE("pause silences the monitor instead of looping the last block")
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    juce::AudioBuffer<float> kit(4, 4096);
    for (int ch = 0; ch < 4; ++ch)
        for (int i = 0; i < kit.getNumSamples(); ++i)
            kit.setSample(ch, i, 0.5f);

    const auto file = writeTempWav(kit);
    auto& player = processor.getFilePlayer();
    REQUIRE(player.load({ file }, kSampleRate).isEmpty());
    player.setPlaying(true);

    juce::AudioBuffer<float> block(2, 128);
    juce::MidiBuffer midi;
    block.clear();
    processor.processBlock(block, midi);
    REQUIRE(block.getMagnitude(0, 0, block.getNumSamples()) > 0.1f);

    // Буфер стенда не переписывается на паузе: без очистки монитор гонял бы
    // по кругу тот же блок, и это слышно как зависший кусок.
    player.setPlaying(false);
    processor.processBlock(block, midi);
    REQUIRE_THAT(block.getSample(0, 100), WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(block.getSample(1, 100), WithinAbs(0.0f, 1.0e-5f));

    // И продолжает с того же места, когда сняли паузу.
    player.setPlaying(true);
    processor.processBlock(block, midi);
    REQUIRE(block.getMagnitude(0, 0, block.getNumSamples()) > 0.1f);

    file.deleteFile();
}

TEST_CASE("panning hard left keeps a bench channel out of the right output")
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    juce::AudioBuffer<float> kit(4, 4096);
    for (int ch = 0; ch < 4; ++ch)
        for (int i = 0; i < kit.getNumSamples(); ++i)
            kit.setSample(ch, i, 0.4f);

    const auto file = writeTempWav(kit);
    REQUIRE(processor.getFilePlayer().load({ file }, kSampleRate).isEmpty());
    processor.getFilePlayer().setPlaying(true);

    auto& state = processor.getParameters();
    // 0.0 нормализованного — это -1, крайнее левое.
    state.getParameter(beat::channelParamId(0, "pan"))->setValueNotifyingHost(0.0f);
    for (int ch = 1; ch < 4; ++ch)
        state.getParameter(beat::channelParamId(ch, "mute"))->setValueNotifyingHost(1.0f);

    juce::AudioBuffer<float> block(2, 128);
    juce::MidiBuffer midi;
    block.clear();
    processor.processBlock(block, midi);

    // Крайняя левая панорама отдаёт весь канал налево. Делитель монитора — все
    // четыре загруженных канала, а не один оставшийся слышимым.
    REQUIRE_THAT(block.getSample(0, 100), WithinAbs(0.25f * 0.4f, 1.0e-3f));
    REQUIRE_THAT(block.getSample(1, 100), WithinAbs(0.0f, 1.0e-4f));

    file.deleteFile();
}

TEST_CASE("solo does not make the channel louder than it was in the mix")
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    juce::AudioBuffer<float> kit(4, 4096);
    for (int ch = 0; ch < 4; ++ch)
        for (int i = 0; i < kit.getNumSamples(); ++i)
            kit.setSample(ch, i, 0.1f * static_cast<float>(ch + 1));

    const auto file = writeTempWav(kit);
    auto& player = processor.getFilePlayer();
    REQUIRE(player.load({ file }, kSampleRate).isEmpty());
    player.setPlaying(true);

    juce::AudioBuffer<float> block(2, 128);
    juce::MidiBuffer midi;
    const float centre = std::cos(0.25f * juce::MathConstants<float>::pi);

    block.clear();
    processor.processBlock(block, midi);
    REQUIRE_THAT(block.getSample(0, 100), WithinAbs(0.25f * 1.0f * centre, 1.0e-3f));

    // Solo убирает остальные, но не поднимает оставшийся: делитель монитора —
    // число загруженных каналов, а не слышимых сейчас.
    processor.getParameters().getParameter(beat::channelParamId(0, "solo"))
        ->setValueNotifyingHost(1.0f);
    processor.processBlock(block, midi);
    REQUIRE_THAT(block.getSample(0, 100), WithinAbs(0.25f * 0.1f * centre, 1.0e-4f));

    // Mute на одном канале — то же самое: остальные остаются на своём уровне.
    processor.getParameters().getParameter(beat::channelParamId(0, "solo"))
        ->setValueNotifyingHost(0.0f);
    processor.getParameters().getParameter(beat::channelParamId(3, "mute"))
        ->setValueNotifyingHost(1.0f);
    processor.processBlock(block, midi);
    REQUIRE_THAT(block.getSample(0, 100), WithinAbs(0.25f * (0.1f + 0.2f + 0.3f) * centre, 1.0e-3f));

    file.deleteFile();
}

TEST_CASE("monitor level and pan do not reach the offline render")
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    const auto source = impulsePair(4096, 100, 124);
    const auto input = writeTempWav(source);
    REQUIRE(processor.getFilePlayer().load({ input }, kSampleRate).isEmpty());

    auto& state = processor.getParameters();
    state.getParameter(beat::channelParamId(0, "pan"))->setValueNotifyingHost(0.0f);
    state.getParameter(beat::channelParamId(0, "levelDb"))->setValueNotifyingHost(0.0f);

    const auto exported = juce::File::createTempFile("wav");
    REQUIRE(processor.exportAligned(exported).isEmpty());

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(exported));
    REQUIRE(reader != nullptr);

    juce::AudioBuffer<float> rendered((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read(&rendered, 0, rendered.getNumSamples(), 0, true, true);

    // Уровень -60 дБ и крайняя панорама — это мониторинг: стем обязан выйти
    // нетронутым, иначе экспорт зависел бы от того, что сейчас слушают.
    REQUIRE(rendered.getNumChannels() == 2);
    REQUIRE(rendered.getMagnitude(0, 0, rendered.getNumSamples()) > 0.5f);

    input.deleteFile();
    exported.deleteFile();
}

TEST_CASE("the display window lines a delayed mic up with the reference")
{
    juce::AudioBuffer<float> source(2, 4096);
    source.clear();
    source.setSample(0, 1000, 1.0f);
    source.setSample(1, 1024, 1.0f); // второй микрофон на 24 отсчёта позже

    const auto file = writeTempWav(source);

    FilePlayer player;
    REQUIRE(player.load({ file }, kSampleRate).isEmpty());

    std::vector<float> first(512, 0.0f);
    std::vector<float> second(512, 0.0f);

    // Первому каналу дают ту же задержку, что и аудиопотоку: пики обязаны
    // встать в один отсчёт, иначе картинка врёт про то, что уйдёт в экспорт.
    REQUIRE(player.readDisplayWindow(0, first.data(), 512, 24) == 512);
    REQUIRE(player.readDisplayWindow(1, second.data(), 512, 0) == 512);

    const auto peakAt = [](const std::vector<float>& window)
    {
        return (int) std::distance(window.begin(),
                                   std::max_element(window.begin(), window.end()));
    };

    REQUIRE(peakAt(first) == peakAt(second));

    file.deleteFile();
}

TEST_CASE("a decimated display window keeps the transient")
{
    juce::AudioBuffer<float> source(1, 8192);
    source.clear();

    // Удар в один отсчёт: прореживание «через один» его бы потеряло.
    source.setSample(0, 4000, 0.9f);

    const auto file = writeTempWav(source);

    FilePlayer player;
    REQUIRE(player.load({ file }, kSampleRate).isEmpty());
    player.setPosition(8000);
    player.setPlaying(true);

    std::vector<float> full(4096, 0.0f);
    std::vector<float> decimated(512, 0.0f);

    REQUIRE(player.readDisplayWindow(0, full.data(), 4096, 0) == 4096);
    REQUIRE(player.readDisplayWindow(0, decimated.data(), 512, 0, 8) == 512);

    const auto peak = [](const std::vector<float>& window)
    { return *std::max_element(window.begin(), window.end()); };

    // Оба окна покрывают одни и те же 4096 отсчётов и обязаны видеть один пик.
    REQUIRE_THAT(peak(full), WithinAbs(0.9f, 1.0e-3f));
    REQUIRE_THAT(peak(decimated), WithinAbs(0.9f, 1.0e-3f));

    file.deleteFile();
}

TEST_CASE("the overview covers the whole take, not the visible window")
{
    juce::AudioBuffer<float> source(2, 48000);
    source.clear();

    // Материал звучит только во второй половине: обзор обязан это показать.
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 30000; i < 32000; ++i)
            source.setSample(ch, i, 0.8f);

    const auto file = writeTempWav(source);

    FilePlayer player;
    const int before = player.getGeneration();
    REQUIRE(player.load({ file }, kSampleRate).isEmpty());
    REQUIRE(player.getGeneration() != before);

    std::vector<float> bins(FilePlayer::kOverviewBins, -1.0f);
    const int count = player.readOverview(bins.data(), (int) bins.size());
    REQUIRE(count == FilePlayer::kOverviewBins);

    const int loud = count * 31000 / 48000;
    REQUIRE_THAT(bins[static_cast<size_t>(loud)], WithinAbs(0.8f, 1.0e-2f));
    REQUIRE_THAT(bins[static_cast<size_t>(count / 4)], WithinAbs(0.0f, 1.0e-4f));

    player.clear();
    REQUIRE(player.readOverview(bins.data(), (int) bins.size()) == 0);

    file.deleteFile();
}

TEST_CASE("clicking the overview moves the play position")
{
    juce::ScopedJuceInitialiser_GUI gui;

    OverviewStrip strip;
    strip.setBounds(0, 0, 401, OverviewStrip::kHeight);

    double seeked = -1.0;
    strip.onSeek = [&seeked](double value) { seeked = value; };

    const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),
                                 { 200.0f, 10.0f },
                                 juce::ModifierKeys::noModifiers,
                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &strip, &strip,
                                 juce::Time::getCurrentTime(),
                                 { 200.0f, 10.0f },
                                 juce::Time::getCurrentTime(),
                                 1,
                                 false);
    strip.mouseDown(event);

    // Клик посередине полосы — середина партии.
    REQUIRE_THAT(seeked, WithinAbs(0.5, 0.01));
}

TEST_CASE("bench rows follow the loaded files, not the audio device")
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    // Карта отдаёт два входа, а на стенде лежит кит из шести дорожек.
    REQUIRE(processor.getTotalNumInputChannels() == 2);

    juce::AudioBuffer<float> kit(6, 4096);
    kit.clear();
    for (int ch = 0; ch < 6; ++ch)
        for (int i = 0; i < 64; ++i)
            kit.setSample(ch, 1000 + 8 * ch + i, (i % 2 == 0) ? 0.7f : -0.7f);

    const auto file = writeTempWav(kit);
    REQUIRE(processor.getFilePlayer().load({ file }, kSampleRate).isEmpty());

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* scoped = dynamic_cast<BeatEqualizerAudioProcessorEditor*>(editor.get());
    REQUIRE(scoped != nullptr);

    REQUIRE(scoped->activeChannelCount() == 6);
    REQUIRE(editor->getHeight() == scoped->chromeHeight() + 6 * ChannelRow::kHeight);

    // Каналы 3…6 мимо устройства не проходят, но осциллограммы у них есть:
    // стенд рисует загруженный материал, а не кольцо выхода.
    scoped->refreshWaveforms();

    juce::Image image(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics g(image);
        editor->paintEntireComponent(g, true);
    }

    const juce::File png = juce::File(BEAT_GUI_TEST_PNG).getSiblingFile("per-channel-scopes-test.png");
    png.deleteFile();
    {
        juce::FileOutputStream stream(png);
        REQUIRE(stream.openedOk());
        juce::PNGImageFormat format;
        REQUIRE(format.writeImageToStream(image, stream));
    }

    const int bottomY = scoped->chromeHeight() + 3 * ChannelRow::kHeight;
    const auto bottom = image.getClippedImage(
        { 0, bottomY, image.getWidth(), image.getHeight() - bottomY });

    int cyan = 0;
    for (int y = 0; y < bottom.getHeight(); ++y)
        for (int x = 0; x < bottom.getWidth(); ++x)
        {
            const auto p = bottom.getPixelAt(x, y);
            if (std::abs((int) p.getRed() - 0x5e) <= 40 && std::abs((int) p.getGreen() - 0xc8) <= 40
                && std::abs((int) p.getBlue() - 0xff) <= 40)
                ++cyan;
        }

    REQUIRE(cyan > 200);

    file.deleteFile();
}

TEST_CASE("the transport shows the whole take with the playhead on it")
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    // Двенадцать секунд с паузой посередине: по обзору обязано быть видно и
    // длину партии, и то, что в середине тихо.
    const int length = static_cast<int>(12.0 * kSampleRate);
    juce::AudioBuffer<float> kit(4, length);
    kit.clear();

    for (int hit = 0; hit < 24; ++hit)
    {
        const int at = static_cast<int>((0.5 * hit) * kSampleRate);
        if (at > length - 2048)
            break;
        if (hit >= 8 && hit < 14) // тишина в середине
            continue;

        for (int ch = 0; ch < 4; ++ch)
            for (int i = 0; i < 2048; ++i)
                kit.setSample(ch, at + i,
                              0.9f * std::exp(-0.004f * (float) i)
                                  * std::sin(0.05f * (float) i));
    }

    const auto file = writeTempWav(kit);
    REQUIRE(processor.getFilePlayer().load({ file }, kSampleRate).isEmpty());
    processor.getFilePlayer().setPosition(length / 2);

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* scoped = dynamic_cast<BeatEqualizerAudioProcessorEditor*>(editor.get());
    REQUIRE(scoped != nullptr);
    scoped->refreshTransport();

    juce::Image image(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics g(image);
        editor->paintEntireComponent(g, true);
    }

    const juce::File png = juce::File(BEAT_GUI_TEST_PNG).getSiblingFile("bench-overview-test.png");
    png.deleteFile();
    {
        juce::FileOutputStream stream(png);
        REQUIRE(stream.openedOk());
        juce::PNGImageFormat format;
        REQUIRE(format.writeImageToStream(image, stream));
    }

    const auto strip = scoped->getOverviewBounds();
    REQUIRE(strip.getHeight() > 0);

    int played = 0;
    int ahead = 0;
    int playhead = 0;
    for (int y = strip.getY(); y < strip.getBottom(); ++y)
        for (int x = strip.getX(); x < strip.getRight(); ++x)
        {
            const auto p = image.getPixelAt(x, y);
            const auto near = [&p](juce::uint8 r, juce::uint8 g, juce::uint8 b)
            {
                return std::abs((int) p.getRed() - (int) r) <= 24
                       && std::abs((int) p.getGreen() - (int) g) <= 24
                       && std::abs((int) p.getBlue() - (int) b) <= 24;
            };

            if (near(0x5e, 0xc8, 0xff))
                ++played;
            else if (near(0x3c, 0x5a, 0x72))
                ++ahead;
            else if (near(0xe8, 0xc5, 0x47))
                ++playhead;
        }

    // Сыгранное, оставшееся и курсор — три разных цвета, и все три на полосе.
    REQUIRE(played > 100);
    REQUIRE(ahead > 100);
    REQUIRE(playhead > 10);

    file.deleteFile();
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

    // Размер редактор выбирает сам: он зависит от числа каналов и ряда стенда.
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());

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

    // Analyze плюс семь кнопок стенда: Load, |<, Play, Detect,
    // static/glide Export, Audio.
    REQUIRE(visibleButtons == 8);

    input.deleteFile();
}
