#pragma once

#include "Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

class ChannelRow final : public juce::Component
{
public:
    static constexpr int kHeight = 52;
    static constexpr int kEnableWidth = 44;
    static constexpr int kNameWidth = 44;
    static constexpr int kWaveWidth = 260;
    static constexpr int kPolarityWidth = 108;

    ChannelRow(juce::AudioProcessorValueTreeState& state, int channelIndex);

    void resized() override;
    void paint(juce::Graphics&) override;

    void setActive(bool shouldBeActive);
    void setWaveform(const float* samples, int count);

    static void layoutHeader(juce::Rectangle<int> row,
                             juce::Label& on,
                             juce::Label& name,
                             juce::Label& wave,
                             juce::Label& delay,
                             juce::Label& polarity);

private:
    juce::Rectangle<int> waveBounds() const;

    bool active = false;
    std::vector<float> waveform;

    juce::ToggleButton enabledButton { "On" };
    juce::Label nameLabel;
    juce::Slider delaySlider;
    juce::ComboBox polarityBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> polarityAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelRow)
};
