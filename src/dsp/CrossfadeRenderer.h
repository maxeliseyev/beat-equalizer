#pragma once

#include "Constants.h"

#include <array>
#include <vector>

namespace beat
{

// Offline renderer for delay jumps hidden by equal-amplitude crossfades.
// The edit points are sample-based so beat_dsp does not depend on beat_doc.
class CrossfadeRenderer
{
public:
    struct EditPoint
    {
        double timeSamples = 0.0;
        double joinStartSamples = 0.0;
        double joinEndSamples = 0.0;
        int referenceChannel = 0;
        std::array<float, kMaxChannels> delaySamples {};
        std::array<bool, kMaxChannels> valid {};

        void setDelay(int channel, float delay);
    };

    struct Options
    {
        double sampleRate = 48000.0;
        int numChannels = 0;
        int numSamples = 0;
        float strength = 1.0f;
        std::array<float, kMaxChannels> baseDelaySamples {};
    };

    struct Result
    {
        int numChannels = 0;
        int numSamples = 0;
        int editPointsUsed = 0;
        int crossfadesRendered = 0;
        int skippedEditPoints = 0;
        std::array<float, kMaxChannels> finalDelaySamples {};
    };

    Result render(const float* const* source,
                  float* const* destination,
                  const Options& options,
                  const std::vector<EditPoint>& editPoints) const;
};

} // namespace beat
