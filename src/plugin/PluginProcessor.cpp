#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "doc/ProtectedZone.h"
#include "dsp/AnalysisState.h"
#include "dsp/Constants.h"
#include "dsp/GlideRenderer.h"
#include "dsp/LatencyModel.h"

#include <cmath>

namespace
{
juce::String percent(float value)
{
    return juce::String(juce::roundToInt(100.0f * value)) + "%";
}

void copyIntoPadded(const juce::AudioBuffer<float>& source,
                    int channels,
                    int sourceStart,
                    int samples,
                    juce::AudioBuffer<float>& padded)
{
    padded.clear();
    for (int ch = 0; ch < channels; ++ch)
        padded.copyFrom(ch, 0, source, ch, sourceStart, samples);
}
} // namespace

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
        channelParams[static_cast<size_t>(i)].pan =
            parameters.getRawParameterValue(beat::channelParamId(i, "pan"));
        channelParams[static_cast<size_t>(i)].levelDb =
            parameters.getRawParameterValue(beat::channelParamId(i, "levelDb"));
    }

    abBypassParam = parameters.getRawParameterValue("global.abBypass");
    referenceParam = parameters.getRawParameterValue("global.reference");
    maxDistanceParam = parameters.getRawParameterValue("global.maxDistanceM");
    freezeParam = parameters.getRawParameterValue("global.freeze");
    monoSumParam = parameters.getRawParameterValue("global.monoSum");
    tempoSourceParam = parameters.getRawParameterValue("global.tempoSource");
    tempoBpmParam = parameters.getRawParameterValue("global.tempoBpm");
    gridDivisionParam = parameters.getRawParameterValue("global.gridDivision");
    glideStrengthParam = parameters.getRawParameterValue("global.glideStrength");

    analysisWorker.onResult = [this] { applyAnalysisResult(analysisWorker.result()); };
    analysisWorker.readWindow = [this](float* dest, int channels, int count)
    {
        // Загруженные файлы важнее живого входа: в Standalone устройство может
        // отдавать два канала, а на стенде лежит весь кит.
        if (filePlayer.hasMaterial())
            return filePlayer.readAnalysisWindow(dest, channels, count);

        return analysisRing.readLast(dest, channels, count);
    };

    detectWorker.currentGeneration = [this] { return filePlayer.getGeneration(); };
    detectWorker.onResult = [this] { applyDetection(detectWorker.result()); };
}

juce::AudioProcessor::BusesProperties BeatEqualizerAudioProcessor::createBusesProperties()
{
    return BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

void BeatEqualizerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    // Блок стенда аллоцируется здесь и только здесь: в processBlock памяти не
    // просят. Запас — на хост, который дал блок больше заявленного.
    benchBuffer.setSize(beat::kMaxChannels, juce::jmax(samplesPerBlock, 2048), false, true, true);
    snapshot = beat::AlignmentSnapshot::identity(getTotalNumInputChannels());
    delay.prepare(sampleRate, beat::kMaxChannels);
    rotator.prepare(sampleRate, beat::kMaxChannels);
    scope.prepare(beat::ScopeRing::capacityForSampleRate(sampleRate));

    const int analysisChannels = juce::jmin(getTotalNumInputChannels(), beat::kMaxChannels);
    const int analysisLength = beat::AnalysisRing::capacityForSampleRate(sampleRate);
    analysisRing.prepare(analysisChannels, analysisLength);
    analysisWorker.prepare(analysisChannels, analysisLength);
    setLatencySamples(beat::LatencyModel::reportedLatency(0.0f));

    // Частоту устройства могли поменять в диалоге Audio…: материал стенда
    // пересчитывается в message thread, здесь только заявка.
    if (filePlayer.hasMaterial() && std::abs(filePlayer.getSampleRate() - sampleRate) > 1.0)
        triggerAsyncUpdate();
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
    for (int channel = numInput; channel < numOutput; ++channel)
        buffer.clear(channel, 0, numSamples);

    updateTransport(numSamples);

    // Стенд обрабатывается в своём буфере: кит шире, чем выходы устройства, и в
    // одноимённые выходы его класть нельзя — на стерео-карте это звучит как
    // «нечётные слева, чётные справа». Внутри хоста этого пути нет вовсе, там
    // N-out passthrough и разводка стемов не ломается.
    const int benchChannels = juce::jmin(filePlayer.numChannels(), beat::kMaxChannels);
    const bool bench = filePlayer.hasMaterial() && benchChannels > 0
                       && numSamples <= benchBuffer.getNumSamples();

    juce::AudioBuffer<float> benchView(benchBuffer.getArrayOfWritePointers(),
                                       bench ? benchChannels : 0,
                                       bench ? numSamples : 0);
    // На паузе fill() возвращает false и буфер не трогает — там остаётся
    // прошлый блок. Без очистки монитор гонял бы его по кругу: один и тот же
    // кусок в сорок миллисекунд бесконечно.
    if (bench && !filePlayer.fill(benchView, benchChannels))
        benchView.clear();

    const int numCh = bench ? benchChannels : juce::jmin(numInput, beat::kMaxChannels);
    auto& source = bench ? benchView : buffer;

    // Анализ смотрит на вход, до задержки и инверсии: буфер пишем до DSP.
    analysisRing.write(source.getArrayOfReadPointers(),
                       juce::jmin(numCh, analysisRing.numChannels()),
                       numSamples);

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

    const bool mono = monoSumParam != nullptr && monoSumParam->load() >= 0.5f;
    const bool monoSum = mono && !bench && numCh >= 2 && enabledCount > 0;
    const float monoGain = (enabledCount > 0) ? 1.0f / static_cast<float>(enabledCount) : 0.0f;

    float monitorLeft[beat::kMaxChannels] {};
    float monitorRight[beat::kMaxChannels] {};

    if (bench)
    {
        // Делитель — число загруженных каналов, а не слышимых сейчас. Иначе
        // Solo на одном канале поднимал бы его в numCh раз: на ките из
        // шестнадцати микрофонов это +24 дБ на ровном месте. Solo и Mute меняют,
        // что слышно, а не громкость того, что осталось.
        const float norm = 1.0f / static_cast<float>(juce::jmax(1, numCh));
        const float quarterPi = 0.25f * juce::MathConstants<float>::pi;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto& params = channelParams[static_cast<size_t>(ch)];
            const float levelDb = (params.levelDb != nullptr) ? params.levelDb->load() : 0.0f;
            const float gain =
                norm * juce::Decibels::decibelsToGain(levelDb, beat::kMinMonitorLevelDb);
            const float pan = (params.pan != nullptr)
                                  ? juce::jlimit(-1.0f, 1.0f, params.pan->load())
                                  : 0.0f;

            // Равная мощность: центр отдаёт по -3 дБ в каждую сторону. Mono Sum
            // — та же сумма со сведёнными в центр панорамами. Выключенный канал
            // из суммы не выкидываем: enabled — про выравнивание, не про
            // слышимость.
            const float angle = mono ? quarterPi : quarterPi * (pan + 1.0f);
            monitorLeft[ch] = audible[ch] ? gain * std::cos(angle) : 0.0f;
            monitorRight[ch] = audible[ch] ? gain * std::sin(angle) : 0.0f;
        }

        buffer.clear();
    }

    const int monitorChannels = juce::jmin(numOutput, 2);

    for (int n = 0; n < numSamples; ++n)
    {
        float left = 0.0f;
        float right = 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = source.getSample(ch, n);
            blockPeak[ch] = juce::jmax(blockPeak[ch], std::abs(x));
            const float y = rotator.processSample(ch, delay.processSample(ch, x));

            // Осциллограф показывает выровненный канал даже под mute: глушим
            // только выход, картинку глушить незачем.
            scopeSample[ch] = y;

            if (bench)
            {
                left += y * monitorLeft[ch];
                right += y * monitorRight[ch];
            }
            else
            {
                buffer.setSample(ch, n, audible[ch] ? y : 0.0f);
            }
        }

        if (bench)
        {
            for (int ch = 0; ch < monitorChannels; ++ch)
                buffer.setSample(ch, n, (ch == 0) ? left : right);
        }
        else if (monoSum)
        {
            float monoSample = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                if (enabled[ch] && audible[ch])
                    monoSample += scopeSample[ch];

            monoSample *= monoGain;

            // Мониторинг: моно уходит только на 1-2, остальные стемы идут
            // выровненными дальше, разводка кита не ломается.
            buffer.setSample(0, n, monoSample);
            buffer.setSample(1, n, monoSample);
            scopeSample[0] = monoSample;
            scopeSample[1] = monoSample;
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

void BeatEqualizerAudioProcessor::handleAsyncUpdate()
{
    if (reloadBenchForSampleRate())
        sendChangeMessage();
}

bool BeatEqualizerAudioProcessor::reloadBenchForSampleRate()
{
    if (!filePlayer.hasMaterial() || currentSampleRate <= 0.0)
        return false;

    if (std::abs(filePlayer.getSampleRate() - currentSampleRate) <= 1.0)
        return false;

    const auto files = filePlayer.getFiles();
    const bool wasPlaying = filePlayer.isPlaying();
    const auto error = filePlayer.load(files, currentSampleRate);

    if (error.isNotEmpty())
    {
        analysisStatus = error;
        return false;
    }

    filePlayer.setPlaying(wasPlaying);
    return true;
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

void BeatEqualizerAudioProcessor::requestDetect()
{
    if (!filePlayer.hasMaterial())
    {
        detectStatus = "Detect works on bench material - load files first";
        glideStatus = "Load files before exporting";
        sendChangeMessage();
        return;
    }

    const double rate = filePlayer.getSampleRate() > 0.0 ? filePlayer.getSampleRate()
                                                         : currentSampleRate;
    const int span = juce::jmin(filePlayer.numSamples(),
                                juce::roundToInt(beat::kDetectSeconds * rate));

    // Окно кончается на позиции воспроизведения: смотрят обычно то, что только
    // что услышали.
    const int end = juce::jlimit(span, filePlayer.numSamples(),
                                 juce::jmax(span, filePlayer.getPosition()));

    DetectWorker::Request request;
    request.clip = &filePlayer.getBuffer();
    request.generation = filePlayer.getGeneration();
    request.sampleRate = rate;
    request.reference = getReferenceChannelIndex();
    request.from = end - span;
    request.length = span;

    if (detectWorker.request(request))
    {
        detectStatus = "Detecting...";
        glideStatus = "Detecting...";
        sendChangeMessage();
    }
}

void BeatEqualizerAudioProcessor::applyDetection(DetectWorker::Result result)
{
    detection = std::move(result);

    if (!detection.valid)
    {
        detectStatus = "Detect found nothing to measure";
        glideStatus = "Run Detect before Export glide";
    }
    else
    {
        // Строка отчёта короткая, но каждое число в ней отвечает на свой
        // вопрос: сколько ударов нашли, в скольких микрофонах их подтвердили и
        // сколько задержек калибровка согласилась знать.
        detectStatus = juce::String(detection.document.events().size()) + " hits, "
                       + juce::String(detection.match.observations) + " obs, "
                       + juce::String(detection.calibration.known) + " delays";
        refreshGlidePreview(false);
    }

    sendChangeMessage();
}

BeatEqualizerAudioProcessor::DelaySpread
BeatEqualizerAudioProcessor::getDelaySpread(int channel) const
{
    DelaySpread spread;

    if (!detection.valid || channel < 0 || channel >= beat::kMaxChannels)
        return spread;

    std::vector<double> delays;
    delays.reserve(detection.document.events().size());
    for (const auto& event : detection.document.events())
        if (event.channels[static_cast<size_t>(channel)].present
            && detection.document.delays().has(event.id, channel))
            delays.push_back(detection.document.delays().raw(event.id, channel));

    if (delays.empty())
        return spread;

    std::sort(delays.begin(), delays.end());
    spread.medianSamples = delays[delays.size() / 2];

    // Разброс — медиана отклонений: по нему и видно, отличается ли по-ударная
    // задержка от статической или это одно и то же число с шумом.
    std::vector<double> deviations;
    deviations.reserve(delays.size());
    for (double value : delays)
        deviations.push_back(std::abs(value - spread.medianSamples));

    std::sort(deviations.begin(), deviations.end());
    spread.spreadSamples = deviations[deviations.size() / 2];
    spread.observations = static_cast<int>(delays.size());
    return spread;
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

bool BeatEqualizerAudioProcessor::canExportGlide() const
{
    return filePlayer.hasMaterial() && detection.valid
           && detection.generation == filePlayer.getGeneration() && detection.sampleRate > 0.0
           && !detection.document.events().empty();
}

float BeatEqualizerAudioProcessor::getGlideStrength() const
{
    return juce::jlimit(0.0f,
                        1.0f,
                        glideStrengthParam != nullptr ? glideStrengthParam->load() : 1.0f);
}

juce::String BeatEqualizerAudioProcessor::renderGlide(GlideRun& run, bool previewOnly) const
{
    run.result = {};
    run.rendered.setSize(0, 0);
    run.sampleRate = 0.0;

    if (!filePlayer.hasMaterial())
        return "Load files before exporting";

    if (!canExportGlide())
        return "Run Detect before Export glide";

    const int channels = juce::jmin(filePlayer.numChannels(), beat::kMaxChannels);
    const int sourceSamples = filePlayer.numSamples();
    const double rate = filePlayer.getSampleRate() > 0.0 ? filePlayer.getSampleRate()
                                                         : currentSampleRate;
    if (channels <= 0 || sourceSamples <= 0 || rate <= 0.0)
        return "Loaded material is not exportable";

    const int sourceStart = previewOnly
                                ? juce::jlimit(0, sourceSamples - 1, detection.from)
                                : 0;
    const int requestedSamples = previewOnly ? detection.length : sourceSamples;
    const int sourceCount = juce::jlimit(0, sourceSamples - sourceStart, requestedSamples);
    if (sourceCount <= 0)
        return "Detect window is not exportable";

    const float samplesPerMs = static_cast<float>(rate) * 0.001f;
    std::vector<beat::exporter::ChannelSettings> settings;
    settings.reserve(static_cast<size_t>(channels));

    std::array<bool, beat::kMaxChannels> enabled {};
    float maxDelay = 0.0f;

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto& params = channelParams[static_cast<size_t>(ch)];
        enabled[static_cast<size_t>(ch)] =
            params.enabled == nullptr || params.enabled->load() >= 0.5f;

        beat::exporter::ChannelSettings channelSettings;
        if (enabled[static_cast<size_t>(ch)])
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

        maxDelay = std::max(maxDelay, channelSettings.delaySamples);
        settings.push_back(channelSettings);
    }

    std::vector<beat::GlideRenderer::EventDelay> events;
    events.reserve(detection.document.events().size());

    const double firstSample = static_cast<double>(sourceStart);
    const double lastSample = firstSample + static_cast<double>(sourceCount);
    for (const auto& event : detection.document.events())
    {
        if (!std::isfinite(event.timeSamples) || event.timeSamples < 0.0
            || event.timeSamples >= static_cast<double>(sourceSamples))
            continue;

        if (event.timeSamples < firstSample || event.timeSamples >= lastSample)
            continue;

        beat::GlideRenderer::EventDelay glide;
        glide.timeSamples = event.timeSamples - firstSample;
        glide.referenceChannel = juce::jlimit(0, channels - 1, event.referenceChannel);

        const auto zone = beat::doc::protectedZone(event, rate);
        glide.protectUntilSamples = zone.empty() ? glide.timeSamples : zone.end - firstSample;

        bool anyDelay = false;
        for (int ch = 0; ch < channels; ++ch)
        {
            const auto index = static_cast<size_t>(ch);
            if (!enabled[index] || !detection.document.delays().has(event.id, ch))
                continue;

            const float delaySamples =
                static_cast<float>(detection.document.delays().applied(event.id, ch));
            glide.setDelay(ch, delaySamples);
            maxDelay = std::max(maxDelay, delaySamples);
            anyDelay = true;
        }

        if (anyDelay)
            events.push_back(glide);
    }

    if (events.empty())
        return "Detect found no per-hit delays to export";

    const float strength = getGlideStrength();
    const int tail = strength > 0.0f
                         ? static_cast<int>(std::ceil(maxDelay * strength))
                               + beat::kInterpolatorLatencySamples
                         : 0;
    const int renderSamples = sourceCount + tail;

    juce::AudioBuffer<float> padded(channels, renderSamples);
    copyIntoPadded(filePlayer.getBuffer(), channels, sourceStart, sourceCount, padded);

    run.rendered.setSize(channels, renderSamples);
    run.rendered.clear();

    std::vector<const float*> input(static_cast<size_t>(channels));
    std::vector<float*> output(static_cast<size_t>(channels));
    for (int ch = 0; ch < channels; ++ch)
    {
        input[static_cast<size_t>(ch)] = padded.getReadPointer(ch);
        output[static_cast<size_t>(ch)] = run.rendered.getWritePointer(ch);
    }

    beat::GlideRenderer::Options options;
    options.sampleRate = rate;
    options.numChannels = channels;
    options.numSamples = renderSamples;
    options.strength = strength;
    for (int ch = 0; ch < channels; ++ch)
        options.baseDelaySamples[static_cast<size_t>(ch)] =
            settings[static_cast<size_t>(ch)].delaySamples;

    beat::GlideRenderer renderer;
    run.result = renderer.render(input.data(), output.data(), options, events);
    run.sampleRate = rate;

    if (previewOnly)
        return {};

    beat::AllpassRotator exportRotator;
    exportRotator.prepare(rate, channels);
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto& channelSettings = settings[static_cast<size_t>(ch)];
        exportRotator.setRotation(ch, channelSettings.rotatorHz, channelSettings.rotatorAmount);
    }
    exportRotator.snapToTargets();

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto& channelSettings = settings[static_cast<size_t>(ch)];
        float* samples = run.rendered.getWritePointer(ch);
        for (int i = 0; i < renderSamples; ++i)
        {
            const float x = channelSettings.invert ? -samples[i] : samples[i];
            samples[i] = exportRotator.processSample(ch, x);
        }
    }

    return {};
}

juce::String BeatEqualizerAudioProcessor::formatGlideStatus(
    const juce::String& action,
    const beat::GlideRenderer::Result& result) const
{
    double before = 0.0;
    double after = 0.0;
    int measured = 0;
    for (const auto& metric : result.events)
    {
        if (metric.channelsMeasured <= 0)
            continue;

        before += static_cast<double>(metric.coherenceBefore);
        after += static_cast<double>(metric.coherenceAfter);
        ++measured;
    }

    juce::String status = action + " @ " + percent(getGlideStrength()) + ": "
                          + juce::String(result.eventsMeasured) + " hits";
    if (measured > 0)
    {
        const float beforeMean = static_cast<float>(before / static_cast<double>(measured));
        const float afterMean = static_cast<float>(after / static_cast<double>(measured));
        status += ", event coherence " + percent(beforeMean) + " -> " + percent(afterMean);
    }
    if (result.limitedEvents > 0)
        status += ", " + juce::String(result.limitedEvents) + " limited";

    return status;
}

juce::String BeatEqualizerAudioProcessor::refreshGlidePreview()
{
    return refreshGlidePreview(true);
}

juce::String BeatEqualizerAudioProcessor::refreshGlidePreview(bool notify)
{
    GlideRun run;
    const auto error = renderGlide(run, true);
    glideStatus = error.isEmpty() ? formatGlideStatus("Glide preview", run.result) : error;

    if (notify)
        sendChangeMessage();

    return error;
}

juce::String BeatEqualizerAudioProcessor::exportGlide(const juce::File& file)
{
    GlideRun run;
    const auto error = renderGlide(run, false);
    if (error.isNotEmpty())
        return error;

    if (!beat::exporter::writeWav(file, run.rendered, run.sampleRate))
        return "Could not write " + file.getFileName();

    glideStatus = formatGlideStatus("Glide exported " + file.getFileName(), run.result);
    sendChangeMessage();
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
