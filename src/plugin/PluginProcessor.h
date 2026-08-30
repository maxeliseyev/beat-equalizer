#pragma once

#include "Parameters.h"
#include "dsp/AlignmentSnapshot.h"
#include "dsp/FractionalDelay.h"

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

private:
    struct ChannelParams
    {
        std::atomic<float>* delayMs = nullptr;
        std::atomic<float>* polarity = nullptr;
        std::atomic<float>* enabled = nullptr;
    };

    static BusesProperties createBusesProperties();

    juce::AudioProcessorValueTreeState parameters;
    beat::AlignmentSnapshot snapshot;
    beat::FractionalDelay delay;
    std::array<ChannelParams, beat::kMaxChannels> channelParams {};
    std::atomic<float>* abBypassParam = nullptr;
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatEqualizerAudioProcessor)
};
