#include "OnsetMetrics.h"
#include "SyntheticKit.h"
#include "doc/Document.h"
#include "doc/ProtectedZone.h"
#include "doc/SpectralFluxDetector.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using Catch::Approx;
using namespace beat;
using namespace beat::doc;
using namespace beat::test;

namespace
{
constexpr double kRate = 48000.0;
const std::vector<int> kHits { 6000, 18000, 30000, 42000 };

// Снейр в три микрофона плюс хэт, который в близкий только просачивается.
KitSpec kit()
{
    KitInstrument snare;
    snare.hitSamples = kHits;
    snare.decayPerSecond = 25.0f;
    snare.toneHz = 190.0f;
    snare.noiseMix = 0.6f;
    snare.arrivalSamples = { 0.0f, 240.0f, 960.0f };
    snare.gain = { 1.0f, 0.35f, 0.2f };

    KitInstrument hat;
    hat.hitSamples = { 12000, 24000, 36000 };
    hat.decayPerSecond = 60.0f;
    hat.toneHz = 4000.0f;
    hat.noiseMix = 0.9f;
    hat.arrivalSamples = { 300.0f, 120.0f, 900.0f };
    hat.gain = { 0.04f, 0.5f, 0.15f };

    KitSpec spec;
    spec.sampleRate = kRate;
    spec.numChannels = 3;
    spec.numSamples = 48000;
    spec.instruments = { snare, hat };
    spec.noiseFloor = 0.0005f;
    spec.seed = 2026;
    return spec;
}

std::vector<const float*> pointersOf(const std::vector<std::vector<float>>& channels)
{
    std::vector<const float*> pointers;
    for (const auto& channel : channels)
        pointers.push_back(channel.data());

    return pointers;
}

std::vector<double> timesOf(const std::vector<Event>& events)
{
    std::vector<double> times;
    for (const auto& event : events)
        times.push_back(event.timeSamples);

    return times;
}
} // namespace

TEST_CASE("detector finds every hit on the close mic and invents none")
{
    const auto channels = renderKit(kit());
    const auto pointers = pointersOf(channels);

    SpectralFluxDetector detector;
    AnalysisContext context;
    context.sampleRate = kRate;
    context.referenceChannel = 0;

    const auto events = detector.analyze(pointers.data(), 3, 48000, context);

    std::vector<double> truth;
    for (int hit : kHits)
        truth.push_back(static_cast<double>(hit));

    // Допуск 10 мс: детектор отвечает «где примерно», субсэмплевое время потом
    // даёт GCC-PHAT.
    const auto match = matchOnsets(timesOf(events), truth, 0.010 * kRate);

    INFO("precision " << match.precision() << " recall " << match.recall()
                      << " worst error " << match.worstErrorSamples << " samples");
    CHECK(match.recall() == 1.0);
    CHECK(match.precision() == 1.0);
    CHECK(match.medianErrorSamples < 0.005 * kRate);
}

TEST_CASE("what the confidence floor drops is the hat bleeding into the close mic")
{
    const auto channels = renderKit(kit());
    const auto pointers = pointersOf(channels);

    SpectralFluxSettings generous;
    generous.minConfidence = 0.0f;
    SpectralFluxDetector detector(generous);

    AnalysisContext context;
    context.sampleRate = kRate;

    const auto events = detector.analyze(pointers.data(), 3, 48000, context);
    REQUIRE(events.size() > kHits.size());

    float loudestBleed = 0.0f;
    float quietestHit = 1.0f;

    for (const auto& event : events)
    {
        const bool isSnare = std::any_of(kHits.begin(), kHits.end(), [&](int hit)
                                         { return std::abs(event.timeSamples - hit) < 0.010 * kRate; });

        if (isSnare)
        {
            quietestHit = std::min(quietestHit, event.confidence);
            continue;
        }

        loudestBleed = std::max(loudestBleed, event.confidence);

        // Лишнее — это хэт, и ничто другое: он громче всего в оверхеде, а не в
        // канале, где его нашли. Отсеет это ступень 3, а не порог.
        CHECK(event.energy[1] > event.energy[0]);
    }

    // Уверенность разводит их на порядок. Именно поэтому она число, а не флаг:
    // порог отсечения двигается потом, без нового анализа.
    CHECK(loudestBleed > 0.0f);
    CHECK(quietestHit > 10.0f * loudestBleed);
}

TEST_CASE("detector reports confidence as a number, not a flag")
{
    const auto channels = renderKit(kit());
    const auto pointers = pointersOf(channels);

    SpectralFluxDetector detector;
    AnalysisContext context;
    context.sampleRate = kRate;

    const auto events = detector.analyze(pointers.data(), 3, 48000, context);
    REQUIRE_FALSE(events.empty());

    for (const auto& event : events)
    {
        CHECK(event.origin == Origin::detector);
        CHECK(event.confidence > 0.0f);
        CHECK(event.confidence < 1.0f);
        CHECK(event.kind == HitKind::unknown);   // тип называет не этот детектор
    }
}

TEST_CASE("the hit is measured only where it was looked for")
{
    const auto channels = renderKit(kit());
    const auto pointers = pointersOf(channels);

    SpectralFluxDetector detector;
    AnalysisContext context;
    context.sampleRate = kRate;
    context.referenceChannel = 0;

    const auto events = detector.analyze(pointers.data(), 3, 48000, context);
    REQUIRE(events.size() >= 4);

    // Соседние микрофоны этот детектор не заполняет: там удар предсказывается
    // по d[i], а это следующий шаг.
    CHECK(observationCount(events.front()) == 1);

    const auto& observation = events.front().channels[0];
    CHECK(observation.present);
    CHECK(observation.attackEndSamples > observation.arrivalSamples);
    CHECK(observation.usefulEndSamples > observation.attackEndSamples);
    CHECK(observation.decayDbPerSecond > 0.0f);

    // Перцептивная атака лежит между приходом и концом атаки и не совпадает с
    // приходом: это два разных числа (инвариант 17).
    CHECK(observation.perceptualAttackSamples >= observation.arrivalSamples);
    CHECK(observation.perceptualAttackSamples <= observation.attackEndSamples);
}

TEST_CASE("energy vector points at the microphone that heard the hit loudest")
{
    const auto channels = renderKit(kit());
    const auto pointers = pointersOf(channels);

    SpectralFluxDetector detector;
    AnalysisContext context;
    context.sampleRate = kRate;

    const auto events = detector.analyze(pointers.data(), 3, 48000, context);
    REQUIRE_FALSE(events.empty());

    const auto& energy = events.front().energy;
    CHECK(energy[0] == Approx(1.0f));
    CHECK(energy[1] < energy[0]);
    CHECK(energy[2] < energy[1]);
}

TEST_CASE("heavy features land in the cache for every channel, not just the reference")
{
    const auto channels = renderKit(kit());
    const auto pointers = pointersOf(channels);

    Document document;
    Source source;
    source.sampleRate = kRate;
    source.numChannels = 3;
    const auto id = document.addSource(std::move(source));

    SpectralFluxDetector detector;
    AnalysisContext context;
    context.sampleRate = kRate;
    context.features = &document.features();
    context.source = id;

    const auto events = detector.analyze(pointers.data(), 3, 48000, context);
    REQUIRE_FALSE(events.empty());

    for (int ch = 0; ch < 3; ++ch)
    {
        CHECK(document.features().contains(id, ch, FeatureKind::spectralFlux));
        CHECK(document.features().contains(id, ch, FeatureKind::envelope));
        CHECK(document.features().contains(id, ch, FeatureKind::noiseFloor));
        CHECK(document.features().contains(id, ch, FeatureKind::bandEnvelope, kOnsetBands - 1));
    }

    // Смена порогов выбрасывает решения и не трогает признаки.
    for (auto& event : events)
        document.addEvent(event);

    REQUIRE(document.events().size() == events.size());
    document.clearEvents();
    CHECK(document.events().empty());
    CHECK(document.features().contains(id, 0, FeatureKind::spectralFlux));
}

TEST_CASE("the same input gives the same events, bit for bit")
{
    const auto channels = renderKit(kit());
    const auto pointers = pointersOf(channels);

    SpectralFluxDetector detector;
    AnalysisContext context;
    context.sampleRate = kRate;

    const auto first = detector.analyze(pointers.data(), 3, 48000, context);
    const auto second = detector.analyze(pointers.data(), 3, 48000, context);

    REQUIRE(first.size() == second.size());
    for (size_t i = 0; i < first.size(); ++i)
    {
        CHECK(first[i].timeSamples == second[i].timeSamples);
        CHECK(first[i].confidence == second[i].confidence);
    }
}

TEST_CASE("events carry the offset from session zero, not from the block")
{
    const auto channels = renderKit(kit());
    const auto pointers = pointersOf(channels);

    SpectralFluxDetector detector;
    AnalysisContext context;
    context.sampleRate = kRate;

    const auto atZero = detector.analyze(pointers.data(), 3, 48000, context);

    context.startSample = 96000.0;
    const auto shifted = detector.analyze(pointers.data(), 3, 48000, context);

    REQUIRE(atZero.size() == shifted.size());
    for (size_t i = 0; i < atZero.size(); ++i)
        CHECK(shifted[i].timeSamples == Approx(atZero[i].timeSamples + 96000.0));
}

TEST_CASE("the detector stamp says what the numbers were made with")
{
    SpectralFluxDetector detector;
    Document document;
    document.setDetectorStamp({ detector.name(), detector.version(), detector.parameters() });

    CHECK(document.detectorStamp().name == "spectral-flux");
    CHECK(document.detectorStamp().parameters.find("bands=8") != std::string::npos);
    CHECK(document.detectorStamp().parameters.find("factor=") != std::string::npos);
}

TEST_CASE("measured attacks feed the protected zone")
{
    const auto channels = renderKit(kit());
    const auto pointers = pointersOf(channels);

    SpectralFluxDetector detector;
    AnalysisContext context;
    context.sampleRate = kRate;

    const auto events = detector.analyze(pointers.data(), 3, 48000, context);
    REQUIRE_FALSE(events.empty());

    const auto zone = protectedZone(events.front(), kRate);
    CHECK_FALSE(zone.empty());
    CHECK(zone.contains(events.front().timeSamples));

    // Пока зона построена по одному микрофону — она короче настоящей, и это
    // ровно то, что чинит следующий шаг.
    CHECK(zone.length() < 0.150 * kRate);
}
