#include "doc/SessionCalibration.h"

#include "dsp/Envelope.h"
#include "dsp/HitSegment.h"

#include <algorithm>
#include <cmath>
#include <vector>

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

double medianOf(std::vector<double> values)
{
    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

// Медиана абсолютных отклонений: один промах линейки не должен решать, знает
// профиль эту пару каналов или нет.
double madOf(const std::vector<double>& values, double median)
{
    std::vector<double> deviations;
    deviations.reserve(values.size());
    for (double value : values)
        deviations.push_back(std::abs(value - median));

    return medianOf(deviations);
}
} // namespace

SessionCalibration::SessionCalibration(CalibrationSettings settings)
    : config(settings)
{
}

SessionProfile SessionCalibration::run(IOnsetDetector& detector,
                                       const float* const* channels,
                                       int numChannels,
                                       int numSamples,
                                       const CalibrationContext& context,
                                       CalibrationReport* report)
{
    SessionProfile profile;

    const int usable = std::min(numChannels, kMaxChannels);
    if (channels == nullptr || usable <= 0 || numSamples <= 0 || context.sampleRate <= 0.0)
        return profile;

    profile.setChannelCount(usable);

    CalibrationReport counted;

    std::vector<std::vector<float>> envelopes(static_cast<size_t>(usable));
    std::vector<float> floors(static_cast<size_t>(usable), 0.0f);

    for (int ch = 0; ch < usable; ++ch)
    {
        const auto index = static_cast<size_t>(ch);
        envelopes[index] = followEnvelope(channels[ch], numSamples, context.sampleRate,
                                          config.attackMs, config.releaseMs);
        floors[index] = noiseFloorOf(envelopes[index]);

        ChannelStat stat;
        stat.noiseFloor = floors[index];
        stat.rms = static_cast<float>(rmsOf(channels[ch], 0, numSamples));
        profile.setChannel(ch, stat);
    }

    const int isolation = msToSamples(config.isolationMs, context.sampleRate);
    const int energyWindow = msToSamples(config.energyWindowMs, context.sampleRate);
    const int search = maxLagSamples(config.searchDistanceM, context.sampleRate);
    const double maxSpread = 0.001 * static_cast<double>(config.maxSpreadMs) * context.sampleRate;

    std::vector<int> references = context.references;
    if (references.empty())
        for (int ch = 0; ch < usable; ++ch)
            references.push_back(ch);

    for (int reference : references)
    {
        if (reference < 0 || reference >= usable)
            continue;

        AnalysisContext analysis;
        analysis.sampleRate = context.sampleRate;
        analysis.referenceChannel = reference;
        analysis.startSample = context.startSample;

        const auto events = detector.analyze(channels, usable, numSamples, analysis);
        counted.detected += static_cast<int>(events.size());

        std::vector<double> collected[kMaxChannels];
        std::vector<double> bleed[kMaxChannels];
        int owned = 0;

        for (size_t e = 0; e < events.size(); ++e)
        {
            const auto& event = events[e];
            if (event.confidence < config.minConfidence)
                continue;

            // Одиночность: у соседа не должно быть шанса подсунуть свой приход
            // вместо нужного. На плотной игре мерить нечего.
            const double before = e > 0 ? event.timeSamples - events[e - 1].timeSamples
                                        : static_cast<double>(numSamples);
            const double after = e + 1 < events.size()
                                     ? events[e + 1].timeSamples - event.timeSamples
                                     : static_cast<double>(numSamples);
            if (std::min(before, after) < static_cast<double>(isolation))
                continue;

            const int at = static_cast<int>(std::lround(event.timeSamples - context.startSample));
            if (at < 0 || at >= numSamples)
                continue;

            const int to = std::min(numSamples, at + energyWindow);
            const double own = rmsOf(channels[reference], at, to);
            if (own <= 0.0)
                continue;

            bool dominant = true;
            for (int ch = 0; ch < usable && dominant; ++ch)
            {
                if (ch == reference)
                    continue;

                const double other = rmsOf(channels[ch], at, to);
                dominant = 20.0 * std::log10(own / std::max(other, 1.0e-12))
                           >= static_cast<double>(config.dominanceDb);
            }

            if (!dominant)
                continue;

            ++counted.selected;
            ++owned;

            const auto& observation = event.channels[static_cast<size_t>(reference)];
            const int referencePeak = static_cast<int>(
                std::lround(observation.attackEndSamples - context.startSample));
            const int referenceOnset = arrivalAboveFloor(envelopes[static_cast<size_t>(reference)],
                                                         referencePeak,
                                                         referencePeak - search,
                                                         floors[static_cast<size_t>(reference)],
                                                         config.arrivalMarginDb);

            for (int ch = 0; ch < usable; ++ch)
            {
                if (ch == reference)
                    continue;

                const auto index = static_cast<size_t>(ch);

                // Кандидат в этом канале — самый громкий в допустимом окне.
                // Удар одиночный, поэтому спутать его почти не с чем; на
                // плотной игре было бы не так, и потому отбор идёт раньше.
                const auto found = segmentHit(envelopes[index], at - search, at + search,
                                              numSamples - 1, floors[index],
                                              config.arrivalMarginDb);

                const int onset = arrivalAboveFloor(envelopes[index], found.attackEnd,
                                                    found.attackEnd - search, floors[index],
                                                    config.arrivalMarginDb);

                const double delay = static_cast<double>(onset - referenceOnset);
                if (std::abs(delay) > static_cast<double>(search))
                    continue;

                collected[index].push_back(delay);

                const double heard = rmsOf(channels[ch], at, to);
                bleed[index].push_back(20.0 * std::log10(std::max(heard, 1.0e-12) / own));
            }
        }

        ChannelStat stat = profile.channel(reference);
        stat.owned = owned;
        profile.setChannel(reference, stat);

        for (int ch = 0; ch < usable; ++ch)
        {
            if (ch == reference)
                continue;

            const auto index = static_cast<size_t>(ch);
            const auto& values = collected[index];

            DelayStat delayStat;
            delayStat.observations = static_cast<int>(values.size());

            if (values.empty())
            {
                profile.setDelay(reference, ch, delayStat);
                ++counted.rejected;
                continue;
            }

            delayStat.medianSamples = medianOf(values);
            delayStat.spreadSamples = madOf(values, delayStat.medianSamples);

            // Знание — это не «посчитали», а «посчитали одно и то же».
            // Геометрия пары постоянна: разъехавшиеся числа означают, что
            // линейка мерила разные удары, и подставлять их медиану в сверку
            // хуже, чем не знать.
            delayStat.known = delayStat.observations >= config.minHits
                              && delayStat.spreadSamples <= maxSpread;

            if (delayStat.known)
                ++counted.known;
            else
                ++counted.rejected;

            profile.setDelay(reference, ch, delayStat);
            profile.setBleedDb(reference, ch, static_cast<float>(medianOf(bleed[index])));
        }
    }

    if (report != nullptr)
        *report = counted;

    return profile;
}

} // namespace beat::doc
