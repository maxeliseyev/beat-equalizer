#include "doc/SessionProfile.h"

#include <algorithm>

namespace beat::doc
{

namespace
{
const DelayStat kUnknownDelay {};
const ChannelStat kUnknownChannel {};
} // namespace

void SessionProfile::setChannelCount(int count)
{
    channels = std::clamp(count, 0, kMaxChannels);
    delays.assign(static_cast<size_t>(channels * channels), DelayStat {});
    bleed.assign(static_cast<size_t>(channels * channels), 0.0f);
    stats.assign(static_cast<size_t>(channels), ChannelStat {});

    // Канал сам с собой стоит нулём и известен всегда: от него и меряют.
    for (int ch = 0; ch < channels; ++ch)
        delays[static_cast<size_t>(index(ch, ch))] = { 0.0, 0.0, 0, true };
}

int SessionProfile::index(int from, int to) const
{
    return from * channels + to;
}

void SessionProfile::setDelay(int from, int to, DelayStat stat)
{
    if (from < 0 || to < 0 || from >= channels || to >= channels)
        return;

    delays[static_cast<size_t>(index(from, to))] = stat;
}

const DelayStat& SessionProfile::delay(int from, int to) const
{
    if (from < 0 || to < 0 || from >= channels || to >= channels)
        return kUnknownDelay;

    return delays[static_cast<size_t>(index(from, to))];
}

bool SessionProfile::knows(int from, int to) const
{
    return delay(from, to).known;
}

void SessionProfile::setChannel(int index, ChannelStat stat)
{
    if (index < 0 || index >= channels)
        return;

    stats[static_cast<size_t>(index)] = stat;
}

const ChannelStat& SessionProfile::channel(int index) const
{
    if (index < 0 || index >= channels)
        return kUnknownChannel;

    return stats[static_cast<size_t>(index)];
}

void SessionProfile::setBleedDb(int from, int to, float db)
{
    if (from < 0 || to < 0 || from >= channels || to >= channels)
        return;

    bleed[static_cast<size_t>(index(from, to))] = db;
}

float SessionProfile::bleedDb(int from, int to) const
{
    if (from < 0 || to < 0 || from >= channels || to >= channels)
        return 0.0f;

    return bleed[static_cast<size_t>(index(from, to))];
}

void SessionProfile::priors(int from, std::array<double, kMaxChannels>& out) const
{
    out.fill(0.0);
    for (int to = 0; to < channels && to < kMaxChannels; ++to)
    {
        const auto& stat = delay(from, to);
        if (stat.known)
            out[static_cast<size_t>(to)] = stat.medianSamples;
    }
}

bool SessionProfile::empty() const
{
    for (int from = 0; from < channels; ++from)
        for (int to = 0; to < channels; ++to)
            if (from != to && knows(from, to))
                return false;

    return true;
}

void SessionProfile::clear()
{
    setChannelCount(channels);
}

} // namespace beat::doc
