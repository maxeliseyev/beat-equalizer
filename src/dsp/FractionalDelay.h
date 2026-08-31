#pragma once

#include "Constants.h"

#include <vector>

namespace beat
{

class FractionalDelay
{
public:
    void prepare(double sampleRate, int numChannels);
    void reset();

    void setAppliedDelaySamples(int channel, float delaySamples);
    // Офлайн-рендер не должен слышать сглаживание: задержка встаёт сразу.
    void snapToTargets();
    void setInvert(int channel, bool shouldInvert);

    float processSample(int channel, float input);

    int interpolatorLatency() const { return kInterpolatorLatencySamples; }

private:
    float readLagrange(int channel, float delaySamples) const;

    double sampleRate = 48000.0;
    int numChannels = 0;
    int bufferLength = 0;
    float smoothCoeff = 1.0f;

    std::vector<std::vector<float>> buffers;
    std::vector<int> writePos;
    std::vector<float> targetDelay;
    std::vector<float> currentDelay;
    std::vector<char> invert;
};

} // namespace beat
