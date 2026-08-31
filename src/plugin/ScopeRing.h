#pragma once

#include "dsp/Constants.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace beat
{

class ScopeRing
{
public:
    static constexpr int kLength = 65536;

    void reset()
    {
        for (auto& channel : data)
            channel.fill(0.0f);
        write.store(0, std::memory_order_relaxed);
    }

    void push(int numChannels, const float* samplePerChannel)
    {
        const int w = write.load(std::memory_order_relaxed);
        const int n = std::clamp(numChannels, 0, kMaxChannels);
        for (int ch = 0; ch < n; ++ch)
            data[static_cast<size_t>(ch)][static_cast<size_t>(w)] = samplePerChannel[ch];

        int next = w + 1;
        if (next >= kLength)
            next = 0;
        write.store(next, std::memory_order_release);
    }

    int writeIndex() const { return write.load(std::memory_order_acquire); }

    void copyLast(int channel, float* dest, int count) const
    {
        if (channel < 0 || channel >= kMaxChannels || dest == nullptr || count <= 0)
            return;

        count = std::min(count, kLength);
        int start = write.load(std::memory_order_acquire) - count;
        while (start < 0)
            start += kLength;

        const auto& src = data[static_cast<size_t>(channel)];
        for (int i = 0; i < count; ++i)
        {
            dest[i] = src[static_cast<size_t>(start)];
            ++start;
            if (start >= kLength)
                start = 0;
        }
    }

    static int windowSamples(float timeMs, double sampleRate)
    {
        if (sampleRate <= 0.0)
            return 64;

        const int n = static_cast<int>(std::lround(static_cast<double>(timeMs) * 0.001 * sampleRate));
        return std::clamp(n, 64, kLength);
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
    std::array<std::array<float, kLength>, kMaxChannels> data {};
    std::atomic<int> write { 0 };
};

} // namespace beat
