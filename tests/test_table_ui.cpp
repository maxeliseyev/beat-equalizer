#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "SyntheticKit.h"
#include "plugin/ChannelRow.h"
#include "plugin/Correlometer.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

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

bool setFourChannels(BeatEqualizerAudioProcessor& processor)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::discreteChannels(4));
    layout.outputBuses.add(juce::AudioChannelSet::discreteChannels(4));
    return processor.setBusesLayout(layout);
}
} // namespace

TEST_CASE("table columns never overlap and the delay slider keeps the rest")
{
    const auto columns = ChannelColumns::from({ 0, 0, 900, 36 });

    REQUIRE(columns.delay.getWidth() > 200);
    REQUIRE_FALSE(overlaps(columns.enable, columns.name));
    REQUIRE_FALSE(overlaps(columns.name, columns.role));
    REQUIRE_FALSE(overlaps(columns.role, columns.delay));
    REQUIRE_FALSE(overlaps(columns.delay, columns.rotator));
    REQUIRE_FALSE(overlaps(columns.rotator, columns.polarity));
    REQUIRE_FALSE(overlaps(columns.polarity, columns.corr));
    REQUIRE(columns.corr.getRight() == 900);
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
