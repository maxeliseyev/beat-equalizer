#include "ChannelRow.h"

ChannelColumns ChannelColumns::from(juce::Rectangle<int> row)
{
    ChannelColumns columns;
    columns.enable = row.removeFromLeft(ChannelRow::kEnableWidth);
    columns.name = row.removeFromLeft(ChannelRow::kNameWidth);
    columns.role = row.removeFromLeft(ChannelRow::kRoleWidth);
    columns.corr = row.removeFromRight(ChannelRow::kCorrWidth);
    columns.polarity = row.removeFromRight(ChannelRow::kPolarityWidth);
    columns.rotator = row.removeFromRight(ChannelRow::kRotatorWidth);
    columns.delay = row;
    return columns;
}

ChannelRow::ChannelRow(juce::AudioProcessorValueTreeState& state, int index)
{
    enabledButton.setClickingTogglesState(true);
    enabledButton.setButtonText({});
    addAndMakeVisible(enabledButton);

    nameLabel.setText(juce::String::formatted("%02d", index + 1), juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(nameLabel);

    roleBox.addItem("-", 1);
    roleBox.addItem("Close", 2);
    roleBox.addItem("OH", 3);
    roleBox.addItem("Room", 4);
    roleBox.addItem("Hats", 5);
    addAndMakeVisible(roleBox);

    delaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    delaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 20);
    delaySlider.setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(delaySlider);

    rotatorSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    // Проценты приходят из самого параметра: attachment перетирает
    // textFromValueFunction слайдера своей версией.
    rotatorSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    addAndMakeVisible(rotatorSlider);

    polarityBox.addItem("Auto", 1);
    polarityBox.addItem("Positive", 2);
    polarityBox.addItem("Invert", 3);
    addAndMakeVisible(polarityBox);

    corrLabel.setJustificationType(juce::Justification::centredRight);
    corrLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(corrLabel);

    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, beat::channelParamId(index, "enabled"), enabledButton);
    roleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, beat::channelParamId(index, "role"), roleBox);
    delayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, beat::channelParamId(index, "delayMs"), delaySlider);
    rotatorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, beat::channelParamId(index, "rotatorAmount"), rotatorSlider);
    polarityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, beat::channelParamId(index, "polarity"), polarityBox);

    setActive(false);
    setCorrelation(0.0f);
}

void ChannelRow::resized()
{
    const auto columns = ChannelColumns::from(getLocalBounds());
    enabledButton.setBounds(columns.enable.reduced(6, 6));
    nameLabel.setBounds(columns.name);
    roleBox.setBounds(columns.role.reduced(2, 5));
    delaySlider.setBounds(columns.delay.reduced(4, 6));
    rotatorSlider.setBounds(columns.rotator.reduced(4, 6));
    polarityBox.setBounds(columns.polarity.reduced(2, 5));
    corrLabel.setBounds(columns.corr);
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

void ChannelRow::setCorrelation(float value)
{
    const float clamped = juce::jlimit(-1.0f, 1.0f, value);
    corrLabel.setText(juce::String(clamped, 2), juce::dontSendNotification);

    // Отрицательная корреляция с опорой — это либо полярность, либо промах
    // выравнивания; красным, чтобы бросалось в глаза без чтения цифр.
    corrLabel.setColour(juce::Label::textColourId,
                        clamped < -0.05f ? juce::Colour(0xffe06c75) : juce::Colour(0xffc5cad3));
}

void ChannelRow::setIsReference(bool isReference)
{
    if (!isReference)
        return;

    corrLabel.setText("ref", juce::dontSendNotification);
    corrLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
}
