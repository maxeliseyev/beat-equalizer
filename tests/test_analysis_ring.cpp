#include <catch2/catch_test_macros.hpp>

#include "dsp/AnalysisRing.h"

#include <vector>

namespace
{
void writeRamp(beat::AnalysisRing& ring, int channels, int count, float start)
{
    std::vector<std::vector<float>> block(static_cast<size_t>(channels),
                                          std::vector<float>(static_cast<size_t>(count), 0.0f));
    std::vector<const float*> pointers(static_cast<size_t>(channels));

    for (int c = 0; c < channels; ++c)
    {
        for (int i = 0; i < count; ++i)
            block[static_cast<size_t>(c)][static_cast<size_t>(i)] =
                start + static_cast<float>(i) + 1000.0f * static_cast<float>(c);

        pointers[static_cast<size_t>(c)] = block[static_cast<size_t>(c)].data();
    }

    ring.write(pointers.data(), channels, count);
}
} // namespace

TEST_CASE("analysis ring returns the last samples in order across a wrap")
{
    beat::AnalysisRing ring;
    ring.prepare(2, 16);

    writeRamp(ring, 2, 10, 0.0f);
    writeRamp(ring, 2, 10, 10.0f);

    std::vector<float> out(2 * 8, -1.0f);
    REQUIRE(ring.readLast(out.data(), 2, 8) == 8);

    for (int i = 0; i < 8; ++i)
    {
        REQUIRE(out[static_cast<size_t>(i)] == 12.0f + static_cast<float>(i));
        REQUIRE(out[static_cast<size_t>(8 + i)] == 1012.0f + static_cast<float>(i));
    }
}

TEST_CASE("analysis ring reports how much material it really has")
{
    beat::AnalysisRing ring;
    ring.prepare(2, 64);
    REQUIRE(ring.samplesWritten() == 0);

    std::vector<float> out(2 * 32, 0.0f);
    REQUIRE(ring.readLast(out.data(), 2, 32) == 0);

    writeRamp(ring, 2, 20, 0.0f);
    REQUIRE(ring.samplesWritten() == 20);
    REQUIRE(ring.readLast(out.data(), 2, 32) == 20);
}

TEST_CASE("analysis ring keeps only the tail of an oversized block")
{
    beat::AnalysisRing ring;
    ring.prepare(1, 8);
    writeRamp(ring, 1, 32, 0.0f);

    std::vector<float> out(8, 0.0f);
    REQUIRE(ring.readLast(out.data(), 1, 8) == 8);
    REQUIRE(out.front() == 24.0f);
    REQUIRE(out.back() == 31.0f);
}

TEST_CASE("analysis ring capacity follows the sample rate, not a literal")
{
    REQUIRE(beat::AnalysisRing::capacityForSampleRate(48000.0)
            == static_cast<int>(beat::kAnalysisSeconds * 48000.0f));
    REQUIRE(beat::AnalysisRing::capacityForSampleRate(96000.0)
            == 2 * beat::AnalysisRing::capacityForSampleRate(48000.0));
    REQUIRE(beat::AnalysisRing::capacityForSampleRate(0.0) == 0);
}
