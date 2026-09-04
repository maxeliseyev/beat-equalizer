#pragma once

#include "doc/Ids.h"
#include "doc/ProtectedZone.h"
#include "dsp/Constants.h"

#include <vector>

namespace beat::doc
{

class Document;

enum class EditRegionStatus
{
    ready = 0,
    crossfadeClamped,
    protectedZonesOverlap,
    missingProtectedZone
};

struct EditRegionPlanOptions
{
    float protectedMarginMs = kProtectedMarginMs;
    float crossfadeMs = kEditCrossfadeMs;
};

// Промежуток между двумя соседними событиями. Атаки уже защищены с обеих
// сторон; body остаётся будущему WSOLA, join — равноамплитудной склейке.
struct EditRegion
{
    EventId previousEvent = kInvalidId;
    EventId nextEvent = kInvalidId;

    Interval previousProtected;
    Interval nextProtected;
    Interval body;
    Interval join;

    double warpBudgetSamples = 0.0;
    double requestedCrossfadeSamples = 0.0;
    EditRegionStatus status = EditRegionStatus::ready;

    bool canCrossfade() const { return !join.empty(); }
    bool canWarpDecay() const { return !body.empty(); }
};

std::vector<EditRegion> buildEditRegionPlan(const Document& document,
                                             EditRegionPlanOptions options = {});

} // namespace beat::doc
