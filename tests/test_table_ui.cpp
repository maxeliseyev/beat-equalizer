#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "SyntheticKit.h"
#include "plugin/ChannelRow.h"
#include "plugin/Correlometer.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <cmath>
#include <memory>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
bool overlaps(juce::Rectangle<int> a, juce::Rectangle<int> b)
{
    return a.getRight() > b.getX() && b.getRight() > a.getX();
}

int countNearColour(const juce::Image& image, juce::Colour target, int tolerance)
{
    int hits = 0;
    for (int y = 0; y < image.getHeight(); ++y)
    {
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const auto p = image.getPixelAt(x, y);
            if (std::abs((int) p.getRed() - (int) target.getRed()) <= tolerance
                && std::abs((int) p.getGreen() - (int) target.getGreen()) <= tolerance
                && std::abs((int) p.getBlue() - (int) target.getBlue()) <= tolerance)
                ++hits;
        }
    }
    return hits;
}

juce::Image render(juce::Component& component, int width, int height)
{
    component.setBounds(0, 0, width, height);
    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Graphics g(image);
    component.paintEntireComponent(g, true);
    return image;
}

bool setChannels(BeatEqualizerAudioProcessor& processor, int channels)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::discreteChannels(channels));
    layout.outputBuses.add(juce::AudioChannelSet::discreteChannels(channels));
    return processor.setBusesLayout(layout);
}

bool setFourChannels(BeatEqualizerAudioProcessor& processor)
{
    return setChannels(processor, 4);
}
} // namespace

TEST_CASE("table columns never overlap and the waveform takes the right edge")
{
    const auto columns = ChannelColumns::from({ 0, 0, 1200, ChannelRow::kHeight });

    REQUIRE(columns.delay.getWidth() == ChannelRow::kDelayWidth);
    REQUIRE_FALSE(overlaps(columns.enable, columns.name));
    REQUIRE_FALSE(overlaps(columns.name, columns.level));
    REQUIRE_FALSE(overlaps(columns.level, columns.pan));
    REQUIRE_FALSE(overlaps(columns.pan, columns.delay));
    REQUIRE_FALSE(overlaps(columns.delay, columns.rotator));
    REQUIRE_FALSE(overlaps(columns.rotator, columns.polarity));
    REQUIRE_FALSE(overlaps(columns.polarity, columns.corr));
    REQUIRE_FALSE(overlaps(columns.corr, columns.phase));
    REQUIRE_FALSE(overlaps(columns.phase, columns.perHit));
    REQUIRE_FALSE(overlaps(columns.perHit, columns.scope));

    // Ручки фиксированной ширины, вся лишняя ширина уходит осциллограмме.
    REQUIRE(columns.perHit.getRight() == ChannelRow::kControlsWidth);
    REQUIRE(columns.scope.getRight() == 1200);
    REQUIRE(columns.scope.getWidth() == 1200 - ChannelRow::kControlsWidth);
}

TEST_CASE("basic table columns hide manual diagnostics and give the waveform room")
{
    const auto advanced =
        ChannelColumns::from({ 0, 0, 1200, ChannelRow::kHeight },
                             true,
                             ChannelTableMode::advanced);
    const auto basicBench =
        ChannelColumns::from({ 0, 0, 1200, ChannelRow::kHeight },
                             true,
                             ChannelTableMode::basic);
    const auto basicHost =
        ChannelColumns::from({ 0, 0, 1200, ChannelRow::kHeight },
                             false,
                             ChannelTableMode::basic);

    REQUIRE(ChannelRow::controlsWidth(ChannelTableMode::basic, false)
            == ChannelRow::kEnableWidth + ChannelRow::kNameWidth);
    REQUIRE(ChannelRow::controlsWidth(ChannelTableMode::basic, true)
            == ChannelRow::kEnableWidth + ChannelRow::kSoloWidth + ChannelRow::kMuteWidth
                   + ChannelRow::kNameWidth);

    CHECK(basicBench.scope.getWidth() > advanced.scope.getWidth());
    CHECK(basicHost.scope.getWidth() > basicBench.scope.getWidth());
    CHECK(basicBench.delay.getWidth() == 0);
    CHECK(basicBench.rotator.getWidth() == 0);
    CHECK(basicBench.polarity.getWidth() == 0);
    CHECK(basicBench.corr.getWidth() == 0);
    CHECK(basicBench.phase.getWidth() == 0);
    CHECK(basicBench.perHit.getWidth() == 0);
    CHECK(basicBench.level.getWidth() == 0);
    CHECK(basicBench.pan.getWidth() == 0);
    CHECK(basicBench.solo.getWidth() == ChannelRow::kSoloWidth);
    CHECK(basicBench.mute.getWidth() == ChannelRow::kMuteWidth);
    CHECK(basicHost.solo.getWidth() == 0);
    CHECK(basicHost.mute.getWidth() == 0);
}

TEST_CASE("monitor columns appear only when the bench has material")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    ChannelRow row(processor.getParameters(), 0);
    row.setActive(true);
    row.setBounds(0, 0, 1200, ChannelRow::kHeight);
    row.resized();

    const int withoutMonitor = row.getScopeBounds().getWidth();

    row.setMonitorVisible(true);
    const auto columns = ChannelColumns::from({ 0, 0, 1200, ChannelRow::kHeight });
    REQUIRE(row.getScopeBounds() == columns.scope);
    REQUIRE(columns.level.getWidth() == ChannelRow::kLevelWidth);
    REQUIRE(columns.pan.getWidth() == ChannelRow::kPanWidth);

    // Level и Pan забирают ширину у осциллограммы, а не у ручек выравнивания.
    REQUIRE(withoutMonitor - row.getScopeBounds().getWidth()
            == ChannelRow::kLevelWidth + ChannelRow::kPanWidth);
    REQUIRE(columns.delay.getX() == columns.pan.getRight());

    row.setMonitorVisible(false);
    REQUIRE(row.getScopeBounds().getWidth() == withoutMonitor);
}

TEST_CASE("a channel row switches between Basic and Advanced table modes")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    ChannelRow row(processor.getParameters(), 0);
    row.setActive(true);
    row.setBounds(0, 0, 1200, ChannelRow::kHeight);
    row.setMonitorVisible(true);
    row.setTableMode(ChannelTableMode::basic);
    row.resized();

    const auto basicColumns =
        ChannelColumns::from({ 0, 0, 1200, ChannelRow::kHeight },
                             true,
                             ChannelTableMode::basic);
    REQUIRE(row.getTableMode() == ChannelTableMode::basic);
    CHECK(row.isSoloVisible());
    CHECK_FALSE(row.isLevelVisible());
    CHECK_FALSE(row.isDelayVisible());
    CHECK_FALSE(row.isPerHitVisible());
    CHECK(row.getScopeBounds() == basicColumns.scope);
    const int basicScopeWidth = row.getScopeBounds().getWidth();

    row.setTableMode(ChannelTableMode::advanced);
    const auto advancedColumns =
        ChannelColumns::from({ 0, 0, 1200, ChannelRow::kHeight },
                             true,
                             ChannelTableMode::advanced);
    CHECK(row.isSoloVisible());
    CHECK(row.isLevelVisible());
    CHECK(row.isDelayVisible());
    CHECK(row.isPerHitVisible());
    CHECK(row.getScopeBounds() == advancedColumns.scope);
    CHECK(row.getScopeBounds().getWidth() < basicScopeWidth);

    row.setMonitorVisible(false);
    row.setTableMode(ChannelTableMode::basic);
    const auto hostBasicColumns =
        ChannelColumns::from({ 0, 0, 1200, ChannelRow::kHeight },
                             false,
                             ChannelTableMode::basic);
    CHECK_FALSE(row.isSoloVisible());
    CHECK(row.getScopeBounds() == hostBasicColumns.scope);
}

TEST_CASE("a channel row draws its own waveform right of the controls")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    ChannelRow row(processor.getParameters(), 0);
    row.setActive(true);

    std::vector<float> sine(1024);
    for (size_t i = 0; i < sine.size(); ++i)
        sine[i] = 0.8f * std::sin(0.12f * static_cast<float>(i));
    row.setWaveform(sine.data(), (int) sine.size());

    const auto image = render(row, 1200, ChannelRow::kHeight);
    // Материала стенда нет, значит нет и колонок монитора: осциллограмма
    // забирает их место.
    const auto columns = ChannelColumns::from({ 0, 0, 1200, ChannelRow::kHeight }, false);
    REQUIRE(row.getScopeBounds() == columns.scope);

    const auto cyan = juce::Colour(0xff5ec8ff);
    REQUIRE(countNearColour(image.getClippedImage(columns.scope), cyan, 40) > 200);
    REQUIRE(countNearColour(image.getClippedImage({ 0, 0, columns.scope.getX(),
                                                   ChannelRow::kHeight }),
                            cyan,
                            40)
            == 0);
}

TEST_CASE("the window grows down linearly with the channel count")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor four;
    REQUIRE(setChannels(four, 4));
    BeatEqualizerAudioProcessor eight;
    REQUIRE(setChannels(eight, 8));

    std::unique_ptr<juce::AudioProcessorEditor> smallEditor(four.createEditor());
    std::unique_ptr<juce::AudioProcessorEditor> bigEditor(eight.createEditor());

    // Каждый канал добавляет ровно одну строку и ни пикселя по ширине.
    REQUIRE(bigEditor->getHeight() - smallEditor->getHeight() == 4 * ChannelRow::kHeight);
    REQUIRE(bigEditor->getWidth() == smallEditor->getWidth());
}

TEST_CASE("correlometer reads +1 in phase and -1 out of phase")
{
    const auto x = beat::test::whiteNoise(2048, 5);
    std::vector<float> flipped(x.size());
    for (size_t i = 0; i < x.size(); ++i)
        flipped[i] = -x[i];

    Correlometer meter;
    meter.setPair(x.data(), x.data(), 2048);
    REQUIRE_THAT(meter.getCorrelation(), WithinAbs(1.0f, 0.001f));
    REQUIRE(meter.getPointCount() > 0);

    const auto inPhase = render(meter, 600, Correlometer::kHeight);
    REQUIRE(countNearColour(inPhase, juce::Colour(0xff7ddc9a), 20) > 200);

    meter.setPair(x.data(), flipped.data(), 2048);
    REQUIRE_THAT(meter.getCorrelation(), WithinAbs(-1.0f, 0.001f));

    const auto outOfPhase = render(meter, 600, Correlometer::kHeight);
    REQUIRE(countNearColour(outOfPhase, juce::Colour(0xffe06c75), 20) > 200);
}

TEST_CASE("correlometer without a pair draws nothing but the frame")
{
    Correlometer meter;
    meter.setPair(nullptr, nullptr, 0);
    REQUIRE(meter.getPointCount() == 0);
    REQUIRE(meter.getCorrelation() == 0.0f);
}

TEST_CASE("mono sum monitors on 1-2 and leaves the other stems aligned")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    REQUIRE(setFourChannels(processor));
    processor.prepareToPlay(48000.0, 64);

    auto* monoSum = processor.getParameters().getParameter("global.monoSum");
    monoSum->setValueNotifyingHost(1.0f);

    juce::AudioBuffer<float> buffer(4, 64);
    juce::MidiBuffer midi;

    for (int block = 0; block < 4; ++block)
    {
        for (int ch = 0; ch < 4; ++ch)
            for (int n = 0; n < 64; ++n)
                buffer.setSample(ch, n, 0.1f * static_cast<float>(ch + 1));

        processor.processBlock(buffer, midi);
    }

    const float mono = 0.25f * (0.1f + 0.2f + 0.3f + 0.4f);
    REQUIRE_THAT(buffer.getSample(0, 63), WithinAbs(mono, 1.0e-4f));
    REQUIRE_THAT(buffer.getSample(1, 63), WithinAbs(mono, 1.0e-4f));
    REQUIRE_THAT(buffer.getSample(2, 63), WithinAbs(0.3f, 1.0e-4f));
    REQUIRE_THAT(buffer.getSample(3, 63), WithinAbs(0.4f, 1.0e-4f));
}

TEST_CASE("a disabled channel drops out of the mono sum")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    REQUIRE(setFourChannels(processor));
    processor.prepareToPlay(48000.0, 64);

    processor.getParameters().getParameter("global.monoSum")->setValueNotifyingHost(1.0f);
    processor.getParameters().getParameter(beat::channelParamId(3, "enabled"))
        ->setValueNotifyingHost(0.0f);

    juce::AudioBuffer<float> buffer(4, 64);
    juce::MidiBuffer midi;

    for (int block = 0; block < 4; ++block)
    {
        for (int ch = 0; ch < 4; ++ch)
            for (int n = 0; n < 64; ++n)
                buffer.setSample(ch, n, 0.1f * static_cast<float>(ch + 1));

        processor.processBlock(buffer, midi);
    }

    REQUIRE_THAT(buffer.getSample(0, 63), WithinAbs((0.1f + 0.2f + 0.3f) / 3.0f, 1.0e-4f));
}

TEST_CASE("mute silences its own channel, solo silences the others")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    REQUIRE(setFourChannels(processor));
    processor.prepareToPlay(48000.0, 64);

    auto& state = processor.getParameters();
    juce::AudioBuffer<float> buffer(4, 64);
    juce::MidiBuffer midi;

    const auto run = [&]
    {
        for (int block = 0; block < 4; ++block)
        {
            for (int ch = 0; ch < 4; ++ch)
                for (int n = 0; n < 64; ++n)
                    buffer.setSample(ch, n, 0.1f * static_cast<float>(ch + 1));

            processor.processBlock(buffer, midi);
        }
    };

    state.getParameter(beat::channelParamId(1, "mute"))->setValueNotifyingHost(1.0f);
    run();

    REQUIRE_THAT(buffer.getSample(0, 63), WithinAbs(0.1f, 1.0e-4f));
    REQUIRE(buffer.getSample(1, 63) == 0.0f);
    REQUIRE_THAT(buffer.getSample(2, 63), WithinAbs(0.3f, 1.0e-4f));

    // Solo сильнее mute на других каналах: слышен только он.
    state.getParameter(beat::channelParamId(2, "solo"))->setValueNotifyingHost(1.0f);
    run();

    REQUIRE(buffer.getSample(0, 63) == 0.0f);
    REQUIRE(buffer.getSample(1, 63) == 0.0f);
    REQUIRE_THAT(buffer.getSample(2, 63), WithinAbs(0.3f, 1.0e-4f));
    REQUIRE(buffer.getSample(3, 63) == 0.0f);
}

TEST_CASE("a muted channel drops out of the mono sum")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    REQUIRE(setFourChannels(processor));
    processor.prepareToPlay(48000.0, 64);

    processor.getParameters().getParameter("global.monoSum")->setValueNotifyingHost(1.0f);
    processor.getParameters().getParameter(beat::channelParamId(3, "mute"))
        ->setValueNotifyingHost(1.0f);

    juce::AudioBuffer<float> buffer(4, 64);
    juce::MidiBuffer midi;

    for (int block = 0; block < 4; ++block)
    {
        for (int ch = 0; ch < 4; ++ch)
            for (int n = 0; n < 64; ++n)
                buffer.setSample(ch, n, 0.1f * static_cast<float>(ch + 1));

        processor.processBlock(buffer, midi);
    }

    REQUIRE_THAT(buffer.getSample(0, 63), WithinAbs((0.1f + 0.2f + 0.3f) / 3.0f, 1.0e-4f));
}

TEST_CASE("a channel row shows the number and the stem name")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    ChannelRow row(processor.getParameters(), 0);

    REQUIRE(row.getLabelText() == "01");
    row.setChannelName("kick");
    REQUIRE(row.getLabelText() == "01  kick");
    row.setChannelName({});
    REQUIRE(row.getLabelText() == "01");
}

TEST_CASE("the phase column shows the coherence of the pair before and after")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    ChannelRow row(processor.getParameters(), 2);

    // До Analyze мерить нечего.
    REQUIRE(row.getPhaseText() == "-");

    row.setPhaseMatch(0.62f, 0.88f, true);
    REQUIRE(row.getPhaseText() == "62 -> 88");

    row.setPhaseMatch(0.0f, 0.0f, false);
    REQUIRE(row.getPhaseText() == "-");
}

TEST_CASE("editor fills the correlometer from the scope ring")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor processor;
    processor.enableAllBuses();
    processor.prepareToPlay(48000.0, 128);

    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;
    for (int block = 0; block < 64; ++block)
    {
        for (int n = 0; n < 128; ++n)
        {
            const float t = static_cast<float>(block * 128 + n);
            const float x = std::sin(0.08f * t);
            buffer.setSample(0, n, x);
            buffer.setSample(1, n, -x); // второй микрофон в противофазе
        }

        processor.processBlock(buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* scoped = dynamic_cast<BeatEqualizerAudioProcessorEditor*>(editor.get());
    REQUIRE(scoped != nullptr);
    editor->setSize(1040, 760);
    scoped->refreshWaveforms();

    REQUIRE(scoped->getCorrelometerValue() < -0.9f);
}
