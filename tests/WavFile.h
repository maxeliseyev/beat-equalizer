#pragma once

// Читатель WAV для стенда: PCM 16/24/32 бита и float32.
//
// Своя реализация, а не JUCE: `beat_tests` линкуется без JUCE вообще, и
// линейка, которой меряют реальный материал, не должна тянуть за собой
// половину фреймворка. Ходит по чанкам, а не по фиксированному смещению —
// Reaper пишет JUNK перед fmt, и файл с постоянным заголовком тут исключение.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace beat::test
{

struct WavInfo
{
    double sampleRate = 0.0;
    int numChannels = 0;
    int bitsPerSample = 0;
    bool isFloat = false;
    std::int64_t numFrames = 0;
    std::int64_t dataOffset = 0;
};

inline bool wavInfoOf(const std::string& path, WavInfo& info)
{
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
        return false;

    char riff[12] {};
    if (std::fread(riff, 1, 12, file) != 12 || std::memcmp(riff, "RIFF", 4) != 0
        || std::memcmp(riff + 8, "WAVE", 4) != 0)
    {
        std::fclose(file);
        return false;
    }

    bool haveFormat = false;
    std::int64_t dataBytes = 0;

    for (;;)
    {
        char id[4] {};
        std::uint32_t size = 0;
        if (std::fread(id, 1, 4, file) != 4 || std::fread(&size, 4, 1, file) != 1)
            break;

        const long body = std::ftell(file);

        if (std::memcmp(id, "fmt ", 4) == 0)
        {
            std::uint16_t format = 0;
            std::uint16_t channels = 0;
            std::uint16_t bits = 0;
            std::uint32_t rate = 0;

            if (std::fread(&format, 2, 1, file) != 1 || std::fread(&channels, 2, 1, file) != 1
                || std::fread(&rate, 4, 1, file) != 1 || std::fseek(file, 6, SEEK_CUR) != 0
                || std::fread(&bits, 2, 1, file) != 1)
                break;

            info.sampleRate = static_cast<double>(rate);
            info.numChannels = channels;
            info.bitsPerSample = bits;
            info.isFloat = (format == 3);
            haveFormat = true;
        }
        else if (std::memcmp(id, "data", 4) == 0)
        {
            info.dataOffset = body;
            dataBytes = size;
            break;
        }

        // Нечётный чанк дополняется байтом: без этого следующий заголовок
        // читается со сдвигом и файл выглядит битым.
        if (std::fseek(file, body + static_cast<long>(size + (size & 1u)), SEEK_SET) != 0)
            break;
    }

    std::fclose(file);

    if (!haveFormat || dataBytes <= 0 || info.numChannels <= 0 || info.bitsPerSample < 8)
        return false;

    info.numFrames = dataBytes / (info.numChannels * (info.bitsPerSample / 8));
    return true;
}

// Читает [from, from + count) кадров одного канала. Короче запрошенного —
// значит файл кончился раньше; это не ошибка, а факт про материал.
inline std::vector<float> wavRead(const std::string& path,
                                  std::int64_t from,
                                  std::int64_t count,
                                  int channel = 0)
{
    WavInfo info {};
    if (!wavInfoOf(path, info) || channel < 0 || channel >= info.numChannels)
        return {};

    from = std::clamp<std::int64_t>(from, 0, info.numFrames);
    count = std::clamp<std::int64_t>(count, 0, info.numFrames - from);
    if (count == 0)
        return {};

    const int bytes = info.bitsPerSample / 8;
    const int frameBytes = info.numChannels * bytes;

    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
        return {};

    std::vector<unsigned char> raw(static_cast<size_t>(count * frameBytes));
    if (std::fseek(file, static_cast<long>(info.dataOffset + from * frameBytes), SEEK_SET) != 0)
    {
        std::fclose(file);
        return {};
    }

    const size_t got = std::fread(raw.data(), 1, raw.size(), file);
    std::fclose(file);

    const std::int64_t frames = static_cast<std::int64_t>(got) / frameBytes;
    std::vector<float> out(static_cast<size_t>(frames));

    for (std::int64_t i = 0; i < frames; ++i)
    {
        const unsigned char* p = raw.data() + (i * frameBytes) + channel * bytes;

        if (info.isFloat && bytes == 4)
        {
            float value = 0.0f;
            std::memcpy(&value, p, 4);
            out[static_cast<size_t>(i)] = value;
        }
        else if (bytes == 2)
        {
            std::int16_t value = 0;
            std::memcpy(&value, p, 2);
            out[static_cast<size_t>(i)] = static_cast<float>(value) / 32768.0f;
        }
        else if (bytes == 3)
        {
            // 24 бита собираются в старшие разряды и сдвигаются обратно:
            // так знак разворачивается сам, без ветки на отрицательные.
            const std::int32_t value = (static_cast<std::int32_t>(p[0]) << 8)
                                       | (static_cast<std::int32_t>(p[1]) << 16)
                                       | (static_cast<std::int32_t>(p[2]) << 24);
            out[static_cast<size_t>(i)] = static_cast<float>(value >> 8) / 8388608.0f;
        }
        else if (bytes == 4)
        {
            std::int32_t value = 0;
            std::memcpy(&value, p, 4);
            out[static_cast<size_t>(i)] = static_cast<float>(value) / 2147483648.0f;
        }
    }

    return out;
}

} // namespace beat::test
