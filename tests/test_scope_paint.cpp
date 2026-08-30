#include <catch2/catch_test_macros.hpp>

#include "plugin/ChannelRow.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <memory>

#ifndef BEAT_GUI_TEST_PNG
#define BEAT_GUI_TEST_PNG "scope-gui-test.png"
#endif

namespace
{

int countNearColour(const juce::Image& image, juce::Colour target, int tolerance)
{
    int hits = 0;
    const int w = image.getWidth();
    const int h = image.getHeight();
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
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

void writePng(const juce::Image& image, const juce::File& dest)
{
    dest.deleteFile();
    juce::FileOutputStream stream(dest);
    REQUIRE(stream.openedOk());
    juce::PNGImageFormat png;
    REQUIRE(png.writeImageToStream(image, stream));
}

void fillSine(juce::AudioBuffer<float>& buffer, int block, float amp0, float amp1)
{
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        const float t = (float) (block * buffer.getNumSamples() + n);
        buffer.setSample(0, n, amp0 * std::sin(t * 0.08f));
        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, n, amp1 * std::sin(t * 0.08f + 0.4f));
    }
}

} // namespace

TEST_CASE("processBlock writes output into the scope ring")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor proc;
    proc.enableAllBuses();
    proc.prepareToPlay(48000.0, 64);

    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    buffer.clear();
    buffer.setSample(0, 10, 1.0f);
    proc.processBlock(buffer, midi);

    float out[64] {};
    proc.getScope().copyLast(0, out, 64);

    float peak = 0.0f;
    for (float sample : out)
        peak = std::max(peak, std::abs(sample));

    REQUIRE(peak > 0.1f);
}

TEST_CASE("channel row paints a cyan oscilloscope strip")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor proc;
    ChannelRow row(proc.getParameters(), 0);
    row.setBounds(0, 0, 700, ChannelRow::kHeight);
    row.setActive(true);

    float sine[512];
    for (int i = 0; i < 512; ++i)
        sine[i] = 0.8f * std::sin((float) i * 0.12f);
    row.setWaveform(sine, 512);

    juce::Image image(juce::Image::ARGB, 700, ChannelRow::kHeight, true);
    {
        juce::Graphics g(image);
        row.paintEntireComponent(g, true);
    }

    writePng(image, juce::File(BEAT_GUI_TEST_PNG).getSiblingFile("scope-row-test.png"));

    const int cyan = countNearColour(image, juce::Colour(0xff5ec8ff), 40);
    REQUIRE(cyan > 80);
}

TEST_CASE("editor copies the scope ring into cyan traces")
{
    juce::ScopedJuceInitialiser_GUI gui;

    BeatEqualizerAudioProcessor proc;
    proc.enableAllBuses();
    proc.prepareToPlay(48000.0, 128);

    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;
    for (int block = 0; block < 64; ++block)
    {
        fillSine(buffer, block, 0.9f, 0.6f);
        proc.processBlock(buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
    REQUIRE(editor != nullptr);
    editor->setSize(920, 640);

    auto* scoped = dynamic_cast<BeatEqualizerAudioProcessorEditor*>(editor.get());
    REQUIRE(scoped != nullptr);
    scoped->refreshWaveforms();

    juce::Image image(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics g(image);
        editor->paintEntireComponent(g, true);
    }

    const juce::File dest(BEAT_GUI_TEST_PNG);
    writePng(image, dest);

    const int cyan = countNearColour(image, juce::Colour(0xff5ec8ff), 40);
    REQUIRE(cyan > 200);
}
