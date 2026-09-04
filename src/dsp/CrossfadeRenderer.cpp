#include "CrossfadeRenderer.h"

#include "FractionalDelay.h"

#include <algorithm>
#include <cmath>

namespace beat
{

namespace
{
float clampDelay(float delaySamples, double sampleRate)
{
    const float maxDelay = sampleRate > 0.0
                               ? kMaxDelayMs * 0.001f * static_cast<float>(sampleRate)
                               : 0.0f;
    return std::clamp(delaySamples, 0.0f, maxDelay);
}

void copyThrough(const float* const* source, float* const* destination, int channels, int samples)
{
    for (int ch = 0; ch < channels; ++ch)
    {
        if (source[ch] == nullptr || destination[ch] == nullptr)
            continue;

        std::copy(source[ch], source[ch] + samples, destination[ch]);
    }
}

bool hasDelay(const CrossfadeRenderer::EditPoint& point, int numChannels)
{
    for (int ch = 0; ch < numChannels; ++ch)
        if (point.valid[static_cast<size_t>(ch)])
            return true;

    return false;
}

std::vector<CrossfadeRenderer::EditPoint> sortedEditPoints(
    const std::vector<CrossfadeRenderer::EditPoint>& editPoints,
    int numChannels,
    int numSamples,
    int& skipped)
{
    std::vector<CrossfadeRenderer::EditPoint> sorted;
    sorted.reserve(editPoints.size());

    for (auto point : editPoints)
    {
        if (!std::isfinite(point.timeSamples) || point.timeSamples < 0.0
            || point.timeSamples >= static_cast<double>(numSamples)
            || !hasDelay(point, numChannels))
        {
            ++skipped;
            continue;
        }

        point.referenceChannel = std::clamp(point.referenceChannel, 0, numChannels - 1);
        sorted.push_back(point);
    }

    std::sort(sorted.begin(),
              sorted.end(),
              [](const auto& a, const auto& b) { return a.timeSamples < b.timeSamples; });

    std::vector<CrossfadeRenderer::EditPoint> usable;
    usable.reserve(sorted.size());

    for (const auto& point : sorted)
    {
        if (usable.empty())
        {
            usable.push_back(point);
            continue;
        }

        if (!std::isfinite(point.joinStartSamples) || !std::isfinite(point.joinEndSamples)
            || !(point.joinEndSamples > point.joinStartSamples))
        {
            ++skipped;
            continue;
        }

        usable.push_back(point);
    }

    return usable;
}

std::vector<std::array<float, kMaxChannels>> buildTargets(
    const std::vector<CrossfadeRenderer::EditPoint>& editPoints,
    int numChannels,
    double sampleRate,
    float strength,
    const std::array<float, kMaxChannels>& baseDelaySamples)
{
    std::vector<std::array<float, kMaxChannels>> targets(editPoints.size());
    std::array<float, kMaxChannels> current {};
    const float amount = std::clamp(strength, 0.0f, 1.0f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto channel = static_cast<size_t>(ch);
        current[channel] = clampDelay(baseDelaySamples[channel], sampleRate) * amount;
    }

    for (size_t index = 0; index < editPoints.size(); ++index)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto channel = static_cast<size_t>(ch);
            if (editPoints[index].valid[channel])
                current[channel] =
                    clampDelay(editPoints[index].delaySamples[channel], sampleRate) * amount;
        }

        targets[index] = current;
    }

    return targets;
}

float blendAmount(const CrossfadeRenderer::EditPoint& point, int sample)
{
    const double width = point.joinEndSamples - point.joinStartSamples;
    if (width <= 0.0)
        return 0.0f;

    const double position = (static_cast<double>(sample) - point.joinStartSamples) / width;
    return static_cast<float>(std::clamp(position, 0.0, 1.0));
}
} // namespace

void CrossfadeRenderer::EditPoint::setDelay(int channel, float delay)
{
    if (channel < 0 || channel >= kMaxChannels)
        return;

    const auto index = static_cast<size_t>(channel);
    delaySamples[index] = delay;
    valid[index] = true;
}

CrossfadeRenderer::Result CrossfadeRenderer::render(
    const float* const* source,
    float* const* destination,
    const Options& options,
    const std::vector<EditPoint>& editPoints) const
{
    Result result;
    result.numChannels = std::clamp(options.numChannels, 0, kMaxChannels);
    result.numSamples = std::max(0, options.numSamples);

    if (source == nullptr || destination == nullptr || result.numChannels <= 0
        || result.numSamples <= 0 || options.sampleRate <= 0.0)
        return result;

    int skipped = 0;
    auto usable = sortedEditPoints(editPoints, result.numChannels, result.numSamples, skipped);
    result.skippedEditPoints = skipped;

    if (options.strength <= 0.0f || usable.empty())
    {
        copyThrough(source, destination, result.numChannels, result.numSamples);
        return result;
    }

    auto targets = buildTargets(usable,
                                result.numChannels,
                                options.sampleRate,
                                options.strength,
                                options.baseDelaySamples);

    FractionalDelay fromDelay;
    FractionalDelay toDelay;
    fromDelay.prepare(options.sampleRate, result.numChannels);
    toDelay.prepare(options.sampleRate, result.numChannels);
    fromDelay.reset();
    toDelay.reset();

    result.editPointsUsed = static_cast<int>(usable.size());

    size_t currentIndex = 0;
    size_t nextIndex = 1;
    for (int sample = 0; sample < result.numSamples; ++sample)
    {
        while (nextIndex < usable.size()
               && static_cast<double>(sample) >= usable[nextIndex].joinEndSamples)
        {
            currentIndex = nextIndex;
            ++nextIndex;
            ++result.crossfadesRendered;
        }

        const bool inCrossfade =
            nextIndex < usable.size()
            && static_cast<double>(sample) >= usable[nextIndex].joinStartSamples
            && static_cast<double>(sample) < usable[nextIndex].joinEndSamples;
        const float mix = inCrossfade ? blendAmount(usable[nextIndex], sample) : 0.0f;
        const size_t toIndex = nextIndex < usable.size() ? nextIndex : currentIndex;

        for (int ch = 0; ch < result.numChannels; ++ch)
        {
            if (source[ch] == nullptr || destination[ch] == nullptr)
                continue;

            const auto channel = static_cast<size_t>(ch);
            const float input = source[ch][sample];
            const float from = fromDelay.processSampleAtDelay(
                ch,
                input,
                targets[currentIndex][channel]);
            const float to = toDelay.processSampleAtDelay(ch, input, targets[toIndex][channel]);

            destination[ch][sample] = from + (to - from) * mix;
        }
    }

    result.finalDelaySamples = targets[currentIndex];
    return result;
}

} // namespace beat
