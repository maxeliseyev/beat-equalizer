#pragma once

#include <cmath>

namespace beat
{

inline constexpr int kMinChannels = 2;
inline constexpr int kMaxChannels = 24;
inline constexpr float kDefaultMaxDistanceM = 4.0f;
inline constexpr float kSpeedOfSoundMps = 343.0f;
inline constexpr float kMaxDelayMs = 20.0f;
inline constexpr float kAnalysisLowHz = 100.0f;
inline constexpr float kAnalysisHighHz = 8000.0f;
inline constexpr int kDefaultFftOrder = 13;
inline constexpr float kPhatEps = 1.0e-12f;

enum class ChannelRole
{
    unknown = 0,
    close,
    overhead,
    room,
    hats
};

enum class PolarityMode
{
    automatic = 0,
    positive,
    invert
};

inline float maxLagSeconds(float distanceM, float speedOfSound = kSpeedOfSoundMps)
{
    if (distanceM <= 0.0f || speedOfSound <= 0.0f)
        return 0.0f;

    return distanceM / speedOfSound;
}

inline int maxLagSamples(float distanceM, double sampleRate, float speedOfSound = kSpeedOfSoundMps)
{
    if (sampleRate <= 0.0)
        return 0;

    return static_cast<int>(std::ceil(static_cast<double>(maxLagSeconds(distanceM, speedOfSound)) * sampleRate));
}

} // namespace beat
