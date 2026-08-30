#pragma once

#include "Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class ChannelRow final : public juce::Component
{
public:
    static constexpr int kHeight = 36;
    static constexpr int kEnableWidth = 48;
    static constexpr int kNameWidth = 44;
    static constexpr int kPolarityWidth = 108;

    ChannelRow(juce::AudioProcessorValueTreeState& state, int channelIndex);

    void resized() override;
    void paint(juce::Graphics&) override;

    void setActive(bool shouldBeActive);

    static void layoutHeader(juce::Rectangle<int> row,
                             juce::Label& on,
                             juce::Label& name,
                             juce::Label& delay,
                             juce::Label& polarity);

private:
    bool active = false;

    juce::ToggleButton enabledButton;
    juce::Label nameLabel;
    juce::Slider delaySlider;
    juce::ComboBox polarityBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> polarityAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelRow)
};
