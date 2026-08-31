#pragma once

#include "ChannelRow.h"
#include "PluginProcessor.h"
#include "ScopeStrip.h"

#include <atomic>
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
    int getScopeWindowSamples() const;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void updateLayoutInfo();
    void updateRowVisibility();
    void updateWaveforms();
    void updateAnalysisStatus();
    int activeChannelCount() const;

    BeatEqualizerAudioProcessor& audioProcessor;

    juce::Label title;
    juce::Label layoutLabel;
    juce::Label latencyLabel;
    juce::Label hint;

    juce::TextButton analyzeButton { "Analyze" };
    juce::ToggleButton freezeButton { "Freeze" };
    juce::Label analysisStatus;

    juce::ToggleButton abButton { "A/B (original, same PDC)" };
    juce::Label referenceLabel;
    juce::ComboBox referenceBox;
    juce::Label distanceLabel;
    juce::Slider distanceSlider;

    juce::Label headerOn;
    juce::Label headerName;
    juce::Label headerDelay;
    juce::Label headerPolarity;

    juce::Label scopeHeader;
    juce::Label timeLabel;
    juce::Slider timeSlider;
    juce::Label scopeTimeLeft;
    juce::Label scopeTimeRight;

    std::vector<float> scopeScratch;
    std::vector<float> scopeWindow;

    juce::Viewport tableViewport;
    juce::Component tableList;
    std::vector<std::unique_ptr<ChannelRow>> rows;

    juce::Viewport scopeViewport;
    juce::Component scopeList;
    std::vector<std::unique_ptr<ScopeStrip>> strips;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> referenceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeAttachment;
    std::atomic<float>* scopeTimeParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatEqualizerAudioProcessorEditor)
};
