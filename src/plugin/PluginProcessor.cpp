#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "dsp/AnalysisState.h"
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
        channelParams[static_cast<size_t>(i)].mute =
            parameters.getRawParameterValue(beat::channelParamId(i, "mute"));
        channelParams[static_cast<size_t>(i)].solo =
            parameters.getRawParameterValue(beat::channelParamId(i, "solo"));
        channelParams[static_cast<size_t>(i)].rotatorAmount =
            parameters.getRawParameterValue(beat::channelParamId(i, "rotatorAmount"));
        channelParams[static_cast<size_t>(i)].rotatorHz =
            parameters.getRawParameterValue(beat::channelParamId(i, "rotatorHz"));
    }

    abBypassParam = parameters.getRawParameterValue("global.abBypass");
    referenceParam = parameters.getRawParameterValue("global.reference");
    maxDistanceParam = parameters.getRawParameterValue("global.maxDistanceM");
    freezeParam = parameters.getRawParameterValue("global.freeze");
    monoSumParam = parameters.getRawParameterValue("global.monoSum");
    tempoSourceParam = parameters.getRawParameterValue("global.tempoSource");
    tempoBpmParam = parameters.getRawParameterValue("global.tempoBpm");
    gridDivisionParam = parameters.getRawParameterValue("global.gridDivision");

    analysisWorker.onResult = [this] { applyAnalysisResult(analysisWorker.result()); };
    analysisWorker.readWindow = [this](float* dest, int channels, int count)
    {
        // Загруженные файлы важнее живого входа: в Standalone устройство может
        // отдавать два канала, а на стенде лежит весь кит.
        if (filePlayer.hasMaterial())
            return filePlayer.readAnalysisWindow(dest, channels, count);

        return analysisRing.readLast(dest, channels, count);
    };
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

    updateTransport(numSamples);

    filePlayer.fill(buffer, numCh);

    // Анализ смотрит на вход, до задержки и инверсии: буфер пишем до DSP.
    analysisRing.write(buffer.getArrayOfReadPointers(), numCh, numSamples);

    const bool bypass = abBypassParam != nullptr && abBypassParam->load() >= 0.5f;
    const float sr = static_cast<float>(currentSampleRate);

    float applied[beat::kMaxChannels] {};
    bool enabled[beat::kMaxChannels] {};
    bool invert[beat::kMaxChannels] {};
    bool audible[beat::kMaxChannels] {};
    float maxApplied = 0.0f;
    bool anySolo = false;

    for (int ch = 0; ch < numCh; ++ch)
    {
        const auto& params = channelParams[static_cast<size_t>(ch)];
        if (params.solo != nullptr && params.solo->load() >= 0.5f)
            anySolo = true;
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        const auto& params = channelParams[static_cast<size_t>(ch)];
        enabled[ch] = params.enabled == nullptr || params.enabled->load() >= 0.5f;

        const bool muted = params.mute != nullptr && params.mute->load() >= 0.5f;
        const bool soloed = params.solo != nullptr && params.solo->load() >= 0.5f;
        audible[ch] = !muted && (!anySolo || soloed);

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

    int enabledCount = 0;
    for (int ch = 0; ch < numCh; ++ch)
        enabledCount += (enabled[ch] && audible[ch]) ? 1 : 0;

    const bool monoSum = monoSumParam != nullptr && monoSumParam->load() >= 0.5f && numCh >= 2
                         && enabledCount > 0;
    const float monoGain = (enabledCount > 0) ? 1.0f / static_cast<float>(enabledCount) : 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = buffer.getSample(ch, n);
            blockPeak[ch] = juce::jmax(blockPeak[ch], std::abs(x));
            const float y = rotator.processSample(ch, delay.processSample(ch, x));

            // Осциллограф показывает выровненный канал даже под mute: глушим
            // только выход, картинку глушить незачем.
            scopeSample[ch] = y;
            buffer.setSample(ch, n, audible[ch] ? y : 0.0f);
        }

        if (monoSum)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                if (enabled[ch] && audible[ch])
                    mono += scopeSample[ch];

            mono *= monoGain;

            // Мониторинг: моно уходит только на 1-2, остальные стемы идут
            // выровненными дальше, разводка кита не ломается.
            buffer.setSample(0, n, mono);
            buffer.setSample(1, n, mono);
            scopeSample[0] = mono;
            scopeSample[1] = mono;
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

void BeatEqualizerAudioProcessor::updateTransport(int numSamples)
{
    double bpm = 0.0;
    int numerator = 4;
    int denominator = 4;
    double quarters = 0.0;
    bool hasPosition = false;

    if (auto* head = getPlayHead())
    {
        if (const auto position = head->getPosition())
        {
            if (const auto hostTempo = position->getBpm())
                bpm = *hostTempo;

            if (const auto signature = position->getTimeSignature())
            {
                numerator = signature->numerator;
                denominator = signature->denominator;
            }

            if (const auto ppq = position->getPpqPosition())
            {
                quarters = *ppq;
                hasPosition = true;
            }
        }
    }

    hostBpm.store(bpm, std::memory_order_relaxed);
    hostNumerator.store(numerator, std::memory_order_relaxed);
    hostDenominator.store(denominator, std::memory_order_relaxed);
    hostHasPosition.store(hasPosition, std::memory_order_relaxed);

    // Хост отдаёт позицию на начало блока, а осциллограф показывает его конец.
    if (hasPosition && bpm > 0.0 && currentSampleRate > 0.0)
        quarters += static_cast<double>(numSamples) / currentSampleRate
                    * beat::grid::quartersPerSecond(bpm);

    quartersAtWrite.store(quarters, std::memory_order_relaxed);
}

BeatEqualizerAudioProcessor::TransportInfo BeatEqualizerAudioProcessor::getTransport() const
{
    TransportInfo info;
    info.hostBpm = hostBpm.load(std::memory_order_relaxed);

    const bool wantsHost = tempoSourceParam == nullptr
                           || juce::roundToInt(tempoSourceParam->load()) == 0;
    info.fromHost = wantsHost && info.hostBpm > 0.0;
    info.bpm = info.fromHost
                   ? info.hostBpm
                   : (tempoBpmParam != nullptr ? tempoBpmParam->load() : beat::kDefaultTempoBpm);

    info.numerator = hostNumerator.load(std::memory_order_relaxed);
    info.denominator = hostDenominator.load(std::memory_order_relaxed);
    info.hasPosition = hostHasPosition.load(std::memory_order_relaxed);
    info.quartersAtWrite = quartersAtWrite.load(std::memory_order_relaxed);

    const int division = (gridDivisionParam != nullptr)
                             ? juce::roundToInt(gridDivisionParam->load())
                             : 0;
    info.division = static_cast<beat::grid::Division>(
        juce::jlimit(0, beat::grid::kDivisionCount - 1, division));

    return info;
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

    const int ringLength = beat::AnalysisRing::capacityForSampleRate(currentSampleRate);
    if (filePlayer.hasMaterial())
        analysisWorker.prepare(juce::jmin(filePlayer.numChannels(), beat::kMaxChannels),
                               juce::jmin(filePlayer.numSamples(), ringLength));
    else
        analysisWorker.prepare(juce::jmin(getTotalNumInputChannels(), beat::kMaxChannels),
                               ringLength);

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
    lastResult = result;

    analysisStatus = juce::String(result.numChannels) + " ch aligned, ref Ch "
                     + juce::String(result.reference + 1) + ", "
                     + juce::String(result.framesLoud) + " frames";
    sendChangeMessage();
}

juce::AudioProcessorEditor* BeatEqualizerAudioProcessor::createEditor()
{
    return new BeatEqualizerAudioProcessorEditor(*this);
}

juce::String BeatEqualizerAudioProcessor::exportAligned(const juce::File& file)
{
    if (!filePlayer.hasMaterial())
        return "Load files before exporting";

    const int channels = juce::jmin(filePlayer.numChannels(), beat::kMaxChannels);
    const float samplesPerMs = static_cast<float>(currentSampleRate) * 0.001f;

    std::vector<beat::exporter::ChannelSettings> settings;
    settings.reserve(static_cast<size_t>(channels));

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto& params = channelParams[static_cast<size_t>(ch)];
        const bool enabled = params.enabled == nullptr || params.enabled->load() >= 0.5f;

        beat::exporter::ChannelSettings channelSettings;
        if (enabled)
        {
            channelSettings.delaySamples =
                (params.delayMs != nullptr) ? params.delayMs->load() * samplesPerMs : 0.0f;
            channelSettings.invert =
                params.polarity != nullptr
                && juce::roundToInt(params.polarity->load())
                       == static_cast<int>(beat::PolarityMode::invert);
            channelSettings.rotatorHz = (params.rotatorHz != nullptr) ? params.rotatorHz->load()
                                                                      : beat::kDefaultRotatorHz;
            channelSettings.rotatorAmount =
                (params.rotatorAmount != nullptr) ? params.rotatorAmount->load() : 0.0f;
        }

        settings.push_back(channelSettings);
    }

    juce::AudioBuffer<float> rendered;
    beat::exporter::renderAligned(filePlayer.getBuffer(), currentSampleRate, settings, rendered);

    if (!beat::exporter::writeWav(file, rendered, currentSampleRate))
        return "Could not write " + file.getFileName();

    return {};
}

void BeatEqualizerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.removeChild(state.getChildWithName(beat::kAnalysisStateTag), nullptr);

    // Параметры и так восстановят задержки; блоб хранит сами оценки, чтобы
    // после открытия проекта было видно уверенность и когерентность.
    if (lastResult.status == beat::AnalysisStatus::ok)
    {
        const auto blob = beat::serializeAnalysis(lastResult, currentSampleRate);
        juce::ValueTree node(beat::kAnalysisStateTag);
        node.setProperty("data", juce::var(juce::MemoryBlock(blob.data(), blob.size())), nullptr);
        state.appendChild(node, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void BeatEqualizerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml(*xml);
    parameters.replaceState(state);

    const auto node = state.getChildWithName(beat::kAnalysisStateTag);
    if (!node.isValid())
        return;

    const auto* blob = node.getProperty("data").getBinaryData();
    if (blob == nullptr)
        return;

    beat::AlignmentEngine::Result restored;
    double storedRate = 0.0;
    const int channels = juce::jmin(getTotalNumInputChannels(), beat::kMaxChannels);

    if (!beat::deserializeAnalysis(static_cast<const std::uint8_t*>(blob->getData()),
                                   blob->getSize(),
                                   channels,
                                   restored,
                                   storedRate))
    {
        analysisStatus = "Saved estimates skipped: channel count changed";
        return;
    }

    lastResult = restored;
    coherenceBefore = restored.coherenceBefore;
    coherenceAfter = restored.coherenceAfter;
    analysisStatus = juce::String(restored.numChannels) + " ch restored, ref Ch "
                     + juce::String(restored.reference + 1);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BeatEqualizerAudioProcessor();
}
