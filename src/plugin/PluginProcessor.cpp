#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "dsp/Constants.h"
#include "dsp/LatencyModel.h"

#include <cmath>

BeatEqualizerAudioProcessor::BeatEqualizerAudioProcessor()
    : AudioProcessor(createBusesProperties()),
      parameters(*this, nullptr, "BeatEqualizer", beat::createParameterLayout()),
      snapshot(beat::AlignmentSnapshot::identity(2))
{
    for (int i = 0; i < beat::kMaxChannels; ++i)
    {
        channelParams[static_cast<size_t>(i)].delayMs =
            parameters.getRawParameterValue(beat::channelParamId(i, "delayMs"));
        channelParams[static_cast<size_t>(i)].polarity =
            parameters.getRawParameterValue(beat::channelParamId(i, "polarity"));
        channelParams[static_cast<size_t>(i)].enabled =
            parameters.getRawParameterValue(beat::channelParamId(i, "enabled"));
    }

    abBypassParam = parameters.getRawParameterValue("global.abBypass");
    referenceParam = parameters.getRawParameterValue("global.reference");
}

juce::AudioProcessor::BusesProperties BeatEqualizerAudioProcessor::createBusesProperties()
{
    return BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

void BeatEqualizerAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    snapshot = beat::AlignmentSnapshot::identity(getTotalNumInputChannels());
    delay.prepare(sampleRate, beat::kMaxChannels);
    scope.reset();
    setLatencySamples(beat::LatencyModel::reportedLatency(0.0f));
}

void BeatEqualizerAudioProcessor::releaseResources()
{
    delay.reset();
}

bool BeatEqualizerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled())
        return false;

    if (in != out)
        return false;

    if (layouts.inputBuses.size() != 1 || layouts.outputBuses.size() != 1)
        return false;

    const int channels = in.size();
    return channels >= beat::kMinChannels && channels <= beat::kMaxChannels;
}

void BeatEqualizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;

    const int numInput = getTotalNumInputChannels();
    const int numOutput = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(numInput, beat::kMaxChannels);

    for (int channel = numInput; channel < numOutput; ++channel)
        buffer.clear(channel, 0, numSamples);

    const bool bypass = abBypassParam != nullptr && abBypassParam->load() >= 0.5f;
    const float sr = static_cast<float>(currentSampleRate);

    float applied[beat::kMaxChannels] {};
    bool enabled[beat::kMaxChannels] {};
    bool invert[beat::kMaxChannels] {};
    float maxApplied = 0.0f;

    for (int ch = 0; ch < numCh; ++ch)
    {
        const auto& params = channelParams[static_cast<size_t>(ch)];
        enabled[ch] = params.enabled == nullptr || params.enabled->load() >= 0.5f;

        const float delayMs = (params.delayMs != nullptr) ? params.delayMs->load() : 0.0f;
        applied[ch] = enabled[ch] ? delayMs * 0.001f * sr : 0.0f;
        if (applied[ch] < 0.0f)
            applied[ch] = 0.0f;

        const int polarity = (params.polarity != nullptr)
                                 ? juce::roundToInt(params.polarity->load())
                                 : 0;
        invert[ch] = enabled[ch] && polarity == static_cast<int>(beat::PolarityMode::invert);

        maxApplied = juce::jmax(maxApplied, applied[ch]);
    }

    const int latency = beat::LatencyModel::reportedLatency(maxApplied);
    if (latency != getLatencySamples())
        setLatencySamples(latency);

    for (int ch = 0; ch < numCh; ++ch)
    {
        const float appliedDelay = (bypass || !enabled[ch]) ? maxApplied : applied[ch];
        delay.setAppliedDelaySamples(ch, appliedDelay);
        delay.setInvert(ch, !bypass && invert[ch]);
    }

    float blockPeak[beat::kMaxChannels] {};
    float scopeSample[beat::kMaxChannels] {};

    for (int n = 0; n < numSamples; ++n)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = buffer.getSample(ch, n);
            blockPeak[ch] = juce::jmax(blockPeak[ch], std::abs(x));
            const float y = delay.processSample(ch, x);
            scopeSample[ch] = y;
            buffer.setSample(ch, n, y);
        }
        scope.push(numCh, scopeSample);
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        const float decayed = inputPeak[static_cast<size_t>(ch)].load(std::memory_order_relaxed) * 0.88f;
        inputPeak[static_cast<size_t>(ch)].store(juce::jmax(blockPeak[ch], decayed),
                                                 std::memory_order_relaxed);
    }
}

float BeatEqualizerAudioProcessor::getInputPeak(int channel) const
{
    if (channel < 0 || channel >= beat::kMaxChannels)
        return 0.0f;

    return inputPeak[static_cast<size_t>(channel)].load(std::memory_order_relaxed);
}

int BeatEqualizerAudioProcessor::getReferenceChannelIndex() const
{
    const int raw = (referenceParam != nullptr) ? juce::roundToInt(referenceParam->load()) : 1;
    return juce::jlimit(0, beat::kMaxChannels - 1, raw - 1);
}

void BeatEqualizerAudioProcessor::numChannelsChanged()
{
    snapshot = beat::AlignmentSnapshot::identity(getTotalNumInputChannels());
    sendChangeMessage();
}

juce::AudioProcessorEditor* BeatEqualizerAudioProcessor::createEditor()
{
    return new BeatEqualizerAudioProcessorEditor(*this);
}

void BeatEqualizerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void BeatEqualizerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BeatEqualizerAudioProcessor();
}
