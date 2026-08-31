#pragma once

#include "ChannelRow.h"
#include "Correlometer.h"
#include "PluginProcessor.h"
#include "ScopeStrip.h"

#include <array>
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
    float getCorrelometerValue() const { return correlometer.getCorrelation(); }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void updateLayoutInfo();
    void updateRowVisibility();
    void updateWaveforms();
    void updateAnalysisStatus();
    void updateBench();
    int activeChannelCount() const;

    BeatEqualizerAudioProcessor& audioProcessor;

    juce::Label title;
    juce::Label layoutLabel;
    juce::Label latencyLabel;
    juce::Label hint;

    juce::TextButton loadButton { "Load files..." };
    juce::TextButton playButton { "Play" };
    juce::TextButton exportButton { "Export aligned..." };
    juce::Label benchLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    bool standalone = false;
    bool benchLoaded = false;

    juce::TextButton analyzeButton { "Analyze" };
    juce::ToggleButton freezeButton { "Freeze" };
    juce::Label analysisStatus;
    juce::Label coherenceLabel;

    juce::ToggleButton abButton { "A/B dry" };
    juce::ToggleButton monoSumButton { "Mono sum" };
    juce::Label referenceLabel;
    juce::ComboBox referenceBox;
    juce::Label distanceLabel;
    juce::Slider distanceSlider;

    juce::Label headerOn;
    juce::Label headerName;
    juce::Label headerRole;
    juce::Label headerDelay;
    juce::Label headerRotator;
    juce::Label headerPolarity;
    juce::Label headerCorr;

    Correlometer correlometer;

    juce::Label scopeHeader;
    juce::Label timeLabel;
    juce::Slider timeSlider;
    juce::Label scopeTimeLeft;
    juce::Label scopeTimeRight;

    std::vector<float> scopeScratch;
    std::vector<float> scopeWindow;
    std::vector<float> referenceWindow;
    std::vector<float> sumWindow;
    std::array<std::atomic<float>*, beat::kMaxChannels> enabledParams {};

    juce::Viewport tableViewport;
    juce::Component tableList;
    std::vector<std::unique_ptr<ChannelRow>> rows;

    juce::Viewport scopeViewport;
    juce::Component scopeList;
    std::vector<std::unique_ptr<ScopeStrip>> strips;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoSumAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> referenceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeAttachment;
    std::atomic<float>* scopeTimeParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatEqualizerAudioProcessorEditor)
};
