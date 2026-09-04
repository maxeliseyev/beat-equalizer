#include "doc/CrossfadeEditAdapter.h"

#include "doc/Document.h"

#include <algorithm>

namespace beat::doc
{

namespace
{
int channelCountOf(const Document& document, int requested)
{
    const int count = requested > 0 ? requested : document.channelCount();
    return std::clamp(count, 0, kMaxChannels);
}

bool fillPoint(CrossfadeRenderer::EditPoint& point,
               const Document& document,
               const Event& event,
               int numChannels)
{
    point.timeSamples = event.timeSamples;
    point.referenceChannel =
        std::clamp(event.referenceChannel, 0, std::max(0, numChannels - 1));

    bool any = false;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (!document.delays().has(event.id, ch))
            continue;

        point.setDelay(ch, static_cast<float>(document.delays().applied(event.id, ch)));
        any = true;
    }

    return any;
}
} // namespace

CrossfadeEditPlan buildCrossfadeEditPlan(const Document& document,
                                         CrossfadeEditAdapterOptions options)
{
    CrossfadeEditPlan plan;
    const int numChannels = channelCountOf(document, options.numChannels);
    if (numChannels <= 0)
        return plan;

    const auto regions = buildEditRegionPlan(document, options.regionOptions);
    EventId lastPointEvent = kInvalidId;

    for (const auto& region : regions)
    {
        ++plan.regionsConsidered;

        if (!region.canCrossfade())
        {
            ++plan.regionsSkipped;
            continue;
        }

        const auto* previous = document.event(region.previousEvent);
        const auto* next = document.event(region.nextEvent);
        if (previous == nullptr || next == nullptr)
        {
            ++plan.regionsSkipped;
            continue;
        }

        const bool needsSeed = lastPointEvent != previous->id;
        CrossfadeRenderer::EditPoint seed;
        if (needsSeed && !fillPoint(seed, document, *previous, numChannels))
        {
            ++plan.eventsWithoutDelay;
            ++plan.regionsSkipped;
            continue;
        }

        CrossfadeRenderer::EditPoint target;
        target.joinStartSamples = region.join.start;
        target.joinEndSamples = region.join.end;
        if (!fillPoint(target, document, *next, numChannels))
        {
            ++plan.eventsWithoutDelay;
            ++plan.regionsSkipped;
            continue;
        }

        if (needsSeed)
        {
            plan.editPoints.push_back(seed);
            lastPointEvent = previous->id;
        }

        plan.editPoints.push_back(target);
        lastPointEvent = next->id;
        ++plan.crossfadesPrepared;
    }

    return plan;
}

} // namespace beat::doc
