#include "SyntheticKit.h"
#include "doc/CrossMicMatcher.h"
#include "doc/SessionCalibration.h"
#include "doc/SpectralFluxDetector.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using Catch::Approx;
using namespace beat;
using namespace beat::doc;
using namespace beat::test;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kLength = 96000;

// Удары стоят через 250 мс: калибровка берёт только одиночные, и на плотной
// игре ей нечего мерить — это её условие, а не ограничение стенда.
const std::vector<int> kSnareHits { 6000, 18000, 30000, 42000, 54000, 66000, 78000, 90000 };

// Снейр слышен во всех трёх микрофонах: близкий, оверхед +240 сэмплов (5 мс),
// комната +960 (20 мс). Четвёртый канал — шум без единого удара: с ним
// калибровке нечего сопоставлять, и она обязана это признать.
std::vector<std::vector<float>> kit(float noiseChannel = 0.05f)
{
    KitInstrument snare;
    snare.hitSamples = kSnareHits;
    snare.decayPerSecond = 25.0f;
    snare.toneHz = 190.0f;
    snare.noiseMix = 0.6f;
    snare.arrivalSamples = { 0.0f, 240.0f, 960.0f, 0.0f };
    snare.gain = { 1.0f, 0.35f, 0.2f, 0.0f };

    KitSpec spec;
    spec.sampleRate = kRate;
    spec.numChannels = 4;
    spec.numSamples = kLength;
    spec.instruments = { snare };
    spec.noiseFloor = 0.0005f;

    auto channels = renderKit(spec);

    auto hiss = whiteNoise(kLength, 4242);
    for (int i = 0; i < kLength; ++i)
        channels[3][static_cast<size_t>(i)] += noiseChannel * hiss[static_cast<size_t>(i)];

    return channels;
}

struct Run
{
    std::vector<std::vector<float>> audio;
    SessionProfile profile;
    CalibrationReport report;
};

Run calibrate()
{
    Run run;
    run.audio = kit();

    std::vector<const float*> pointers(run.audio.size());
    for (size_t i = 0; i < run.audio.size(); ++i)
        pointers[i] = run.audio[i].data();

    CalibrationContext context;
    context.sampleRate = kRate;

    SpectralFluxDetector detector;
    SessionCalibration calibration;
    run.profile = calibration.run(detector, pointers.data(), static_cast<int>(run.audio.size()),
                                  kLength, context, &run.report);
    return run;
}
} // namespace

TEST_CASE("calibration recovers the delays that were planted")
{
    auto run = calibrate();

    REQUIRE(run.report.selected > 0);
    REQUIRE(run.profile.channelCount() == 4);

    const auto& overhead = run.profile.delay(0, 1);
    const auto& room = run.profile.delay(0, 2);

    CHECK(overhead.known);
    CHECK(overhead.medianSamples == Approx(240.0).margin(4.0));
    CHECK(room.known);
    CHECK(room.medianSamples == Approx(960.0).margin(4.0));

    // Геометрия пары постоянна: разброс обязан быть в единицах сэмплов, иначе
    // мерили не тот удар.
    CHECK(overhead.spreadSamples < 8.0);
    CHECK(room.spreadSamples < 8.0);
}

TEST_CASE("a pair the material cannot support stays unknown, not zero")
{
    auto run = calibrate();

    // Четвёртый канал — шум: удара там нет, сопоставлять нечего.
    CHECK_FALSE(run.profile.knows(0, 3));
    CHECK(run.report.rejected > 0);

    // Ноль сюда подставлять нельзя: он означает «микрофоны стоят рядом».
    std::array<double, kMaxChannels> priors {};
    run.profile.priors(0, priors);
    CHECK(priors[3] == 0.0);
    CHECK(priors[1] == Approx(240.0).margin(4.0));
}

TEST_CASE("the channel stands at zero against itself and is always known")
{
    auto run = calibrate();

    for (int ch = 0; ch < run.profile.channelCount(); ++ch)
    {
        INFO(ch);
        CHECK(run.profile.knows(ch, ch));
        CHECK(run.profile.delay(ch, ch).medianSamples == 0.0);
    }
}

TEST_CASE("the profile records how much quieter the bleed is")
{
    auto run = calibrate();

    // Оверхед слышит снейр на 0.35 от близкого — это −9.1 дБ.
    CHECK(run.profile.bleedDb(0, 1) == Approx(-9.1).margin(3.0));
    // Комната тише оверхеда: 0.2 против 0.35.
    CHECK(run.profile.bleedDb(0, 2) < run.profile.bleedDb(0, 1));

    CHECK(run.profile.channel(0).rms > 0.0f);
    CHECK(run.profile.channel(0).noiseFloor > 0.0f);
    CHECK(run.profile.channel(0).owned > 0);
}

TEST_CASE("the profile is what lets the matcher reach the room mic")
{
    auto run = calibrate();

    std::vector<const float*> pointers(run.audio.size());
    for (size_t i = 0; i < run.audio.size(); ++i)
        pointers[i] = run.audio[i].data();

    const auto observe = [&](bool withProfile)
    {
        Document document;

        AnalysisContext analysis;
        analysis.sampleRate = kRate;
        analysis.referenceChannel = 0;

        SpectralFluxDetector detector;
        for (auto& event : detector.analyze(pointers.data(), 4, kLength, analysis))
            document.addEvent(event);

        MatchContext context;
        context.sampleRate = kRate;
        if (withProfile)
            run.profile.priors(0, context.prior);

        CrossMicMatcher matcher;
        matcher.match(document, pointers.data(), 4, kLength, context);

        int seen = 0;
        double delay = 0.0;
        for (const auto& event : document.events())
            if (event.channels[2].present)
            {
                ++seen;
                delay = document.delays().raw(event.id, 2);
            }

        return std::pair<int, double> { seen, delay };
    };

    const auto blind = observe(false);
    const auto informed = observe(true);

    // Комната на 20 мс лежит за окном поиска в 4 метра: без профиля она
    // отсеивается, и это правильный ответ. С профилем — находится и уточняется.
    CHECK(blind.first == 0);
    CHECK(informed.first > 0);
    CHECK(informed.second == Approx(960.0).margin(4.0));
}
