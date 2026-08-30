#include "ChannelRow.h"

#include <cmath>

namespace
{
juce::Rectangle<int> peakBounds(juce::Rectangle<int> row)
{
    return row.withTrimmedLeft(ChannelRow::kEnableWidth + ChannelRow::kNameWidth)
        .removeFromLeft(ChannelRow::kPeakWidth)
        .reduced(2, 6);
}
} // namespace

void ChannelRow::layoutHeader(juce::Rectangle<int> row,
                              juce::Label& on,
                              juce::Label& name,
                              juce::Label& peak,
                              juce::Label& delay,
                              juce::Label& polarity)
{
    on.setBounds(row.removeFromLeft(kEnableWidth));
    name.setBounds(row.removeFromLeft(kNameWidth));
    peak.setBounds(row.removeFromLeft(kPeakWidth));
    polarity.setBounds(row.removeFromRight(kPolarityWidth));
    delay.setBounds(row);
}

ChannelRow::ChannelRow(juce::AudioProcessorValueTreeState& state, int index)
{
    enabledButton.setClickingTogglesState(true);
    addAndMakeVisible(enabledButton);

    nameLabel.setText(juce::String::formatted("%02d", index + 1), juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(nameLabel);

    delaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    delaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 20);
    delaySlider.setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(delaySlider);

    polarityBox.addItem("Auto", 1);
    polarityBox.addItem("Positive", 2);
    polarityBox.addItem("Invert", 3);
    addAndMakeVisible(polarityBox);

    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, beat::channelParamId(index, "enabled"), enabledButton);
    delayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, beat::channelParamId(index, "delayMs"), delaySlider);
    polarityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, beat::channelParamId(index, "polarity"), polarityBox);

    setActive(false);
}

void ChannelRow::resized()
{
    auto row = getLocalBounds();
    enabledButton.setBounds(row.removeFromLeft(kEnableWidth).reduced(2, 2));
    nameLabel.setBounds(row.removeFromLeft(kNameWidth));
    row.removeFromLeft(kPeakWidth);
    polarityBox.setBounds(row.removeFromRight(kPolarityWidth).reduced(2, 2));
    delaySlider.setBounds(row.reduced(4, 2));
}

void ChannelRow::paint(juce::Graphics& g)
{
    if (active)
        g.setColour(juce::Colour(0xff1e222a));
    else
        g.setColour(juce::Colour(0xff14161b));
    g.fillRect(getLocalBounds());

    auto meter = peakBounds(getLocalBounds());
    g.setColour(juce::Colour(0xff0d0f13));
    g.fillRect(meter);

    const float db = juce::Decibels::gainToDecibels(peak, -60.0f);
    const float level = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto fill = meter.removeFromLeft(juce::roundToInt((float) meter.getWidth() * level));
    g.setColour(peak > 0.001f ? juce::Colour(0xff3dd68c) : juce::Colour(0xff2a3038));
    g.fillRect(fill);
}

void ChannelRow::setActive(bool shouldBeActive)
{
    active = shouldBeActive;
    setVisible(shouldBeActive);
    setEnabled(shouldBeActive);
    nameLabel.setColour(juce::Label::textColourId,
                        shouldBeActive ? juce::Colours::white : juce::Colour(0xff6b7280));
}

void ChannelRow::setPeak(float linearPeak)
{
    if (std::abs(peak - linearPeak) < 0.0005f)
        return;
    peak = linearPeak;
    repaint(peakBounds(getLocalBounds()));
}
