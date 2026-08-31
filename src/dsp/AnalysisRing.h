#pragma once

#include "Constants.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace beat
{

// Кольцевой буфер сырого входа для анализа: пишет аудиопоток блоками (один
// writer, без аллокаций), читает worker. Скоп рисует выход и живёт отдельно —
// здесь нужен именно вход, до задержки и инверсии.
class AnalysisRing
{
public:
    void prepare(int channels, int numSamples)
    {
        const int ch = std::clamp(channels, 0, kMaxChannels);
        const int len = std::max(numSamples, 0);

        data.assign(static_cast<size_t>(ch), std::vector<float>(static_cast<size_t>(len), 0.0f));
        preparedChannels = (len > 0) ? ch : 0;
        bufferLength = (preparedChannels > 0) ? len : 0;
        writePos.store(0, std::memory_order_relaxed);
        written.store(0, std::memory_order_relaxed);
    }

    void reset()
    {
        for (auto& channel : data)
            std::fill(channel.begin(), channel.end(), 0.0f);

        writePos.store(0, std::memory_order_relaxed);
        written.store(0, std::memory_order_relaxed);
    }

    int numChannels() const { return preparedChannels; }
    int length() const { return bufferLength; }
    std::int64_t samplesWritten() const { return written.load(std::memory_order_acquire); }

    // Аудиопоток. Каналы сверх подготовленных игнорируются.
    void write(const float* const* channels, int numChannels, int numSamples)
    {
        if (channels == nullptr || bufferLength <= 0 || numSamples <= 0)
            return;

        const int ch = std::min(numChannels, preparedChannels);
        int pos = writePos.load(std::memory_order_relaxed);
        int remaining = std::min(numSamples, bufferLength);
        int offset = numSamples - remaining;

        while (remaining > 0)
        {
            const int chunk = std::min(remaining, bufferLength - pos);

            for (int c = 0; c < ch; ++c)
                std::copy_n(channels[c] + offset, chunk, data[static_cast<size_t>(c)].begin() + pos);

            pos += chunk;
            if (pos >= bufferLength)
                pos = 0;

            offset += chunk;
            remaining -= chunk;
        }

        writePos.store(pos, std::memory_order_relaxed);
        written.fetch_add(numSamples, std::memory_order_release);
    }

    // Worker. Пишет channel-major: канал c начинается с dest + c * count.
    // Возвращает число реально скопированных сэмплов на канал.
    int readLast(float* dest, int channels, int count) const
    {
        if (dest == nullptr || bufferLength <= 0 || count <= 0)
            return 0;

        const int ch = std::min(channels, preparedChannels);
        const int have = static_cast<int>(std::min<std::int64_t>(samplesWritten(), bufferLength));
        const int n = std::min(count, have);
        if (n <= 0 || ch <= 0)
            return 0;

        int start = writePos.load(std::memory_order_acquire) - n;
        while (start < 0)
            start += bufferLength;

        for (int c = 0; c < ch; ++c)
        {
            const auto& src = data[static_cast<size_t>(c)];
            float* out = dest + static_cast<std::ptrdiff_t>(c) * count;
            int pos = start;

            for (int i = 0; i < n; ++i)
            {
                out[i] = src[static_cast<size_t>(pos)];
                if (++pos >= bufferLength)
                    pos = 0;
            }
        }

        return n;
    }

    static int capacityForSampleRate(double sampleRate, float seconds = kAnalysisSeconds)
    {
        if (sampleRate <= 0.0 || seconds <= 0.0f)
            return 0;

        return static_cast<int>(std::lround(static_cast<double>(seconds) * sampleRate));
    }

private:
    std::vector<std::vector<float>> data;
    int preparedChannels = 0;
    int bufferLength = 0;
    std::atomic<int> writePos { 0 };
    std::atomic<std::int64_t> written { 0 };
};

} // namespace beat
