#pragma once

#include "ChannelRow.h"
#include "Correlometer.h"
#include "PluginProcessor.h"

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
    // Сколько строк канал+осциллограмма сейчас показано: в Standalone это
    // может быть больше, чем каналов у устройства.
    int activeChannelCount() const;
    int chromeHeight() const;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void updateLayoutInfo();
    void updateRowVisibility();
    void updateWaveforms();
    void updateAnalysisStatus();
    void updateBench();
    void updateTransportRow();
    void syncChannelCount();
    void updateChannelNames();
    void updateTransportInfo();
    // Линии сетки внутри показанного окна; count = 0, когда темпа или позиции нет.
    int buildGrid(double startQuarters, int windowSamples, beat::grid::Line* out) const;
    bool isAudible(int channel) const;

    BeatEqualizerAudioProcessor& audioProcessor;

    juce::Label title;
    juce::Label layoutLabel;
    juce::Label latencyLabel;
    juce::Label hint;

    juce::TextButton loadButton { "Load files..." };
    juce::TextButton rewindButton { "|<" };
    juce::TextButton playButton { "Play" };
    juce::TextButton exportButton { "Export aligned..." };
    juce::TextButton audioButton { "Audio..." };
    juce::Label benchLabel;
    juce::Slider positionSlider;
    juce::Label positionLabel;
    juce::Label deviceLabel;
    bool draggingPosition = false;
    std::unique_ptr<juce::FileChooser> chooser;
    bool standalone = false;
    bool benchLoaded = false;
    int lastActiveChannels = 0;

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
    juce::Label headerSolo;
    juce::Label headerMute;
    juce::Label headerName;
    juce::Label headerRole;
    juce::Label headerDelay;
    juce::Label headerRotator;
    juce::Label headerPolarity;
    juce::Label headerCorr;
    juce::Label headerPhase;

    Correlometer correlometer;

    juce::Label scopeHeader;
    juce::Label tempoLabel;
    juce::ComboBox tempoBox;
    juce::Slider tempoSlider;
    juce::Label gridLabel;
    juce::ComboBox gridBox;
    juce::Label timeLabel;
    juce::Slider timeSlider;
    juce::Label scopeTimeLeft;
    juce::Label scopeTimeRight;

    std::vector<float> scopeScratch;
    std::vector<float> scopeWindow;
    std::vector<float> referenceWindow;
    std::vector<float> sumWindow;
    std::array<std::atomic<float>*, beat::kMaxChannels> enabledParams {};
    std::array<std::atomic<float>*, beat::kMaxChannels> delayParams {};
    std::array<std::atomic<float>*, beat::kMaxChannels> polarityParams {};
    std::array<std::atomic<float>*, beat::kMaxChannels> muteParams {};
    std::array<std::atomic<float>*, beat::kMaxChannels> soloParams {};
    std::atomic<float>* bypassParam = nullptr;

    juce::Viewport tableViewport;
    juce::Component tableList;
    std::vector<std::unique_ptr<ChannelRow>> rows;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoSumAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> referenceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tempoSourceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAttachment;
    std::atomic<float>* scopeTimeParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatEqualizerAudioProcessorEditor)
};
