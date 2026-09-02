#include "SyntheticKit.h"
#include "doc/CrossMicMatcher.h"
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
const std::vector<int> kSnareHits { 6000, 18000, 30000, 42000 };
const std::vector<int> kHatHits { 12000, 24000, 36000 };

// Снейр слышен во всех трёх микрофонах: близкий, оверхед +5 мс, комната +20 мс.
// Хэт живёт в оверхеде и просачивается в близкий на −28 дБ.
std::vector<std::vector<float>> kit()
{
    KitInstrument snare;
    snare.hitSamples = kSnareHits;
    snare.decayPerSecond = 25.0f;
    snare.toneHz = 190.0f;
    snare.noiseMix = 0.6f;
    snare.arrivalSamples = { 0.0f, 240.0f, 960.0f };
    snare.gain = { 1.0f, 0.35f, 0.2f };

    KitInstrument hat;
    hat.hitSamples = kHatHits;
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
    return renderKit(spec);
}

struct Take
{
    std::vector<std::vector<float>> channels;
    std::vector<const float*> pointers;
    Document document;
    MatchReport report;

    const float* const* data() const { return pointers.data(); }
};

// Полный круг: детектор находит удары на опорном канале, документ принимает их,
// матчер достраивает наблюдения в остальных микрофонах.
Take run(int reference = 0, double roomPrior = 0.0, float minConfidence = kOnsetMinConfidence)
{
    Take take;
    take.channels = kit();
    for (const auto& channel : take.channels)
        take.pointers.push_back(channel.data());

    SpectralFluxSettings detectorSettings;
    detectorSettings.minConfidence = minConfidence;
    SpectralFluxDetector detector(detectorSettings);

    AnalysisContext analysis;
    analysis.sampleRate = kRate;
    analysis.referenceChannel = reference;

    for (const auto& event : detector.analyze(take.data(), 3, 48000, analysis))
        take.document.addEvent(event);

    CrossMicMatcher matcher;
    MatchContext context;
    context.sampleRate = kRate;
    context.prior[2] = roomPrior;

    take.report = matcher.match(take.document, take.data(), 3, 48000, context);
    return take;
}
} // namespace

TEST_CASE("the neighbour mic is predicted and refined, not searched from scratch")
{
    auto take = run();

    REQUIRE(take.document.events().size() == kSnareHits.size());
    CHECK(take.report.observations == 4);

    for (const auto& event : take.document.events())
    {
        // Оверхед: 240 сэмплов заложено, столько и достали.
        CHECK(take.document.delays().raw(event.id, 1) == Approx(240.0).margin(0.5));
        CHECK(event.channels[1].present);
        CHECK(event.channels[1].confidence > 0.9f);

        // Опора стоит в поле нулём: остальные меряются от неё.
        CHECK(take.document.delays().raw(event.id, 0) == 0.0);
    }
}

TEST_CASE("a mic further away than the search radius is rejected, not guessed")
{
    auto take = run();

    // Комната на 20 мс, а окно поиска — 4 метра, то есть 12 мс. Приход вне
    // физически допустимого окна не тот удар, и обсуждать нечего.
    CHECK(take.report.rejected == 4);

    for (const auto& event : take.document.events())
    {
        CHECK_FALSE(event.channels[2].present);
        CHECK_FALSE(take.document.delays().has(event.id, 2));
    }
}

TEST_CASE("a prior brings the room mic inside the window and gets refined to the truth")
{
    auto take = run(0, 900.0);

    CHECK(take.report.rejected == 0);
    CHECK(take.report.observations == 8);

    for (const auto& event : take.document.events())
    {
        // Предсказано 900, заложено 960: остаток нашёл GCC-PHAT.
        CHECK(take.document.delays().raw(event.id, 2) == Approx(960.0).margin(0.5));
        CHECK(observationCount(event) == 3);
    }
}

TEST_CASE("the hit belongs to the mic that heard it first and loudest")
{
    // Опорный канал — оверхед. В нём слышно и снейр, и хэт.
    auto take = run(1, 720.0);

    REQUIRE(take.document.events().size() == kSnareHits.size() + kHatHits.size());
    CHECK(take.report.reattributed == static_cast<int>(kSnareHits.size()));

    for (const auto& event : take.document.events())
    {
        const bool isSnare = std::any_of(kSnareHits.begin(), kSnareHits.end(), [&](int hit)
                                         { return std::abs(event.timeSamples - (hit + 240)) < 600; });

        // Снейр отдан близкому микрофону, хотя искали его в оверхеде: там он
        // и раньше, и энергичнее относительно собственного среднего.
        CHECK(event.referenceChannel == (isSnare ? 0 : 1));
    }
}

TEST_CASE("bleed stays a hit nobody else heard")
{
    // Порог уверенности снят: просачивание хэта в близкий микрофон возвращается.
    auto take = run(0, 900.0, 0.0f);

    REQUIRE(take.document.events().size() > kSnareHits.size());
    CHECK(take.report.singleChannel == 2);

    for (const auto& event : take.document.events())
    {
        const bool isSnare = std::any_of(kSnareHits.begin(), kSnareHits.end(), [&](int hit)
                                         { return std::abs(event.timeSamples - hit) < 600; });

        // Настоящий удар слышен во всех трёх; просачивание — только там, где
        // его нашли. Это и есть подпись просачивания, а не уровень.
        CHECK(observationCount(event) == (isSnare ? 3 : 1));
    }
}

TEST_CASE("the protected zone now spans every microphone")
{
    auto matched = run(0, 900.0);
    REQUIRE_FALSE(matched.document.events().empty());

    const auto& event = matched.document.events().front();
    const auto zone = protectedZone(event, kRate);

    Event alone;
    alone.channels[0] = event.channels[0];
    const auto singleMic = protectedZone(alone, kRate);

    // Комната отстаёт на 20 мс: зона обязана вырасти минимум на столько.
    CHECK(zone.length() > singleMic.length() + 0.015 * kRate);
    CHECK(zone.start == Approx(singleMic.start));
    CHECK(zone.contains(event.channels[2].arrivalSamples));
}

TEST_CASE("raw delays land in the field and alignment stays reversible")
{
    auto take = run(0, 900.0);
    auto& delays = take.document.delays();
    const auto id = take.document.events().front().id;

    REQUIRE(delays.raw(id, 2) == Approx(960.0).margin(0.5));

    // r = 0 — комната приходит одновременно с близким.
    CHECK(delays.applied(id, 2) == Approx(0.0).margin(0.5));
    CHECK(delays.applied(id, 0) == Approx(960.0).margin(0.5));

    // r = 1 — расстояние между микрофонами вернулось, сырой TDOA цел.
    delays.setReturn(2, 1.0f);
    CHECK(delays.applied(id, 2) == Approx(delays.applied(id, 0)).margin(0.5));
    CHECK(delays.raw(id, 2) == Approx(960.0).margin(0.5));
}

TEST_CASE("matching the same take twice gives the same delays")
{
    auto first = run(0, 900.0);
    auto second = run(0, 900.0);

    REQUIRE(first.document.events().size() == second.document.events().size());
    for (size_t i = 0; i < first.document.events().size(); ++i)
    {
        const auto a = first.document.events()[i].id;
        const auto b = second.document.events()[i].id;
        CHECK(first.document.delays().raw(a, 1) == second.document.delays().raw(b, 1));
        CHECK(first.document.delays().raw(a, 2) == second.document.delays().raw(b, 2));
    }
}

TEST_CASE("an empty document and a bad block are not a crash")
{
    Document empty;
    CrossMicMatcher matcher;
    MatchContext context;
    context.sampleRate = kRate;

    const auto channels = kit();
    std::vector<const float*> pointers;
    for (const auto& channel : channels)
        pointers.push_back(channel.data());

    CHECK(matcher.match(empty, pointers.data(), 3, 48000, context).observations == 0);
    CHECK(matcher.match(empty, nullptr, 3, 48000, context).observations == 0);
    CHECK(matcher.match(empty, pointers.data(), 0, 48000, context).observations == 0);
    CHECK(matcher.match(empty, pointers.data(), 3, 0, context).observations == 0);
}
