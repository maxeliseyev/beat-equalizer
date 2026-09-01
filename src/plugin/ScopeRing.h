#pragma once

#include "dsp/Constants.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace beat
{

class ScopeRing
{
public:
    static constexpr int kMinLength = 64;
    static constexpr int kMaxLength = 192000;

    ScopeRing() { prepare(4096); }

    void prepare(int numSamples)
    {
        const int n = std::clamp(numSamples, kMinLength, kMaxLength);
        for (auto& channel : data)
        {
            if ((int) channel.size() != n)
                channel.assign(static_cast<size_t>(n), 0.0f);
            else
                std::fill(channel.begin(), channel.end(), 0.0f);
        }
        write.store(0, std::memory_order_relaxed);
    }

    void reset()
    {
        for (auto& channel : data)
            std::fill(channel.begin(), channel.end(), 0.0f);
        write.store(0, std::memory_order_relaxed);
    }

    int length() const { return (int) data.front().size(); }

    void push(int numChannels, const float* samplePerChannel)
    {
        const int len = length();
        if (len <= 0 || samplePerChannel == nullptr)
            return;

        const int w = write.load(std::memory_order_relaxed);
        const int n = std::clamp(numChannels, 0, kMaxChannels);
        for (int ch = 0; ch < n; ++ch)
            data[static_cast<size_t>(ch)][static_cast<size_t>(w)] = samplePerChannel[ch];

        int next = w + 1;
        if (next >= len)
            next = 0;
        write.store(next, std::memory_order_release);
    }

    int writeIndex() const { return write.load(std::memory_order_acquire); }

    void copyLast(int channel, float* dest, int count) const
    {
        const int len = length();
        if (channel < 0 || channel >= kMaxChannels || dest == nullptr || count <= 0 || len <= 0)
            return;

        count = std::min(count, len);
        int start = write.load(std::memory_order_acquire) - count;
        while (start < 0)
            start += len;

        const auto& src = data[static_cast<size_t>(channel)];
        for (int i = 0; i < count; ++i)
        {
            dest[i] = src[static_cast<size_t>(start)];
            ++start;
            if (start >= len)
                start = 0;
        }
    }

    static int windowSamples(float timeMs, double sampleRate)
    {
        if (sampleRate <= 0.0)
            return kMinLength;

        const float clampedMs = std::clamp(timeMs, kMinScopeTimeMs, kMaxScopeTimeMs);
        const int n = static_cast<int>(std::lround(static_cast<double>(clampedMs) * 0.001 * sampleRate));
        return std::clamp(n, kMinLength, kMaxLength);
    }

    static int capacityForSampleRate(double sampleRate)
    {
        return windowSamples(kMaxRingTimeMs, sampleRate);
    }

    static int findRisingTrigger(const float* samples, int count, float threshold)
    {
        if (samples == nullptr || count < 2)
            return -1;

        for (int i = count - 1; i > 0; --i)
        {
            if (samples[i - 1] <= threshold && samples[i] > threshold)
                return i;
        }
        return -1;
    }

private:
    std::array<std::vector<float>, kMaxChannels> data {};
    std::atomic<int> write { 0 };
};

} // namespace beat
