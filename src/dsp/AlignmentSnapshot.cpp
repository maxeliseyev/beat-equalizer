#include "AlignmentSnapshot.h"

#include <algorithm>

namespace beat
{

AlignmentSnapshot AlignmentSnapshot::identity(int numChannels)
{
    AlignmentSnapshot snapshot;
    snapshot.numChannels = std::clamp(numChannels, 0, kMaxChannels);
    snapshot.reference = 0;
    snapshot.latencySamples = 0;

    for (int i = 0; i < kMaxChannels; ++i)
    {
        snapshot.delaySamples[i] = 0.0f;
        snapshot.invert[i] = false;
        snapshot.rotatorCoeff[i] = 0.0f;
        snapshot.enabled[i] = i < snapshot.numChannels;
    }

    return snapshot;
}

} // namespace beat
