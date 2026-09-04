#pragma once

#include "ChannelRow.h"
#include "Correlometer.h"
#include "OverviewStrip.h"
#include "PluginProcessor.h"
#include "SourceDiagnosticTable.h"
#include "dsp/Constants.h"

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
    enum class UiMode
    {
        basic = 0,
        advanced = 1
    };

    explicit BeatEqualizerAudioProcessorEditor(BeatEqualizerAudioProcessor&);
    ~BeatEqualizerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshWaveforms();
    // Окно в исходных отсчётах — то, что задаёт Time. Точек в буфере может
    // быть меньше: длинное окно прореживается.
    int getScopeWindowSamples() const { return displaySpan; }
    int getScopeDisplayPoints() const;
    float getCorrelometerValue() const { return correlometer.getCorrelation(); }
    // Сколько строк канал+осциллограмма сейчас показано: в Standalone это
    // может быть больше, чем каналов у устройства.
    int activeChannelCount() const;
    ChannelRow& getRow(int index) { return *rows[static_cast<size_t>(index)]; }
    juce::String getSourceStatusText() const { return sourceStatus.getText(); }
    UiMode getUiMode() const;
    void refreshUiMode();
    bool isAdvancedChromeVisible() const { return correlometer.isVisible(); }
    bool isSourceStatusVisible() const { return sourceStatus.isVisible(); }
    bool isGlideStrengthVisible() const { return glideStrengthSlider.isVisible(); }
    bool isScopeGridVisible() const { return gridBox.isVisible(); }
    bool isDistanceControlVisible() const { return distanceSlider.isVisible(); }
    bool isHintVisible() const { return hint.isVisible(); }
    bool isDetectStatusVisible() const { return detectStatus.isVisible(); }
    bool isSourceDiagnosticsVisible() const { return sourceDiagnostics.isVisible(); }
    juce::String getPrimaryStatusText() const { return analysisStatus.getText(); }
    juce::String getDetectStatusText() const { return detectStatus.getText(); }
    juce::String getBenchStatusText() const { return benchLabel.getText(); }
    juce::String getSourceDiagnosticsText() const { return sourceDiagnostics.getText(); }
    int getSourceDiagnosticsRowCount() const { return sourceDiagnostics.getRowCount(); }
    void refreshStatus();
    int chromeHeight() const;
    // Тестам нужно попасть в полосу обзора и обновить её без таймера.
    juce::Rectangle<int> getOverviewBounds() const { return overview.getBounds(); }
    void refreshTransport() { updateTransportRow(); }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void updateLayoutInfo();
    void updateRowVisibility();
    void updateWaveforms();
    void updateAnalysisStatus();
    void updateBench();
    // События последнего Detect: маркеры на строках и на полосе обзора,
    // колонка по-ударной задержки.
    void updateDetection();
    void updateTransportRow();
    void syncChannelCount();
    void setUiMode(UiMode mode);
    void updateUiModeControls();
    void updateUiModeVisibility();
    void updateEditorSizeForMode();
    bool isAdvancedMode() const;
    ChannelTableMode currentChannelTableMode() const;
    // Колонки монитора появляются вместе с материалом стенда и исчезают с ним.
    void setMonitorColumns(bool visible);
    void updateChannelNames();
    void updateTransportInfo();
    // Линии сетки внутри показанного окна; count = 0, когда темпа или позиции нет.
    int buildGrid(double startQuarters, int windowSamples, beat::grid::Line* out) const;
    bool isAudible(int channel) const;
    juce::String basicOutcomeStatus();
    juce::String benchStatusForMode(const juce::String& advancedText);
    void syncBenchStatusForMode();
    void updateSourceDiagnostics();
    int sourceDiagnosticsHeight() const;
    std::array<beat::ChannelRole, beat::kMaxChannels> channelRoles() const;

    BeatEqualizerAudioProcessor& audioProcessor;

    juce::Label title;
    juce::Label layoutLabel;
    juce::Label latencyLabel;
    juce::Label hint;
    juce::TextButton basicModeButton { "Basic" };
    juce::TextButton advancedModeButton { "Advanced" };

    juce::TextButton loadButton { "Load files..." };
    juce::TextButton rewindButton { "|<" };
    juce::TextButton playButton { "Play" };
    juce::TextButton exportButton { "Export static..." };
    juce::TextButton glideExportButton { "Export glide..." };
    juce::TextButton audioButton { "Audio..." };
    juce::TextButton detectButton { "Detect" };
    juce::Label glideStrengthLabel;
    juce::Slider glideStrengthSlider;
    juce::Label benchLabel;
    OverviewStrip overview;
    juce::Label positionLabel;
    juce::Label deviceLabel;
    juce::Label detectStatus;
    juce::Label sourceStatus;
    SourceDiagnosticTable sourceDiagnostics;
    std::unique_ptr<juce::FileChooser> chooser;
    bool standalone = false;
    bool benchLoaded = false;
    int overviewGeneration = -1;
    int displaySpan = 0;
    // Сдвиг окна каждой строки в отсчётах — тот же, с которым читается волна.
    // Маркеры обязаны считаться от него же, иначе удар нарисуется не там, где
    // он на картинке.
    std::array<int, beat::kMaxChannels> displayShift {};
    bool monitorColumns = false;
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
    juce::Label headerLevel;
    juce::Label headerPan;
    juce::Label headerCorr;
    juce::Label headerPhase;
    juce::Label headerPerHit;

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
    std::array<std::atomic<float>*, beat::kMaxChannels> roleParams {};
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
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        glideStrengthAttachment;
    std::atomic<float>* scopeTimeParam = nullptr;
    std::atomic<float>* uiModeParam = nullptr;
    juce::String lastGlideStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatEqualizerAudioProcessorEditor)
};
