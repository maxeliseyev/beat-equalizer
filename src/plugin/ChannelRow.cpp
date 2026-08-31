#include "ChannelRow.h"

void ChannelRow::layoutHeader(juce::Rectangle<int> row,
                              juce::Label& on,
                              juce::Label& name,
                              juce::Label& delay,
                              juce::Label& polarity)
{
    on.setBounds(row.removeFromLeft(kEnableWidth));
    name.setBounds(row.removeFromLeft(kNameWidth));
    polarity.setBounds(row.removeFromRight(kPolarityWidth));
    delay.setBounds(row);
}

ChannelRow::ChannelRow(juce::AudioProcessorValueTreeState& state, int index)
{
    enabledButton.setClickingTogglesState(true);
    enabledButton.setButtonText({});
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
    enabledButton.setBounds(row.removeFromLeft(kEnableWidth).reduced(8, 6));
    nameLabel.setBounds(row.removeFromLeft(kNameWidth));
    polarityBox.setBounds(row.removeFromRight(kPolarityWidth).reduced(2, 4));
    delaySlider.setBounds(row.reduced(4, 6));
}

void ChannelRow::paint(juce::Graphics& g)
{
    g.setColour(active ? juce::Colour(0xff1e222a) : juce::Colour(0xff14161b));
    g.fillRect(getLocalBounds());
}

void ChannelRow::setActive(bool shouldBeActive)
{
    active = shouldBeActive;
    setVisible(shouldBeActive);
    setEnabled(shouldBeActive);
    nameLabel.setColour(juce::Label::textColourId,
                        shouldBeActive ? juce::Colours::white : juce::Colour(0xff6b7280));
}
