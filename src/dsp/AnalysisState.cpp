#include "AnalysisState.h"

#include <cstring>

namespace beat
{

namespace
{
template <typename T>
void write(std::vector<std::uint8_t>& out, const T& value)
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

template <typename T>
bool read(const std::uint8_t*& cursor, const std::uint8_t* end, T& value)
{
    if (static_cast<std::size_t>(end - cursor) < sizeof(T))
        return false;

    std::memcpy(&value, cursor, sizeof(T));
    cursor += sizeof(T);
    return true;
}
} // namespace

std::vector<std::uint8_t> serializeAnalysis(const AlignmentEngine::Result& result,
                                            double sampleRate)
{
    std::vector<std::uint8_t> out;
    const std::int32_t channels = result.numChannels;

    write(out, kAnalysisStateVersion);
    write(out, sampleRate);
    write(out, channels);
    write(out, static_cast<std::int32_t>(result.reference));
    write(out, result.coherenceBefore);
    write(out, result.coherenceAfter);

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto& estimate = result.channels[static_cast<std::size_t>(ch)];
        write(out, estimate.tdoaSamples);
        write(out, estimate.confidence);
        write(out, estimate.coherenceBefore);
        write(out, estimate.coherenceAfter);
        write(out, estimate.rotatorHz);
        write(out, estimate.rotatorAmount);
        write(out, static_cast<std::int32_t>(estimate.framesUsed));
        write(out, static_cast<std::uint8_t>(estimate.invert ? 1 : 0));
        write(out, static_cast<std::uint8_t>(estimate.valid ? 1 : 0));
    }

    return out;
}

bool deserializeAnalysis(const std::uint8_t* data,
                         std::size_t size,
                         int expectedChannels,
                         AlignmentEngine::Result& result,
                         double& sampleRate)
{
    if (data == nullptr || size == 0)
        return false;

    const std::uint8_t* cursor = data;
    const std::uint8_t* end = data + size;

    std::int32_t version = 0;
    std::int32_t channels = 0;
    std::int32_t reference = 0;
    double storedRate = 0.0;
    float coherenceBefore = 0.0f;
    float coherenceAfter = 0.0f;

    if (!read(cursor, end, version) || version != kAnalysisStateVersion)
        return false;

    if (!read(cursor, end, storedRate) || !read(cursor, end, channels)
        || !read(cursor, end, reference) || !read(cursor, end, coherenceBefore)
        || !read(cursor, end, coherenceAfter))
        return false;

    if (channels < kMinChannels || channels > kMaxChannels || channels != expectedChannels)
        return false;

    AlignmentEngine::Result restored;
    restored.status = AnalysisStatus::ok;
    restored.numChannels = channels;
    restored.reference = (reference >= 0 && reference < channels) ? reference : 0;
    restored.coherenceBefore = coherenceBefore;
    restored.coherenceAfter = coherenceAfter;

    for (int ch = 0; ch < channels; ++ch)
    {
        auto& estimate = restored.channels[static_cast<std::size_t>(ch)];
        std::int32_t framesUsed = 0;
        std::uint8_t invert = 0;
        std::uint8_t valid = 0;

        if (!read(cursor, end, estimate.tdoaSamples) || !read(cursor, end, estimate.confidence)
            || !read(cursor, end, estimate.coherenceBefore)
            || !read(cursor, end, estimate.coherenceAfter)
            || !read(cursor, end, estimate.rotatorHz) || !read(cursor, end, estimate.rotatorAmount)
            || !read(cursor, end, framesUsed) || !read(cursor, end, invert)
            || !read(cursor, end, valid))
            return false;

        estimate.framesUsed = framesUsed;
        estimate.invert = invert != 0;
        estimate.valid = valid != 0;
    }

    result = restored;
    sampleRate = storedRate;
    return true;
}

} // namespace beat
