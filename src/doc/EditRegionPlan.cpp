#include "doc/EditRegionPlan.h"

#include "doc/Document.h"

#include <algorithm>

namespace beat::doc
{

namespace
{
double msToSamples(float valueMs, double sampleRate)
{
    if (valueMs <= 0.0f || sampleRate <= 0.0)
        return 0.0;

    return 0.001 * static_cast<double>(valueMs) * sampleRate;
}
} // namespace

std::vector<EditRegion> buildEditRegionPlan(const Document& document,
                                             EditRegionPlanOptions options)
{
    const auto& events = document.events();
    const double sampleRate = document.sampleRate();
    if (events.size() < 2 || sampleRate <= 0.0)
        return {};

    std::vector<EditRegion> regions;
    regions.reserve(events.size() - 1);

    const double requestedCrossfade = msToSamples(options.crossfadeMs, sampleRate);

    for (size_t i = 1; i < events.size(); ++i)
    {
        const auto& previous = events[i - 1];
        const auto& next = events[i];

        EditRegion region;
        region.previousEvent = previous.id;
        region.nextEvent = next.id;
        region.previousProtected = protectedZone(previous, sampleRate, options.protectedMarginMs);
        region.nextProtected = protectedZone(next, sampleRate, options.protectedMarginMs);
        region.requestedCrossfadeSamples = requestedCrossfade;

        if (region.previousProtected.empty() || region.nextProtected.empty())
        {
            region.status = EditRegionStatus::missingProtectedZone;
            regions.push_back(region);
            continue;
        }

        region.warpBudgetSamples = warpBudget(region.previousProtected, region.nextProtected);
        if (region.warpBudgetSamples <= 0.0)
        {
            region.status = EditRegionStatus::protectedZonesOverlap;
            regions.push_back(region);
            continue;
        }

        const double crossfade = std::min(requestedCrossfade, region.warpBudgetSamples);
        if (crossfade > 0.0)
            region.join = { region.nextProtected.start - crossfade, region.nextProtected.start };

        region.body = { region.previousProtected.end, region.nextProtected.start - crossfade };
        if (region.body.empty())
            region.body = {};

        if (crossfade < requestedCrossfade)
            region.status = EditRegionStatus::crossfadeClamped;

        regions.push_back(region);
    }

    return regions;
}

} // namespace beat::doc
