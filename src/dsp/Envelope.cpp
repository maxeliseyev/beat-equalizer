#include "Envelope.h"

#include <algorithm>
#include <cmath>

namespace beat
{

namespace
{
float onePoleCoefficient(float timeMs, double sampleRate)
{
    if (timeMs <= 0.0f || sampleRate <= 0.0)
        return 0.0f;

    const double tau = 0.001 * static_cast<double>(timeMs) * sampleRate;
    return static_cast<float>(std::exp(-1.0 / tau));
}
} // namespace

std::vector<float> followEnvelope(const float* samples,
                                  int numSamples,
                                  double sampleRate,
                                  float attackMs,
                                  float releaseMs)
{
    if (samples == nullptr || numSamples <= 0)
        return {};

    const float attack = onePoleCoefficient(attackMs, sampleRate);
    const float release = onePoleCoefficient(releaseMs, sampleRate);

    std::vector<float> envelope(static_cast<size_t>(numSamples), 0.0f);
    float state = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float value = std::abs(samples[i]);
        const float coefficient = value > state ? attack : release;
        state = value + coefficient * (state - value);
        envelope[static_cast<size_t>(i)] = state;
    }

    return envelope;
}

float noiseFloorOf(const std::vector<float>& envelope, float percentile)
{
    if (envelope.empty())
        return 0.0f;

    std::vector<float> sorted(envelope);
    const auto index = static_cast<size_t>(
        std::clamp(percentile, 0.0f, 1.0f) * static_cast<float>(sorted.size() - 1));

    std::nth_element(sorted.begin(), sorted.begin() + static_cast<long>(index), sorted.end());
    return sorted[index];
}

} // namespace beat
