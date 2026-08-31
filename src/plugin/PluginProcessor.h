#pragma once

#include "AnalysisWorker.h"
#include "Exporter.h"
#include "FilePlayer.h"
#include "Parameters.h"
#include "ScopeRing.h"
#include "dsp/AlignmentSnapshot.h"
#include "dsp/AllpassRotator.h"
#include "dsp/AnalysisRing.h"
#include "dsp/FractionalDelay.h"
#include "dsp/Grid.h"

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

class BeatEqualizerAudioProcessor final : public juce::AudioProcessor,
                                          public juce::ChangeBroadcaster
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
    FilePlayer& getFilePlayer() { return filePlayer; }
    juce::String exportAligned(const juce::File& file);
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
        std::atomic<float>* rotatorAmount = nullptr;
        std::atomic<float>* rotatorHz = nullptr;
    };

    static BusesProperties createBusesProperties();

    void updateTransport(int numSamples);

    void setParameterValue(const juce::String& parameterId, float value);

    juce::AudioProcessorValueTreeState parameters;
    beat::AlignmentSnapshot snapshot;
    beat::FractionalDelay delay;
    beat::AllpassRotator rotator;
    std::array<ChannelParams, beat::kMaxChannels> channelParams {};
    std::atomic<float>* abBypassParam = nullptr;
    std::array<std::atomic<float>, beat::kMaxChannels> inputPeak {};
    beat::ScopeRing scope;
    std::atomic<float>* referenceParam = nullptr;
    std::atomic<float>* maxDistanceParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;
    std::atomic<float>* monoSumParam = nullptr;
    std::atomic<float>* tempoSourceParam = nullptr;
    std::atomic<float>* tempoBpmParam = nullptr;
    std::atomic<float>* gridDivisionParam = nullptr;
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<double> quartersAtWrite { 0.0 };
    std::atomic<int> hostNumerator { 4 };
    std::atomic<int> hostDenominator { 4 };
    std::atomic<bool> hostHasPosition { false };
    beat::AnalysisRing analysisRing;
    FilePlayer filePlayer;
    beat::AlignmentEngine::Result lastResult;
    AnalysisWorker analysisWorker { analysisRing };
    juce::String analysisStatus { "Press Analyze after playing a few bars" };
    float coherenceBefore = 0.0f;
    float coherenceAfter = 0.0f;
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatEqualizerAudioProcessor)
};
