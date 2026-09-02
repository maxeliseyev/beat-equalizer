#include "doc/CrossMicMatcher.h"

#include "dsp/Envelope.h"
#include "dsp/GccPhat.h"
#include "dsp/HitSegment.h"

#include <algorithm>
#include <cmath>

namespace beat::doc
{

namespace
{
int msToSamples(float ms, double sampleRate)
{
    return std::max(1, static_cast<int>(std::lround(0.001 * static_cast<double>(ms) * sampleRate)));
}

double rmsOf(const float* samples, int from, int to)
{
    if (to <= from)
        return 0.0;

    double sum = 0.0;
    for (int i = from; i < to; ++i)
        sum += static_cast<double>(samples[i]) * static_cast<double>(samples[i]);

    return std::sqrt(sum / static_cast<double>(to - from));
}

// Корреляция логарифмов огибающих. Логарифм, а не сама огибающая: у прямого
// звука и его просачивания совпадает форма затухания, а не уровень.
float envelopeCorrelation(const std::vector<float>& a,
                          const std::vector<float>& b,
                          int aFrom,
                          int bFrom,
                          int length,
                          float floorA,
                          float floorB)
{
    const int aEnd = std::min(aFrom + length, static_cast<int>(a.size()));
    const int bEnd = std::min(bFrom + length, static_cast<int>(b.size()));
    const int count = std::min(aEnd - aFrom, bEnd - bFrom);

    if (aFrom < 0 || bFrom < 0 || count < 8)
        return 0.0f;

    const auto logAt = [](const std::vector<float>& x, int index, float floorLevel)
    {
        return std::log(std::max(x[static_cast<size_t>(index)], std::max(floorLevel, 1.0e-9f)));
    };

    double meanA = 0.0;
    double meanB = 0.0;
    for (int i = 0; i < count; ++i)
    {
        meanA += logAt(a, aFrom + i, floorA);
        meanB += logAt(b, bFrom + i, floorB);
    }

    meanA /= count;
    meanB /= count;

    double covariance = 0.0;
    double varianceA = 0.0;
    double varianceB = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const double da = logAt(a, aFrom + i, floorA) - meanA;
        const double db = logAt(b, bFrom + i, floorB) - meanB;
        covariance += da * db;
        varianceA += da * da;
        varianceB += db * db;
    }

    const double denominator = std::sqrt(varianceA * varianceB);
    return denominator > 0.0 ? static_cast<float>(covariance / denominator) : 0.0f;
}
} // namespace

CrossMicMatcher::CrossMicMatcher(MatchSettings settings)
    : config(settings)
{
}

MatchReport CrossMicMatcher::match(Document& document,
                                   const float* const* channels,
                                   int numChannels,
                                   int numSamples,
                                   const MatchContext& context)
{
    MatchReport report;

    const int usable = std::min(numChannels, kMaxChannels);
    if (channels == nullptr || usable <= 0 || numSamples <= 0 || context.sampleRate <= 0.0)
        return report;

    const int maxLag = maxLagSamples(config.maxDistanceM, context.sampleRate);
    const int frame = msToSamples(config.frameMs, context.sampleRate);
    const int preRoll = msToSamples(config.preRollMs, context.sampleRate);
    const int envelopeWindow = msToSamples(config.envelopeWindowMs, context.sampleRate);
    const int energyWindow = msToSamples(kOnsetEnergyWindowMs, context.sampleRate);

    std::vector<std::vector<float>> envelopes(static_cast<size_t>(usable));
    std::vector<float> floors(static_cast<size_t>(usable), 0.0f);
    std::vector<double> channelRms(static_cast<size_t>(usable), 0.0);

    for (int ch = 0; ch < usable; ++ch)
    {
        const auto index = static_cast<size_t>(ch);
        envelopes[index] = followEnvelope(channels[ch],
                                          numSamples,
                                          context.sampleRate,
                                          config.attackMs,
                                          config.releaseMs);
        floors[index] = noiseFloorOf(envelopes[index]);
        channelRms[index] = rmsOf(channels[ch], 0, numSamples);
    }

    GccPhat gcc(kDefaultFftOrder);

    std::vector<EventId> ids;
    ids.reserve(document.events().size());
    for (const auto& event : document.events())
        ids.push_back(event.id);

    for (EventId id : ids)
    {
        auto* event = document.event(id);
        if (event == nullptr)
            continue;

        const int reference = std::clamp(event->referenceChannel, 0, usable - 1);
        const auto& referenceObservation = event->channels[static_cast<size_t>(reference)];
        if (!referenceObservation.present)
            continue;

        const int refArrival = static_cast<int>(
            std::lround(referenceObservation.arrivalSamples - context.startSample));

        if (refArrival < 0 || refArrival >= numSamples)
            continue;

        // Опора всегда стоит в поле задержек нулём: остальные меряются от неё.
        document.delays().setRaw(id, reference, 0.0);

        for (int ch = 0; ch < usable; ++ch)
        {
            if (ch == reference)
                continue;

            const auto index = static_cast<size_t>(ch);
            const int predicted = refArrival
                                  + static_cast<int>(std::lround(context.prior[index]));

            const int from = refArrival - preRoll;
            const int to = predicted - preRoll;
            if (from < 0 || to < 0 || from + frame > numSamples || to + frame > numSamples)
            {
                ++report.rejected;
                continue;
            }

            // Предсказание уточняется, а не подтверждается: GCC-PHAT ищет
            // остаток относительно априорной задержки, и только внутри окна,
            // которое разрешает физика.
            const auto estimate = gcc.estimate(channels[reference] + from,
                                               channels[ch] + to,
                                               frame,
                                               maxLag,
                                               context.sampleRate);

            if (!estimate.valid)
            {
                ++report.rejected;
                continue;
            }

            const double delay = context.prior[index] + static_cast<double>(estimate.lagSamples);
            const int arrival = static_cast<int>(std::lround(refArrival + delay));
            if (arrival < 0 || arrival >= numSamples)
            {
                ++report.rejected;
                continue;
            }

            const float correlation = envelopeCorrelation(envelopes[static_cast<size_t>(reference)],
                                                          envelopes[index],
                                                          refArrival,
                                                          arrival,
                                                          envelopeWindow,
                                                          floors[static_cast<size_t>(reference)],
                                                          floors[index]);

            const double level = rmsOf(channels[ch], arrival, std::min(numSamples,
                                                                       arrival + energyWindow));
            const double floorLevel = std::max(static_cast<double>(floors[index]), 1.0e-9);
            const double aboveFloorDb = 20.0 * std::log10(std::max(level, 1.0e-12) / floorLevel);

            if (correlation < config.minCorrelation
                || aboveFloorDb < static_cast<double>(config.minAudibleDb))
            {
                ++report.rejected;
                continue;
            }

            const auto segment = segmentHit(envelopes[index],
                                            arrival - preRoll,
                                            arrival + frame,
                                            numSamples - 1,
                                            floors[index],
                                            config.usefulEndMarginDb);

            auto& observation = event->channels[index];
            observation.present = true;
            observation.origin = Origin::detector;
            observation.confidence = std::clamp(correlation, 0.0f, 1.0f);
            observation.arrivalSamples = context.startSample + refArrival + delay;
            observation.attackEndSamples = context.startSample + segment.attackEnd;
            observation.usefulEndSamples = context.startSample + segment.usefulEnd;
            observation.decayDbPerSecond = decaySlope(envelopes[index], segment,
                                                      context.sampleRate);
            // Перцептивная атака в этом канале своя, но её меряет детектор по
            // полосам; здесь известен только приход, и врать нечем.
            observation.perceptualAttackSamples = observation.arrivalSamples;

            document.delays().setRaw(id, ch, delay);
            ++report.observations;
        }

        // Владелец: канал, где удар и раньше, и энергичнее относительно
        // собственного среднего. Просачивание не проходит ни по одному из двух.
        int owner = reference;
        double earliest = 0.0;
        double ownerLoudness = 0.0;

        for (int ch = 0; ch < usable; ++ch)
        {
            const auto index = static_cast<size_t>(ch);
            if (!event->channels[index].present)
                continue;

            const int arrival = static_cast<int>(
                std::lround(event->channels[index].arrivalSamples - context.startSample));
            const double level = rmsOf(channels[ch], arrival,
                                       std::min(numSamples, arrival + energyWindow));
            const double own = std::max(channelRms[index], 1.0e-12);
            const double loudness = level / own;

            if (loudness < static_cast<double>(config.ownerMargin))
                continue;

            const double delay = document.delays().raw(id, ch);
            if (owner == reference && ownerLoudness == 0.0)
            {
                owner = ch;
                earliest = delay;
                ownerLoudness = loudness;
                continue;
            }

            if (delay < earliest - 1.0 || (std::abs(delay - earliest) <= 1.0
                                           && loudness > ownerLoudness))
            {
                owner = ch;
                earliest = delay;
                ownerLoudness = loudness;
            }
        }

        if (owner != event->referenceChannel)
        {
            event->referenceChannel = owner;
            ++report.reattributed;
        }

        if (observationCount(*event) <= 1)
            ++report.singleChannel;
    }

    return report;
}

} // namespace beat::doc
