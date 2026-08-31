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
        channelParams[static_cast<size_t>(i)].rotatorAmount =
            parameters.getRawParameterValue(beat::channelParamId(i, "rotatorAmount"));
        channelParams[static_cast<size_t>(i)].rotatorHz =
            parameters.getRawParameterValue(beat::channelParamId(i, "rotatorHz"));
    }

    abBypassParam = parameters.getRawParameterValue("global.abBypass");
    referenceParam = parameters.getRawParameterValue("global.reference");
    maxDistanceParam = parameters.getRawParameterValue("global.maxDistanceM");
    freezeParam = parameters.getRawParameterValue("global.freeze");

    analysisWorker.onResult = [this] { applyAnalysisResult(analysisWorker.result()); };
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
    rotator.prepare(sampleRate, beat::kMaxChannels);
    scope.prepare(beat::ScopeRing::capacityForSampleRate(sampleRate));

    const int analysisChannels = juce::jmin(getTotalNumInputChannels(), beat::kMaxChannels);
    const int analysisLength = beat::AnalysisRing::capacityForSampleRate(sampleRate);
    analysisRing.prepare(analysisChannels, analysisLength);
    analysisWorker.prepare(analysisChannels, analysisLength);
    setLatencySamples(beat::LatencyModel::reportedLatency(0.0f));
}

void BeatEqualizerAudioProcessor::releaseResources()
{
    delay.reset();
    rotator.reset();
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

    // Анализ смотрит на вход, до задержки и инверсии: буфер пишем до DSP.
    analysisRing.write(buffer.getArrayOfReadPointers(), numCh, numSamples);

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
        const auto& params = channelParams[static_cast<size_t>(ch)];
        const float appliedDelay = (bypass || !enabled[ch]) ? maxApplied : applied[ch];
        delay.setAppliedDelaySamples(ch, appliedDelay);
        delay.setInvert(ch, !bypass && invert[ch]);

        const float amount = (bypass || !enabled[ch] || params.rotatorAmount == nullptr)
                                 ? 0.0f
                                 : params.rotatorAmount->load();
        const float hz = (params.rotatorHz != nullptr) ? params.rotatorHz->load()
                                                       : beat::kDefaultRotatorHz;
        rotator.setRotation(ch, hz, amount);
    }

    float blockPeak[beat::kMaxChannels] {};
    float scopeSample[beat::kMaxChannels] {};

    for (int n = 0; n < numSamples; ++n)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = buffer.getSample(ch, n);
            blockPeak[ch] = juce::jmax(blockPeak[ch], std::abs(x));
            const float y = rotator.processSample(ch, delay.processSample(ch, x));
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
    analysisRing.reset();
    sendChangeMessage();
}

bool BeatEqualizerAudioProcessor::isFrozen() const
{
    return freezeParam != nullptr && freezeParam->load() >= 0.5f;
}

void BeatEqualizerAudioProcessor::setParameterValue(const juce::String& parameterId, float value)
{
    if (auto* parameter = parameters.getParameter(parameterId))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void BeatEqualizerAudioProcessor::requestAnalyze()
{
    beat::AnalysisRequest request;
    request.sampleRate = currentSampleRate;
    request.maxDistanceM = (maxDistanceParam != nullptr) ? maxDistanceParam->load()
                                                         : beat::kDefaultMaxDistanceM;
    request.reference = getReferenceChannelIndex();

    if (analysisWorker.request(request))
    {
        analysisStatus = "Analyzing...";
        sendChangeMessage();
    }
}

void BeatEqualizerAudioProcessor::applyAnalysisResult(const beat::AlignmentEngine::Result& result)
{
    switch (result.status)
    {
        case beat::AnalysisStatus::ok:
            break;
        case beat::AnalysisStatus::tooQuiet:
            analysisStatus = "Too quiet - play the kit, then Analyze";
            sendChangeMessage();
            return;
        case beat::AnalysisStatus::notEnoughData:
            analysisStatus = "Not enough audio yet - play a few seconds";
            sendChangeMessage();
            return;
        case beat::AnalysisStatus::idle:
        case beat::AnalysisStatus::badRequest:
            analysisStatus = "Analysis needs at least two channels";
            sendChangeMessage();
            return;
    }

    if (isFrozen())
    {
        analysisStatus = "Frozen - estimates found but not applied";
        sendChangeMessage();
        return;
    }

    snapshot = result.snapshot;

    const float msPerSample = (currentSampleRate > 0.0)
                                  ? static_cast<float>(1000.0 / currentSampleRate)
                                  : 0.0f;

    for (int ch = 0; ch < result.numChannels; ++ch)
    {
        const float delayMs = juce::jlimit(0.0f,
                                           beat::kMaxDelayMs,
                                           snapshot.delaySamples[ch] * msPerSample);
        setParameterValue(beat::channelParamId(ch, "delayMs"), delayMs);

        // Полярность трогаем только там, где оценка действительно была:
        // на молчавшем канале выбор пользователя важнее догадки.
        if (!result.channels[static_cast<size_t>(ch)].valid)
            continue;

        const auto polarity = snapshot.invert[ch] ? beat::PolarityMode::invert
                                                  : beat::PolarityMode::positive;
        setParameterValue(beat::channelParamId(ch, "polarity"),
                          static_cast<float>(static_cast<int>(polarity)));

        const auto& estimate = result.channels[static_cast<size_t>(ch)];
        setParameterValue(beat::channelParamId(ch, "rotatorHz"), estimate.rotatorHz);
        setParameterValue(beat::channelParamId(ch, "rotatorAmount"), estimate.rotatorAmount);
    }

    coherenceBefore = result.coherenceBefore;
    coherenceAfter = result.coherenceAfter;

    analysisStatus = juce::String(result.numChannels) + " ch aligned, ref Ch "
                     + juce::String(result.reference + 1) + ", "
                     + juce::String(result.framesLoud) + " frames";
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
