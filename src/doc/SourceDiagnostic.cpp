#include "doc/SourceDiagnostic.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace beat::doc
{

namespace
{
struct SampleStats
{
    int count = 0;
    double median = 0.0;
    double spread = 0.0;
};

SampleStats statsOf(std::vector<double> values)
{
    SampleStats stats;
    if (values.empty())
        return stats;

    std::sort(values.begin(), values.end());
    stats.count = static_cast<int>(values.size());
    stats.median = values[values.size() / 2];

    std::vector<double> deviations;
    deviations.reserve(values.size());
    for (double value : values)
        deviations.push_back(std::abs(value - stats.median));

    std::sort(deviations.begin(), deviations.end());
    stats.spread = deviations[deviations.size() / 2];
    return stats;
}
} // namespace

SourceDiagnostic buildSourceDiagnostic(const Document& document,
                                       const SessionProfile& profile,
                                       int source,
                                       int channels)
{
    SourceDiagnostic diagnostic;

    const int usable = std::clamp(channels, 0, kMaxChannels);
    if (usable <= 0)
        return diagnostic;

    source = std::clamp(source, 0, usable - 1);
    diagnostic.valid = true;
    diagnostic.sourceChannel = source;
    diagnostic.totalEvents = static_cast<int>(document.events().size());

    for (int ch = 0; ch < usable; ++ch)
        if (ch != source && profile.knows(source, ch))
            ++diagnostic.calibratedDelays;

    std::array<std::vector<double>, kMaxChannels> raw;
    std::array<std::vector<double>, kMaxChannels> fullAlign;

    for (const auto& event : document.events())
    {
        if (event.referenceChannel != source || !document.delays().has(event.id, source))
            continue;

        ++diagnostic.sourceOwnedEvents;
        const double sourceRaw = document.delays().raw(event.id, source);
        const double sourceApplied = document.delays().applied(event.id, source);

        for (int ch = 0; ch < usable; ++ch)
        {
            if (!event.channels[static_cast<size_t>(ch)].present
                || !document.delays().has(event.id, ch))
                continue;

            const double micRaw = document.delays().raw(event.id, ch);
            raw[static_cast<size_t>(ch)].push_back(micRaw - sourceRaw);
            fullAlign[static_cast<size_t>(ch)].push_back(
                micRaw + document.delays().applied(event.id, ch) - sourceRaw - sourceApplied);
        }
    }

    double nearest = std::numeric_limits<double>::max();
    double farthest = 0.0;

    for (int ch = 0; ch < usable; ++ch)
    {
        const auto index = static_cast<size_t>(ch);
        auto& row = diagnostic.channels[index];
        const auto rawStats = statsOf(std::move(raw[index]));
        const auto fullStats = statsOf(std::move(fullAlign[index]));

        row.observations = rawStats.count;
        row.rawMedianSamples = rawStats.median;
        row.rawSpreadSamples = rawStats.spread;
        row.fullAlignOffsetSamples = fullStats.median;
        row.naturalOffsetSamples = rawStats.median;
        row.calibrated = ch == source || profile.knows(source, ch);

        if (ch != source && profile.knows(source, ch))
            row.calibrationResidualSamples =
                rawStats.median - profile.delay(source, ch).medianSamples;

        diagnostic.sourceObservations += row.observations;

        if (ch == source || row.observations <= 0)
            continue;

        const double absRaw = std::abs(row.rawMedianSamples);
        if (absRaw < nearest)
        {
            nearest = absRaw;
            diagnostic.closeChannel = ch;
        }

        if (absRaw > farthest)
        {
            farthest = absRaw;
            diagnostic.lateChannel = ch;
        }
    }

    return diagnostic;
}

} // namespace beat::doc
