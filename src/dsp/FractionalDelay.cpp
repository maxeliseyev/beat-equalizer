#include "FractionalDelay.h"

#include <algorithm>
#include <cmath>

namespace beat
{

void FractionalDelay::prepare(double newSampleRate, int newNumChannels)
{
    sampleRate = std::max(newSampleRate, 1.0);
    numChannels = std::clamp(newNumChannels, 1, kMaxChannels);

    const int maxApplied = static_cast<int>(std::ceil(kMaxDelayMs * 0.001 * sampleRate));
    bufferLength = maxApplied + kInterpolatorLatencySamples + 16;

    buffers.assign(static_cast<size_t>(numChannels), std::vector<float>(static_cast<size_t>(bufferLength), 0.0f));
    writePos.assign(static_cast<size_t>(numChannels), 0);
    targetDelay.assign(static_cast<size_t>(numChannels), 0.0f);
    currentDelay.assign(static_cast<size_t>(numChannels), 0.0f);
    invert.assign(static_cast<size_t>(numChannels), 0);

    const float tauSamples = std::max(1.0f, kDelaySmoothMs * 0.001f * static_cast<float>(sampleRate));
    smoothCoeff = 1.0f - std::exp(-1.0f / tauSamples);

    reset();
}

void FractionalDelay::reset()
{
    for (auto& buffer : buffers)
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    std::fill(writePos.begin(), writePos.end(), 0);
    std::fill(currentDelay.begin(), currentDelay.end(), 0.0f);
}

void FractionalDelay::setAppliedDelaySamples(int channel, float delaySamples)
{
    if (channel < 0 || channel >= numChannels)
        return;

    const float maxApplied = static_cast<float>(bufferLength - kInterpolatorLatencySamples - 4);
    targetDelay[static_cast<size_t>(channel)] = std::clamp(delaySamples, 0.0f, maxApplied);
}

void FractionalDelay::snapToTargets()
{
    std::copy(targetDelay.begin(), targetDelay.end(), currentDelay.begin());
}

void FractionalDelay::setInvert(int channel, bool shouldInvert)
{
    if (channel < 0 || channel >= numChannels)
        return;

    invert[static_cast<size_t>(channel)] = shouldInvert ? 1 : 0;
}

float FractionalDelay::processSample(int channel, float input)
{
    if (channel < 0 || channel >= numChannels || bufferLength <= 0)
        return input;

    const auto ch = static_cast<size_t>(channel);
    currentDelay[ch] += (targetDelay[ch] - currentDelay[ch]) * smoothCoeff;

    auto& buffer = buffers[ch];
    const int w = writePos[ch];
    buffer[static_cast<size_t>(w)] = input;

    const float totalDelay = currentDelay[ch] + static_cast<float>(kInterpolatorLatencySamples);
    float output = readLagrange(channel, totalDelay);

    if (invert[ch] != 0)
        output = -output;

    writePos[ch] = w + 1;
    if (writePos[ch] >= bufferLength)
        writePos[ch] = 0;

    return output;
}

float FractionalDelay::readLagrange(int channel, float delaySamples) const
{
    const auto ch = static_cast<size_t>(channel);
    const auto& buffer = buffers[ch];
    const int w = writePos[ch];

    const float minDelay = static_cast<float>(kInterpolatorLatencySamples);
    const float maxDelay = static_cast<float>(bufferLength - 4);
    delaySamples = std::clamp(delaySamples, minDelay, maxDelay);

    const int n = static_cast<int>(std::floor(delaySamples));
    const float f = delaySamples - static_cast<float>(n);

    float x[6];
    for (int k = -2; k <= 3; ++k)
    {
        int index = w - (n + k);
        while (index < 0)
            index += bufferLength;
        while (index >= bufferLength)
            index -= bufferLength;
        x[k + 2] = buffer[static_cast<size_t>(index)];
    }

    const float nodes[6] = { -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };
    float y = 0.0f;
    for (int i = 0; i < 6; ++i)
    {
        float l = 1.0f;
        for (int j = 0; j < 6; ++j)
        {
            if (i == j)
                continue;
            l *= (f - nodes[j]) / (nodes[i] - nodes[j]);
        }
        y += x[i] * l;
    }

    return y;
}

} // namespace beat
