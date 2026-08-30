#pragma once

#include "Constants.h"

#include <algorithm>
#include <cmath>

namespace beat
{

struct LatencyModel
{
    static int interpolatorLatencySamples() { return kInterpolatorLatencySamples; }

    static int reportedLatency(float maxAppliedSamples)
    {
        if (maxAppliedSamples < 0.0f)
            maxAppliedSamples = 0.0f;

        return static_cast<int>(std::ceil(maxAppliedSamples)) + interpolatorLatencySamples();
    }

    // tdoa[i] > 0 means channel i is later than the reference.
    // applied[i] = max(tdoa) - tdoa[i] (>= 0). Disabled channels get 0.
    static void applyTdoa(const float* tdoa,
                          const bool* enabled,
                          int numChannels,
                          float* appliedOut,
                          int& latencySamples)
    {
        numChannels = std::clamp(numChannels, 0, kMaxChannels);

        float maxTdoa = 0.0f;
        for (int i = 0; i < numChannels; ++i)
        {
            if (enabled != nullptr && !enabled[i])
                continue;
            maxTdoa = std::max(maxTdoa, tdoa[i]);
        }

        for (int i = 0; i < numChannels; ++i)
        {
            if (enabled != nullptr && !enabled[i])
            {
                appliedOut[i] = 0.0f;
                continue;
            }

            appliedOut[i] = maxTdoa - tdoa[i];
            if (appliedOut[i] < 0.0f)
                appliedOut[i] = 0.0f;
        }

        latencySamples = reportedLatency(maxTdoa);
    }
};

} // namespace beat
