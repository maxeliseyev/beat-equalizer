#include "doc/SpectralFluxDetector.h"

#include "dsp/Envelope.h"
#include "dsp/OnsetAnalysis.h"
#include "dsp/PeakPicker.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace beat::doc
{

namespace
{
struct Segment
{
    int arrival = 0;
    int attackEnd = 0;
    int usefulEnd = 0;
    float peak = 0.0f;
};

int msToSamples(float ms, double sampleRate)
{
    return std::max(1, static_cast<int>(std::lround(0.001 * static_cast<double>(ms) * sampleRate)));
}

// Границы удара по огибающей: пик — конец атаки, приход — последняя точка
// перед подъёмом, конец полезного — там, где вклад ушёл под пол плюс запас.
Segment measure(const std::vector<float>& envelope,
                int from,
                int to,
                int limit,
                float floorLevel,
                float marginDb)
{
    Segment segment;
    const int n = static_cast<int>(envelope.size());
    from = std::clamp(from, 0, std::max(0, n - 1));
    to = std::clamp(to, from, std::max(0, n - 1));

    segment.attackEnd = from;
    for (int i = from; i <= to; ++i)
        if (envelope[static_cast<size_t>(i)] > segment.peak)
        {
            segment.peak = envelope[static_cast<size_t>(i)];
            segment.attackEnd = i;
        }

    // Приход — последняя точка перед подъёмом. Если огибающая до края окна так
    // и не опустилась под порог (тихий удар на хвосте соседнего), берём самую
    // тихую точку: это лучшее, что известно, и оно всегда определено.
    const float onsetLevel = floorLevel + 0.1f * std::max(0.0f, segment.peak - floorLevel);
    int crossing = -1;
    int quietest = segment.attackEnd;
    float lowest = envelope[static_cast<size_t>(segment.attackEnd)];

    for (int i = segment.attackEnd; i >= from; --i)
    {
        const float value = envelope[static_cast<size_t>(i)];
        if (value <= onsetLevel)
        {
            crossing = i;
            break;
        }

        if (value < lowest)
        {
            lowest = value;
            quietest = i;
        }
    }

    segment.arrival = crossing >= 0 ? crossing : quietest;

    const float usefulLevel = floorLevel * std::pow(10.0f, marginDb / 20.0f);
    segment.usefulEnd = std::min(limit, n - 1);
    for (int i = segment.attackEnd; i <= std::min(limit, n - 1); ++i)
        if (envelope[static_cast<size_t>(i)] <= usefulLevel)
        {
            segment.usefulEnd = i;
            break;
        }

    segment.usefulEnd = std::max(segment.usefulEnd, segment.attackEnd);
    return segment;
}

// Спад: прямая по логарифму огибающей от пика до 20 дБ ниже него. Наклон в
// дБ за секунду — число, сравнимое между каналами и между сессиями.
float decaySlope(const std::vector<float>& envelope,
                 const Segment& segment,
                 double sampleRate)
{
    if (segment.peak <= 0.0f || segment.usefulEnd <= segment.attackEnd)
        return 0.0f;

    const float target = segment.peak * 0.1f;
    int end = segment.attackEnd;
    for (int i = segment.attackEnd; i <= segment.usefulEnd; ++i)
    {
        end = i;
        if (envelope[static_cast<size_t>(i)] <= target)
            break;
    }

    if (end <= segment.attackEnd)
        return 0.0f;

    const float level = std::max(envelope[static_cast<size_t>(end)], 1.0e-9f);
    const double drop = 20.0 * std::log10(static_cast<double>(segment.peak / level));
    const double seconds = static_cast<double>(end - segment.attackEnd) / sampleRate;

    return seconds > 0.0 ? static_cast<float>(drop / seconds) : 0.0f;
}

// Перцептивная атака: момент, где громкость растёт быстрее всего. Считается по
// сумме логарифмов полос, а не по широкополосной энергии: у бочки низ
// разгоняется дольше щелчка, и в широкой полосе этот момент смазан.
double perceptualAttack(const OnsetFeatures& features, int fromFrame, int toFrame)
{
    const int numBands = static_cast<int>(features.bands.size());
    if (numBands == 0 || toFrame <= fromFrame)
        return features.timeOfFrame(std::max(0, fromFrame));

    const auto loudnessAt = [&](int frame)
    {
        float sum = 0.0f;
        for (int b = 0; b < numBands; ++b)
            sum += std::log1p(1000.0f * features.bands[static_cast<size_t>(b)]
                                                      [static_cast<size_t>(frame)]);

        return sum;
    };

    int best = fromFrame;
    float steepest = 0.0f;
    for (int frame = std::max(1, fromFrame); frame <= toFrame; ++frame)
    {
        const float rise = loudnessAt(frame) - loudnessAt(frame - 1);
        if (rise > steepest)
        {
            steepest = rise;
            best = frame;
        }
    }

    return features.timeOfFrame(best);
}

Feature toFeature(std::vector<float> data, int hopSamples, double firstSampleOffset)
{
    Feature feature;
    feature.data = std::move(data);
    feature.hopSamples = static_cast<double>(hopSamples);
    feature.firstSampleOffset = firstSampleOffset;
    return feature;
}

// Огибающая кладётся в кэш на кадровой сетке: в полном разрешении она весит
// столько же, сколько звук, а нужна для показа и для порогов.
std::vector<float> onHopGrid(const std::vector<float>& envelope, int hop, int numFrames, int window)
{
    std::vector<float> out(static_cast<size_t>(std::max(0, numFrames)), 0.0f);
    const int n = static_cast<int>(envelope.size());

    for (int frame = 0; frame < numFrames; ++frame)
    {
        const int start = frame * hop;
        float top = 0.0f;
        for (int i = start; i < std::min(n, start + window); ++i)
            top = std::max(top, envelope[static_cast<size_t>(i)]);

        out[static_cast<size_t>(frame)] = top;
    }

    return out;
}
} // namespace

SpectralFluxDetector::SpectralFluxDetector(SpectralFluxSettings settings)
    : config(settings)
{
}

std::string SpectralFluxDetector::parameters() const
{
    std::ostringstream out;
    out << "window=" << config.windowMs << "ms"
        << ";hop=" << config.hopMs << "ms"
        << ";bands=" << config.numBands
        << ";factor=" << config.thresholdFactor
        << ";bias=" << config.thresholdBias
        << ";minInterval=" << config.minIntervalMs << "ms"
        << ";minConfidence=" << config.minConfidence
        << ";attack=" << config.attackMs << "ms"
        << ";release=" << config.releaseMs << "ms";

    return out.str();
}

std::vector<Event> SpectralFluxDetector::analyze(const float* const* channels,
                                                 int numChannels,
                                                 int numSamples,
                                                 const AnalysisContext& context)
{
    const int usable = std::min(numChannels, kMaxChannels);
    if (channels == nullptr || usable <= 0 || numSamples <= 0 || context.sampleRate <= 0.0)
        return {};

    const int reference = std::clamp(context.referenceChannel, 0, usable - 1);

    OnsetAnalysisConfig analysisConfig;
    analysisConfig.sampleRate = context.sampleRate;
    analysisConfig.windowMs = config.windowMs;
    analysisConfig.hopMs = config.hopMs;
    analysisConfig.numBands = config.numBands;

    OnsetAnalysis analysis(analysisConfig);
    const int hop = analysis.hopSamples();
    const int window = analysis.windowSamples();

    std::vector<OnsetFeatures> features(static_cast<size_t>(usable));
    std::vector<std::vector<float>> envelopes(static_cast<size_t>(usable));
    std::vector<float> floors(static_cast<size_t>(usable), 0.0f);

    for (int ch = 0; ch < usable; ++ch)
    {
        const auto index = static_cast<size_t>(ch);
        features[index] = analysis.run(channels[ch], numSamples);
        envelopes[index] = followEnvelope(channels[ch],
                                          numSamples,
                                          context.sampleRate,
                                          config.attackMs,
                                          config.releaseMs);
        floors[index] = noiseFloorOf(envelopes[index]);

        if (context.features == nullptr)
            continue;

        // Признаки живут отдельно от решений: смена порогов выбросит события и
        // не заставит считать это заново.
        const double offset = context.startSample + features[index].firstSampleOffset;
        auto& cache = *context.features;
        cache.put(context.source, ch, FeatureKind::spectralFlux, 0,
                  toFeature(features[index].flux, hop, offset));
        cache.put(context.source, ch, FeatureKind::envelope, 0,
                  toFeature(onHopGrid(envelopes[index], hop, features[index].numFrames, window),
                            hop, context.startSample));
        cache.put(context.source, ch, FeatureKind::noiseFloor, 0,
                  toFeature({ floors[index] }, hop, context.startSample));

        for (size_t b = 0; b < features[index].bands.size(); ++b)
            cache.put(context.source, ch, FeatureKind::bandEnvelope, static_cast<int>(b),
                      toFeature(features[index].bands[b], hop, offset));
    }

    PeakPickConfig pickConfig;
    pickConfig.sampleRate = context.sampleRate;
    pickConfig.hopSamples = hop;
    pickConfig.thresholdFactor = config.thresholdFactor;
    pickConfig.thresholdBias = config.thresholdBias;
    pickConfig.minIntervalMs = config.minIntervalMs;

    const auto& referenceFeatures = features[static_cast<size_t>(reference)];
    const auto peaks = pickPeaks(referenceFeatures.flux, pickConfig);
    const auto& envelope = envelopes[static_cast<size_t>(reference)];
    const float floorLevel = floors[static_cast<size_t>(reference)];
    const int energyWindow = msToSamples(config.energyWindowMs, context.sampleRate);

    std::vector<Event> events;
    events.reserve(peaks.size());

    for (size_t p = 0; p < peaks.size(); ++p)
    {
        const auto& peak = peaks[p];
        const int centre = static_cast<int>(std::lround(referenceFeatures.timeOfFrame(peak.frame)));
        const int nextCentre = p + 1 < peaks.size()
                                   ? static_cast<int>(std::lround(
                                         referenceFeatures.timeOfFrame(peaks[p + 1].frame)))
                                   : numSamples - 1;

        // Кадр шире удара: искать границы надо по огибающей вокруг него, а не
        // считать центр кадра приходом.
        const auto segment = measure(envelope,
                                     centre - window,
                                     centre + window,
                                     std::max(centre, nextCentre - window),
                                     floorLevel,
                                     config.usefulEndMarginDb);

        Event event;
        event.referenceChannel = reference;
        event.origin = Origin::detector;
        event.timeSamples = context.startSample + static_cast<double>(segment.arrival);

        // Уверенность — число, а не флаг: пороги отсечения настраиваются потом
        // без нового анализа.
        const float strength = std::max(1.0f, peak.strength());
        event.confidence = 1.0f - 1.0f / strength;

        auto& observation = event.channels[static_cast<size_t>(reference)];
        observation.present = true;
        observation.origin = Origin::detector;
        observation.confidence = event.confidence;
        observation.arrivalSamples = event.timeSamples;
        observation.attackEndSamples = context.startSample + static_cast<double>(segment.attackEnd);
        observation.usefulEndSamples = context.startSample + static_cast<double>(segment.usefulEnd);
        observation.decayDbPerSecond = decaySlope(envelope, segment, context.sampleRate);

        const int arrivalFrame = std::clamp(segment.arrival / hop, 0,
                                            std::max(0, referenceFeatures.numFrames - 1));
        const int attackFrame = std::clamp(segment.attackEnd / hop, 0,
                                           std::max(0, referenceFeatures.numFrames - 1));
        observation.perceptualAttackSamples =
            context.startSample + perceptualAttack(referenceFeatures, arrivalFrame, attackFrame);

        // Энергетический вектор: признак, по которому дальше решают, тот ли это
        // удар в соседнем микрофоне и какой это барабан.
        float top = 0.0f;
        for (int ch = 0; ch < usable; ++ch)
        {
            double sum = 0.0;
            const int from = std::clamp(segment.arrival, 0, numSamples - 1);
            const int to = std::min(numSamples, from + energyWindow);
            for (int i = from; i < to; ++i)
            {
                const double value = static_cast<double>(channels[ch][i]);
                sum += value * value;
            }

            const auto count = static_cast<double>(std::max(1, to - from));
            const float rms = static_cast<float>(std::sqrt(sum / count));
            event.energy[static_cast<size_t>(ch)] = rms;
            top = std::max(top, rms);
        }

        if (top > 0.0f)
            for (int ch = 0; ch < usable; ++ch)
                event.energy[static_cast<size_t>(ch)] /= top;

        // Отсев дешёвый и объяснимый: пик, стоящий впритык к адаптивному
        // порогу, чаще оказывается просачиванием соседнего инструмента.
        if (event.confidence < config.minConfidence)
            continue;

        events.push_back(event);
    }

    return events;
}

} // namespace beat::doc
