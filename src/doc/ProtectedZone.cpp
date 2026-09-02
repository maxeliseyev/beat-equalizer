#include "doc/ProtectedZone.h"

#include <algorithm>
#include <limits>

namespace beat::doc
{

Interval protectedZone(const Event& event, double sampleRate, float marginMs)
{
    if (sampleRate <= 0.0)
        return {};

    double first = std::numeric_limits<double>::max();
    double last = std::numeric_limits<double>::lowest();
    bool any = false;

    for (const auto& observation : event.channels)
    {
        if (!observation.present)
            continue;

        first = std::min(first, observation.arrivalSamples);
        last = std::max(last, observation.attackEndSamples);
        any = true;
    }

    if (!any || !(last > first))
        return {};

    const double margin = 0.001 * static_cast<double>(marginMs) * sampleRate;
    return { first - margin, last + margin };
}

double warpBudget(const Interval& previous, const Interval& next)
{
    if (previous.empty() || next.empty())
        return 0.0;

    return next.start - previous.end;
}

} // namespace beat::doc
