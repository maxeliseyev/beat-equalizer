#pragma once

#include "Constants.h"

#include <array>
#include <vector>

namespace beat
{

// Offline per-event delay renderer. Events provide absolute applied delays per
// channel; the renderer holds delay through protected attacks and slews only in
// the decay between events.
class GlideRenderer
{
public:
    struct EventDelay
    {
        double timeSamples = 0.0;
        double protectUntilSamples = 0.0;
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
        float maxSlew = kGlideMaxSlew;
    };

    struct EventMetric
    {
        double timeSamples = 0.0;
        int referenceChannel = 0;
        int channelsMeasured = 0;
        bool limited = false;
        float coherenceBefore = 0.0f;
        float coherenceAfter = 0.0f;
        std::array<float, kMaxChannels> actualDelaySamples {};
        std::array<float, kMaxChannels> targetDelaySamples {};
    };

    struct Result
    {
        int numChannels = 0;
        int numSamples = 0;
        int eventsMeasured = 0;
        int limitedEvents = 0;
        float maxSlewObserved = 0.0f;
        std::array<float, kMaxChannels> finalDelaySamples {};
        std::vector<EventMetric> events;
    };

    Result render(const float* const* source,
                  float* const* destination,
                  const Options& options,
                  const std::vector<EventDelay>& events) const;
};

} // namespace beat
