#pragma once

#include "Constants.h"

namespace beat
{

struct AlignmentSnapshot
{
    int numChannels = 0;
    int reference = 0;
    float delaySamples[kMaxChannels] {};
    bool invert[kMaxChannels] {};
    float rotatorCoeff[kMaxChannels] {};
    float rotatorAmount[kMaxChannels] {}; // 0 = ротатор выключен
    bool enabled[kMaxChannels] {};
    int latencySamples = 0;

    static AlignmentSnapshot identity(int numChannels);
};

} // namespace beat
