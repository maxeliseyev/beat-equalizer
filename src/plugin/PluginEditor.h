#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class BeatEqualizerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::ChangeListener
{
public:
    explicit BeatEqualizerAudioProcessorEditor(BeatEqualizerAudioProcessor&);
    ~BeatEqualizerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void updateChannelLabel();

    BeatEqualizerAudioProcessor& audioProcessor;

    juce::Label title;
    juce::Label channelLabel;
    juce::Label note;

    juce::Label referenceLabel;
    juce::ComboBox referenceBox;
    juce::Label distanceLabel;
    juce::Slider distanceSlider;
    juce::ToggleButton abButton { "A/B Bypass" };
    juce::ToggleButton monoSumButton { "Mono Sum" };

    juce::Label ch1DelayLabel;
    juce::Slider ch1DelaySlider;
    juce::Label ch2DelayLabel;
    juce::Slider ch2DelaySlider;
    juce::Label ch2PolarityLabel;
    juce::ComboBox ch2PolarityBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> referenceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoSumAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ch1DelayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ch2DelayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ch2PolarityAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatEqualizerAudioProcessorEditor)
};
