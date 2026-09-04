#pragma once

#include "doc/EditRegionPlan.h"
#include "dsp/CrossfadeRenderer.h"

#include <vector>

namespace beat::doc
{

struct CrossfadeEditAdapterOptions
{
    int numChannels = 0;
    EditRegionPlanOptions regionOptions {};
};

struct CrossfadeEditPlan
{
    std::vector<CrossfadeRenderer::EditPoint> editPoints;
    int regionsConsidered = 0;
    int crossfadesPrepared = 0;
    int regionsSkipped = 0;
    int eventsWithoutDelay = 0;
};

CrossfadeEditPlan buildCrossfadeEditPlan(const Document& document,
                                         CrossfadeEditAdapterOptions options = {});

} // namespace beat::doc
