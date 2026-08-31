#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/Grid.h"
#include "plugin/ChannelRow.h"
#include "plugin/Exporter.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <cmath>
#include <memory>

using Catch::Matchers::WithinAbs;

namespace
{
constexpr double kSampleRate = 48000.0;

// Хост, которого в тестах нет: отдаёт ровно то, что спрашивает процессор.
class FakePlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        juce::AudioPlayHead::PositionInfo info;
        info.setBpm(bpm);
        info.setTimeSignature(juce::AudioPlayHead::TimeSignature { numerator, denominator });
        info.setPpqPosition(ppq);
        info.setIsPlaying(true);
        return info;
    }

    double bpm = 96.0;
    double ppq = 8.0;
    int numerator = 3;
    int denominator = 4;
};

void runBlock(BeatEqualizerAudioProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
}

int countNearColour(const juce::Image& image, juce::Colour target, int tolerance)
{
    int hits = 0;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const auto p = image.getPixelAt(x, y);
            if (std::abs((int) p.getRed() - (int) target.getRed()) <= tolerance
                && std::abs((int) p.getGreen() - (int) target.getGreen()) <= tolerance
                && std::abs((int) p.getBlue() - (int) target.getBlue()) <= tolerance)
                ++hits;
        }
    return hits;
}
} // namespace

TEST_CASE("tempo and time signature come from the host when it has them")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    FakePlayHead head;
    processor.setPlayHead(&head);
    runBlock(processor, 128);

    const auto transport = processor.getTransport();
    REQUIRE(transport.fromHost);
    REQUIRE_THAT(transport.bpm, WithinAbs(96.0, 1.0e-6));
    REQUIRE(transport.numerator == 3);
    REQUIRE(transport.denominator == 4);
    REQUIRE(transport.hasPosition);

    // Хост отдаёт позицию на начало блока, осциллограф показывает его конец.
    const double expected = 8.0 + 128.0 / kSampleRate * 96.0 / 60.0;
    REQUIRE_THAT(transport.quartersAtWrite, WithinAbs(expected, 1.0e-9));

    processor.setPlayHead(nullptr);
}

TEST_CASE("manual tempo wins when asked, and covers a host without one")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    auto& state = processor.getParameters();
    auto* tempo = state.getParameter("global.tempoBpm");
    tempo->setValueNotifyingHost(tempo->convertTo0to1(141.0f));

    // Хоста нет вовсе — Standalone.
    runBlock(processor, 128);
    auto transport = processor.getTransport();
    REQUIRE_FALSE(transport.fromHost);
    REQUIRE_THAT(transport.bpm, WithinAbs(141.0, 0.01));
    REQUIRE_FALSE(transport.hasPosition);

    // Хост есть, но пользователь выбрал ручной темп.
    FakePlayHead head;
    processor.setPlayHead(&head);
    state.getParameter("global.tempoSource")->setValueNotifyingHost(1.0f);
    runBlock(processor, 128);

    transport = processor.getTransport();
    REQUIRE_FALSE(transport.fromHost);
    REQUIRE_THAT(transport.bpm, WithinAbs(141.0, 0.01));
    REQUIRE_THAT(transport.hostBpm, WithinAbs(96.0, 1.0e-6));

    processor.setPlayHead(nullptr);
}

TEST_CASE("the grid division parameter reaches the transport")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    auto* grid = processor.getParameters().getParameter("global.gridDivision");

    grid->setValueNotifyingHost(grid->convertTo0to1(0.0f));
    REQUIRE(processor.getTransport().division == beat::grid::Division::off);

    grid->setValueNotifyingHost(grid->convertTo0to1(4.0f));
    REQUIRE(processor.getTransport().division == beat::grid::Division::sixteenth);
}

TEST_CASE("the bench draws grid lines over the traces")
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.enableAllBuses();
    processor.prepareToPlay(kSampleRate, 128);

    // Две секунды материала: при 120 BPM это четыре доли.
    juce::AudioBuffer<float> kit(2, 96000);
    kit.clear();
    // Удары нарочно не на сетке: иначе линии окажутся ровно под трассой и
    // проверка будет измерять цвет волны, а не сетки.
    for (int ch = 0; ch < 2; ++ch)
        for (int hit = 0; hit < 12; ++hit)
            for (int i = 0; i < 200; ++i)
                kit.setSample(ch, 1000 + hit * 7000 + i, (i % 2 == 0) ? 0.8f : -0.8f);

    const auto file = juce::File::createTempFile("wav");
    REQUIRE(beat::exporter::writeWav(file, kit, kSampleRate));
    REQUIRE(processor.getFilePlayer().load({ file }, kSampleRate).isEmpty());

    auto& state = processor.getParameters();
    state.getParameter("global.tempoSource")->setValueNotifyingHost(1.0f);
    auto* tempo = state.getParameter("global.tempoBpm");
    tempo->setValueNotifyingHost(tempo->convertTo0to1(120.0f));
    auto* time = state.getParameter("global.scopeTimeMs");
    time->setValueNotifyingHost(time->convertTo0to1(1000.0f));
    auto* grid = state.getParameter("global.gridDivision");
    grid->setValueNotifyingHost(grid->convertTo0to1(2.0f)); // 1/8

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* scoped = dynamic_cast<BeatEqualizerAudioProcessorEditor*>(editor.get());
    REQUIRE(scoped != nullptr);
    scoped->refreshWaveforms();

    juce::Image image(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics g(image);
        editor->paintEntireComponent(g, true);
    }

    const juce::File png =
        juce::File(BEAT_GUI_TEST_PNG).getSiblingFile("tempo-grid-test.png");
    png.deleteFile();
    {
        juce::FileOutputStream stream(png);
        REQUIRE(stream.openedOk());
        juce::PNGImageFormat format;
        REQUIRE(format.writeImageToStream(image, stream));
    }

    // Сетку выключили — сравниваем с той же картинкой без линий: абсолютный
    // счёт ловил бы сглаженный текст похожего оттенка.
    grid->setValueNotifyingHost(grid->convertTo0to1(0.0f));
    scoped->refreshWaveforms();

    juce::Image off(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics g(off);
        editor->paintEntireComponent(g, true);
    }

    // Секундное окно при 120 BPM — две четверти: доли и восьмые между ними.
    const auto beats = juce::Colour(0xff4d5a6b);
    const auto divisions = juce::Colour(0xff2b333d);
    REQUIRE(countNearColour(image, beats, 6) - countNearColour(off, beats, 6) > 60);
    REQUIRE(countNearColour(image, divisions, 6) - countNearColour(off, divisions, 6) > 60);

    file.deleteFile();
}
