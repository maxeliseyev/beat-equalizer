#pragma once

#include <cmath>

namespace beat
{

inline constexpr int kMinChannels = 2;
inline constexpr int kMaxChannels = 24;
inline constexpr float kMinDistanceM = 0.5f;
inline constexpr float kMaxDistanceM = 10.0f;
inline constexpr float kDefaultMaxDistanceM = 4.0f;
inline constexpr float kSpeedOfSoundMps = 343.0f;
// Линия задержки обязана покрывать самую дальнюю дистанцию поиска, иначе
// автовыравнивание упрётся в потолок параметра на комнатном микрофоне.
inline constexpr float kMaxDelayMs = 1000.0f * kMaxDistanceM / kSpeedOfSoundMps + 1.0f;
inline constexpr float kAnalysisLowHz = 100.0f;
inline constexpr float kAnalysisHighHz = 8000.0f;
inline constexpr int kDefaultFftOrder = 13;
inline constexpr float kPhatEps = 1.0e-12f;
inline constexpr int kLagrangeOrder = 5;
inline constexpr int kInterpolatorLatencySamples = 2;
inline constexpr float kDelaySmoothMs = 5.0f;
inline constexpr float kMinScopeTimeMs = 5.0f;
inline constexpr float kMaxScopeTimeMs = 1000.0f;
inline constexpr float kDefaultScopeTimeMs = 40.0f;

// Analysis window: кадр FFT, hop 50 %, кольцевой буфер сырого входа.
inline constexpr float kAnalysisSeconds = 8.0f;
inline constexpr float kAnalysisMinRms = 0.0005f;
// Пик GCC к медиане окна поиска: ниже — кадр без внятного пика, не считаем.
// На синтетике пара «шум и его копия» даёт ~380, две независимые дорожки ~5.
// Порог калибруется на реальных китах (plan.md, PR 8).
inline constexpr float kAnalysisMinPeakRatio = 8.0f;
inline constexpr int kAnalysisMinFrames = 3;

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
