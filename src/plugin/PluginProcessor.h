#pragma once

#include "AnalysisWorker.h"
#include "DetectWorker.h"
#include "Exporter.h"
#include "FilePlayer.h"
#include "Parameters.h"
#include "ScopeRing.h"
#include "dsp/AlignmentSnapshot.h"
#include "dsp/AllpassRotator.h"
#include "dsp/AnalysisRing.h"
#include "dsp/Constants.h"
#include "dsp/FractionalDelay.h"
#include "dsp/GlideRenderer.h"
#include "dsp/Grid.h"

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

class BeatEqualizerAudioProcessor final : public juce::AudioProcessor,
                                          public juce::ChangeBroadcaster,
                                          private juce::AsyncUpdater
{
public:
    BeatEqualizerAudioProcessor();
    ~BeatEqualizerAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void numChannelsChanged() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    const beat::AlignmentSnapshot& getSnapshot() const { return snapshot; }
    double getCurrentSampleRate() const { return currentSampleRate; }
    float getInputPeak(int channel) const;
    const beat::ScopeRing& getScope() const { return scope; }
    int getReferenceChannelIndex() const;

    // Темп и позиция для сетки. Хост отдаёт их не всегда, поэтому в структуре
    // видно и то, что пришло, и то, что выбрал пользователь.
    struct TransportInfo
    {
        double bpm = beat::kDefaultTempoBpm;
        double hostBpm = 0.0;
        int numerator = 4;
        int denominator = 4;
        bool fromHost = false;
        bool hasPosition = false;
        double quartersAtWrite = 0.0;
        beat::grid::Division division = beat::grid::Division::off;
    };

    TransportInfo getTransport() const;
    const beat::AlignmentEngine::Result& getLastResult() const { return lastResult; }

    void requestAnalyze();

    // Detect на стенде: события, поле задержек и профиль сессии. Считается в
    // своём потоке по окну вокруг позиции воспроизведения — весь клип по всем
    // каналам не влезает в память (план, секция 8).
    void requestDetect();
    const DetectWorker::Result& getDetection() const { return detection; }
    // Публично по той же причине, что и applyAnalysisResult: путь «результат ->
    // строки таблицы и маркеры» проверяется тестом без message loop.
    void applyDetection(DetectWorker::Result result);
    bool isDetectBusy() const { return detectWorker.isBusy(); }
    juce::String getDetectStatus() const { return detectStatus; }
    const beat::doc::SourceDiagnostic& getSourceDiagnostic() const { return detection.source; }
    juce::String getSourceDiagnosticStatus() const { return sourceDiagnosticStatus; }
    // Медиана и разброс по-ударной задержки канала, сэмплы. observations = 0 —
    // событий с этим каналом нет, показывать нечего.
    struct DelaySpread
    {
        double medianSamples = 0.0;
        double spreadSamples = 0.0;
        int observations = 0;
    };
    DelaySpread getDelaySpread(int channel) const;
    FilePlayer& getFilePlayer() { return filePlayer; }
    // Устройство сменило частоту: материал стенда пересчитывается под неё,
    // иначе он играет с чужой скоростью. true — перезагрузили.
    bool reloadBenchForSampleRate();
    juce::String exportAligned(const juce::File& file);
    bool canExportGlide() const;
    juce::String refreshGlidePreview();
    juce::String exportGlide(const juce::File& file);
    juce::String getGlideStatus() const { return glideStatus; }
    // Публично, потому что путь «результат -> параметры» проверяется тестом
    // без message loop; worker зовёт то же самое из handleAsyncUpdate.
    void applyAnalysisResult(const beat::AlignmentEngine::Result& result);
    const beat::AnalysisRing& getAnalysisRing() const { return analysisRing; }
    bool isAnalysisBusy() const { return analysisWorker.isBusy(); }
    bool isFrozen() const;
    juce::String getAnalysisStatus() const { return analysisStatus; }
    float getCoherenceBefore() const { return coherenceBefore; }
    float getCoherenceAfter() const { return coherenceAfter; }

private:
    struct ChannelParams
    {
        std::atomic<float>* delayMs = nullptr;
        std::atomic<float>* polarity = nullptr;
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mute = nullptr;
        std::atomic<float>* solo = nullptr;
        std::atomic<float>* role = nullptr;
        std::atomic<float>* rotatorAmount = nullptr;
        std::atomic<float>* rotatorHz = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* levelDb = nullptr;
    };

    struct GlideRun
    {
        beat::GlideRenderer::Result result;
        juce::AudioBuffer<float> rendered;
        double sampleRate = 0.0;
    };

    static BusesProperties createBusesProperties();

    void updateTransport(int numSamples);
    void handleAsyncUpdate() override;

    void setParameterValue(const juce::String& parameterId, float value);
    float getGlideStrength() const;
    juce::String renderGlide(GlideRun& run, bool previewOnly) const;
    juce::String formatGlideStatus(const juce::String& action,
                                   const beat::GlideRenderer::Result& result) const;
    beat::ChannelRole channelRole(int channel) const;
    juce::String formatSourceDiagnosticStatus(
        const beat::doc::SourceDiagnostic& source) const;
    juce::String refreshGlidePreview(bool notify);

    juce::AudioProcessorValueTreeState parameters;
    beat::AlignmentSnapshot snapshot;
    beat::FractionalDelay delay;
    beat::AllpassRotator rotator;
    std::array<ChannelParams, beat::kMaxChannels> channelParams {};
    std::atomic<float>* abBypassParam = nullptr;
    std::array<std::atomic<float>, beat::kMaxChannels> inputPeak {};
    // Стенд обрабатывается здесь, а не в буфере хоста: каналов кита больше,
    // чем выходов устройства, и в монитор они складываются уже выровненными.
    juce::AudioBuffer<float> benchBuffer;
    beat::ScopeRing scope;
    std::atomic<float>* referenceParam = nullptr;
    std::atomic<float>* maxDistanceParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;
    std::atomic<float>* monoSumParam = nullptr;
    std::atomic<float>* tempoSourceParam = nullptr;
    std::atomic<float>* tempoBpmParam = nullptr;
    std::atomic<float>* gridDivisionParam = nullptr;
    std::atomic<float>* glideStrengthParam = nullptr;
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<double> quartersAtWrite { 0.0 };
    std::atomic<int> hostNumerator { 4 };
    std::atomic<int> hostDenominator { 4 };
    std::atomic<bool> hostHasPosition { false };
    beat::AnalysisRing analysisRing;
    FilePlayer filePlayer;
    beat::AlignmentEngine::Result lastResult;
    AnalysisWorker analysisWorker { analysisRing };
    DetectWorker detectWorker;
    DetectWorker::Result detection;
    juce::String detectStatus { "Load bench material, then Detect" };
    juce::String sourceDiagnosticStatus { "Source: -" };
    juce::String glideStatus { "Run Detect, then Export glide" };
    juce::String analysisStatus { "Press Analyze after playing a few bars" };
    float coherenceBefore = 0.0f;
    float coherenceAfter = 0.0f;
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatEqualizerAudioProcessor)
};
