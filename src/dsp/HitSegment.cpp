#include "HitSegment.h"

#include <algorithm>
#include <cmath>

namespace beat
{

HitSegment segmentHit(const std::vector<float>& envelope,
                      int from,
                      int to,
                      int limit,
                      float floorLevel,
                      float marginDb)
{
    HitSegment segment;
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

float decaySlope(const std::vector<float>& envelope,
                 const HitSegment& segment,
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

} // namespace beat
