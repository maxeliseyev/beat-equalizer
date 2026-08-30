#pragma once

#include "ChannelRow.h"
#include "PluginProcessor.h"

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class BeatEqualizerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::ChangeListener,
                                                private juce::Timer
{
public:
    explicit BeatEqualizerAudioProcessorEditor(BeatEqualizerAudioProcessor&);
    ~BeatEqualizerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshWaveforms();

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void updateLayoutInfo();
    void updateRowVisibility();
    void updateWaveforms();

    BeatEqualizerAudioProcessor& audioProcessor;

    juce::Label title;
    juce::Label layoutLabel;
    juce::Label latencyLabel;
    juce::Label hint;

    juce::ToggleButton abButton { "A/B (original, same PDC)" };
    juce::Label referenceLabel;
    juce::ComboBox referenceBox;
    juce::Label distanceLabel;
    juce::Slider distanceSlider;

    juce::Label headerOn;
    juce::Label headerName;
    juce::Label headerWave;
    juce::Label headerDelay;
    juce::Label headerPolarity;

    std::vector<float> scopeScratch;
    std::vector<float> scopeWindow;

    juce::Viewport viewport;
    juce::Component rowList;
    std::vector<std::unique_ptr<ChannelRow>> rows;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> referenceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distanceAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatEqualizerAudioProcessorEditor)
};
