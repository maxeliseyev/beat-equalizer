#include "RealKit.h"
#include "doc/CrossMicMatcher.h"
#include "doc/SessionCalibration.h"
#include "doc/SpectralFluxDetector.h"
#include "dsp/Envelope.h"
#include "dsp/GccPhat.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace beat;
using namespace beat::doc;
using namespace beat::test;

namespace
{
constexpr double kRate = 96000.0;

// Редкая часть партии: 10…50 с. Дальше игра плотнее, огибающая не успевает
// вернуться к полу — и линейка начинает мерить границу окна вместо прихода.
// Это свойство линейки, а не кита, и калибровку сессии придётся учить
// выбирать удары так же (лестница детекции, ступень 2).
constexpr double kCalibrationStartSec = 10.0;
constexpr double kCalibrationLengthSec = 40.0;

// Протокол задержек этого кита относительно верхнего микрофона снейра,
// миллисекунды. Числа мерены линейкой из RealKit.h и записаны в
// docs/real-kit-protocol.md — здесь они держат материал, а не наоборот.
constexpr double kProtocolMs[kKitMicCount] = {
    2.667, // kick
    0.000, // snare top
    0.281, // snare bottom
    1.229, // hat
    2.177, // ride
    0.958, // tom 1
    1.708, // tom 2
    14.656 // room
};

constexpr double kToleranceMs = 0.30; // ~10 см: сдвиг стойки между дублями
constexpr double kSpreadMs = 0.25;

struct Kit
{
    std::vector<std::vector<float>> audio;
    std::vector<std::vector<float>> envelopes;
    std::vector<float> floors;
    int numSamples = 0;
};

Kit loadKit(const std::string& dir, double startSec, double lengthSec)
{
    Kit kit;
    kit.audio.resize(kKitMicCount);
    kit.envelopes.resize(kKitMicCount);
    kit.floors.resize(kKitMicCount);

    for (int mic = 0; mic < kKitMicCount; ++mic)
    {
        const auto index = static_cast<size_t>(mic);
        kit.audio[index] = wavRead(kitPath(dir, mic),
                                   static_cast<std::int64_t>(startSec * kRate),
                                   static_cast<std::int64_t>(lengthSec * kRate));
        kit.envelopes[index] = followEnvelope(kit.audio[index].data(),
                                              static_cast<int>(kit.audio[index].size()),
                                              kRate,
                                              kEnvelopeAttackMs,
                                              kEnvelopeReleaseMs);
        kit.floors[index] = noiseFloorOf(kit.envelopes[index]);
        kit.numSamples = static_cast<int>(kit.audio[index].size());
    }

    return kit;
}

// Задержка микрофона относительно опоры на одиночных ударах опоры, сэмплы.
std::vector<double> measureDelays(const Kit& kit, int reference, int mic)
{
    const int guard = static_cast<int>(0.200 * kRate);
    const int search = static_cast<int>(0.060 * kRate);
    const auto hits = isolatedHits(kit.envelopes[static_cast<size_t>(reference)], guard, 8.0f);

    std::vector<double> delays;
    for (int hit : hits)
    {
        const double at = arrivalNear(kit.envelopes[static_cast<size_t>(reference)],
                                      kit.floors[static_cast<size_t>(reference)], hit, search);
        const double other = arrivalNear(kit.envelopes[static_cast<size_t>(mic)],
                                         kit.floors[static_cast<size_t>(mic)], hit, search);
        if (at < 0.0 || other < 0.0)
            continue;

        delays.push_back(other - at);
    }

    return delays;
}

double toMs(double samples)
{
    return samples * 1000.0 / kRate;
}

std::vector<const float*> pointersOf(const Kit& kit)
{
    std::vector<const float*> pointers(kKitMicCount);
    for (int mic = 0; mic < kKitMicCount; ++mic)
        pointers[static_cast<size_t>(mic)] = kit.audio[static_cast<size_t>(mic)].data();

    return pointers;
}

struct SourceDelayStats
{
    int observations = 0;
    double rawMedianMs = 0.0;
    double rawSpreadMs = 0.0;
    double residualMedianMs = 0.0;
    double residualSpreadMs = 0.0;
    double fullAlignOffsetMs = 0.0;
    double roomReturnOffsetMs = 0.0;
};

SourceDelayStats statsForSource(const Document& document,
                                int source,
                                int mic,
                                const DelayField& roomReturn)
{
    SourceDelayStats stats;
    const auto& delays = document.delays();
    std::vector<double> raw;
    std::vector<double> residual;
    std::vector<double> fullOffset;
    std::vector<double> returnedOffset;

    for (const auto& event : document.events())
    {
        if (event.referenceChannel != source || !delays.has(event.id, source)
            || !delays.has(event.id, mic))
            continue;

        const double sourceRaw = delays.raw(event.id, source);
        const double micRaw = delays.raw(event.id, mic);
        const double rawRelative = micRaw - sourceRaw;
        const double full =
            micRaw + delays.applied(event.id, mic) - sourceRaw - delays.applied(event.id, source);
        const double returned = micRaw + roomReturn.applied(event.id, mic) - sourceRaw
                                - roomReturn.applied(event.id, source);

        raw.push_back(rawRelative);
        residual.push_back(rawRelative - kProtocolMs[mic] * kRate / 1000.0);
        fullOffset.push_back(full);
        returnedOffset.push_back(returned);
    }

    stats.observations = static_cast<int>(raw.size());
    if (raw.empty())
        return stats;

    const double rawMedian = medianOf(raw);
    const double residualMedian = medianOf(residual);
    const double fullMedian = medianOf(fullOffset);
    const double returnedMedian = medianOf(returnedOffset);

    stats.rawMedianMs = toMs(rawMedian);
    stats.rawSpreadMs = toMs(madOf(raw, rawMedian));
    stats.residualMedianMs = toMs(residualMedian);
    stats.residualSpreadMs = toMs(madOf(residual, residualMedian));
    stats.fullAlignOffsetMs = toMs(fullMedian);
    stats.roomReturnOffsetMs = toMs(returnedMedian);
    return stats;
}

Document detectAndMatchSnareSource(const Kit& kit,
                                   const SessionProfile& profile,
                                   MatchReport& report)
{
    auto pointers = pointersOf(kit);

    Document document;
    AnalysisContext analysis;
    analysis.sampleRate = kRate;
    analysis.referenceChannel = kSnareTop;

    SpectralFluxDetector detector;
    for (auto& event : detector.analyze(pointers.data(), kKitMicCount, kit.numSamples, analysis))
        document.addEvent(event);

    MatchContext match;
    match.sampleRate = kRate;
    profile.priors(kSnareTop, match.prior);
    for (int mic = 0; mic < kKitMicCount; ++mic)
    {
        const auto index = static_cast<size_t>(mic);
        const auto& stat = profile.delay(kSnareTop, mic);
        match.priorKnown[index] = stat.known;
        match.priorSpread[index] = stat.spreadSamples;
    }

    CrossMicMatcher matcher;
    report = matcher.match(document, pointers.data(), kKitMicCount, kit.numSamples, match);
    return document;
}

int eventsOwnedBy(const Document& document, int source)
{
    int count = 0;
    for (const auto& event : document.events())
        if (event.referenceChannel == source)
            ++count;

    return count;
}
} // namespace

TEST_CASE("стемы реального кита лежат от одного нуля", "[real-kit]")
{
    const auto dir = realKitDir();
    if (dir.empty())
        SKIP("BEAT_REAL_KIT_DIR не задан: реальный кит в репозитории не лежит");

    std::int64_t frames = -1;
    for (int mic = 0; mic < kKitMicCount; ++mic)
    {
        WavInfo info {};
        INFO(kitNames()[mic]);
        REQUIRE(wavInfoOf(kitPath(dir, mic), info));
        CHECK(info.sampleRate == kRate);
        CHECK(info.numChannels == 1);

        // Экспорт «от одного нуля»: длины обязаны совпасть до сэмпла. Иначе
        // sessionOffsetSamples у стемов разный, и весь протокол задержек ниже
        // мерит не геометрию, а промах экспорта.
        if (frames < 0)
            frames = info.numFrames;
        CHECK(info.numFrames == frames);
    }

    CHECK(frames > static_cast<std::int64_t>(kRate * 60.0));
}

TEST_CASE("геометрия кита совпадает с записанным протоколом", "[real-kit]")
{
    const auto dir = realKitDir();
    if (dir.empty())
        SKIP("BEAT_REAL_KIT_DIR не задан: реальный кит в репозитории не лежит");

    const auto kit = loadKit(dir, kCalibrationStartSec, kCalibrationLengthSec);

    for (int mic = 0; mic < kKitMicCount; ++mic)
    {
        const auto delays = measureDelays(kit, kSnareTop, mic);
        INFO(kitNames()[mic]);
        REQUIRE(delays.size() >= 10);

        const double median = medianOf(delays);
        CHECK(std::abs(toMs(median) - kProtocolMs[mic]) < kToleranceMs);

        // Разброс — главное число здесь. Задержка пары «источник, микрофон»
        // геометрическая: она обязана держаться от удара к удару, иначе
        // априорной задержке грош цена и мерить её надо каждый раз заново.
        CHECK(toMs(madOf(delays, median)) < kSpreadMs);
    }
}

TEST_CASE("комнатный микрофон стоит дальше окна поиска по умолчанию", "[real-kit]")
{
    const auto dir = realKitDir();
    if (dir.empty())
        SKIP("BEAT_REAL_KIT_DIR не задан: реальный кит в репозитории не лежит");

    const auto kit = loadKit(dir, kCalibrationStartSec, kCalibrationLengthSec);
    const double roomMs = toMs(medianOf(measureDelays(kit, kSnareTop, kRoom)));

    const double windowMs = 1000.0 * static_cast<double>(maxLagSeconds(kDefaultMaxDistanceM));
    CHECK(roomMs > windowMs);

    // Расширять окно нельзя (инвариант 5): коррелятор залипнет на периоде
    // бочки. Дальний микрофон приходит с априорной задержкой — или не
    // приходит вовсе, и это честнее догадки.
    CHECK(roomMs < 1000.0 * static_cast<double>(maxLagSeconds(kMaxDistanceM)));
}

TEST_CASE("сверка находит комнату только с априорной задержкой", "[real-kit]")
{
    const auto dir = realKitDir();
    if (dir.empty())
        SKIP("BEAT_REAL_KIT_DIR не задан: реальный кит в репозитории не лежит");

    const auto kit = loadKit(dir, kCalibrationStartSec, 20.0);

    std::vector<const float*> pointers(kKitMicCount);
    for (int mic = 0; mic < kKitMicCount; ++mic)
        pointers[static_cast<size_t>(mic)] = kit.audio[static_cast<size_t>(mic)].data();

    const auto detect = [&](Document& document)
    {
        AnalysisContext context;
        context.sampleRate = kRate;
        context.referenceChannel = kSnareTop;

        SpectralFluxDetector detector;
        for (auto& event : detector.analyze(pointers.data(), kKitMicCount, kit.numSamples, context))
            document.addEvent(event);
    };

    const auto roomObservations = [&](Document& document)
    {
        int seen = 0;
        for (const auto& event : document.events())
            if (event.channels[static_cast<size_t>(kRoom)].present)
                ++seen;
        return seen;
    };

    CrossMicMatcher matcher;

    Document blind;
    detect(blind);
    REQUIRE(blind.events().size() > 20);

    MatchContext without;
    without.sampleRate = kRate;
    matcher.match(blind, pointers.data(), kKitMicCount, kit.numSamples, without);

    Document informed;
    detect(informed);

    MatchContext with;
    with.sampleRate = kRate;
    with.prior[static_cast<size_t>(kRoom)] = kProtocolMs[kRoom] * kRate / 1000.0;
    matcher.match(informed, pointers.data(), kKitMicCount, kit.numSamples, with);

    CHECK(roomObservations(informed) > roomObservations(blind));
}

// Кадр плюс окно поиска обязаны помещаться в БПФ, иначе свёртка круговая и
// оценка приходит из другого конца кадра. Порядок выводится из кадра, а не
// берётся константой в сэмплах: на 96 кГц тот же кадр вдвое длиннее, и на
// 48 кГц эта ошибка не проявлялась (инвариант 4).
TEST_CASE("кадр сверки помещается в БПФ на любой частоте")
{
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const int frame = static_cast<int>(std::lround(kMatchFrameMs * rate / 1000.0));
        const int refine = static_cast<int>(std::lround(kMatchRefineMs * rate / 1000.0));
        INFO(rate);
        CHECK(frame + 2 * refine <= (1 << fftOrderFor(frame + 2 * refine + 2)));

        // И движок этапа 1: кадр там равен всему БПФ, поэтому корреляции
        // отдан порядок на единицу больше.
        const int lag = maxLagSamples(kDefaultMaxDistanceM, rate);
        GccPhat gcc(kDefaultFftOrder + 1);
        CHECK(gcc.usableSamples(lag) >= (1 << kDefaultFftOrder));
    }
}

// Минимальный интервал между ударами держится на кадрах потока спектра, а
// событие получает время прихода — откат назад по огибающей. Два пика в
// 12–15 мс друг от друга откатываются в один и тот же приход, и в документ
// попадают два события с одинаковым временем. На синтетике удары стоят редко,
// поэтому видно это только на реальной игре.
TEST_CASE("детектор не ставит два события в один приход", "[real-kit]")
{
    const auto dir = realKitDir();
    if (dir.empty())
        SKIP("BEAT_REAL_KIT_DIR не задан: реальный кит в репозитории не лежит");

    const auto audio = wavRead(kitPath(dir, kSnareTop),
                               static_cast<std::int64_t>(30.0 * kRate),
                               static_cast<std::int64_t>(20.0 * kRate));
    REQUIRE(!audio.empty());

    const float* pointers[1] = { audio.data() };
    AnalysisContext context;
    context.sampleRate = kRate;
    context.referenceChannel = 0;

    SpectralFluxDetector detector;
    const auto events = detector.analyze(pointers, 1, static_cast<int>(audio.size()), context);
    REQUIRE(events.size() > 20);

    int tooClose = 0;
    for (size_t i = 1; i < events.size(); ++i)
        if (toMs(events[i].timeSamples - events[i - 1].timeSamples) < kOnsetMinIntervalMs)
            ++tooClose;

    CHECK(tooClose == 0);
}

// Сверка обязана уточнять предсказание, а не портить его.
//
// Так было не всегда: с отбеливанием PHAT и кадром в 85 мс уточнение
// разносило априорную задержку на порядок. У синтетического просачивания
// когерентность единица во всём кадре, у настоящего прямой путь когерентен
// первые единицы миллисекунд, дальше два микрофона слышат разные поля — а
// PHAT выбеливает все корзины одинаково, и переходный участок тонет в
// некогерентном хвосте. Отсюда `kPlainWeighting` и короткий кадр.
TEST_CASE("уточнение не портит априорную задержку", "[real-kit]")
{
    const auto dir = realKitDir();
    if (dir.empty())
        SKIP("BEAT_REAL_KIT_DIR не задан: реальный кит в репозитории не лежит");

    const auto kit = loadKit(dir, kCalibrationStartSec, kCalibrationLengthSec);

    const MatchSettings settings;
    const int guard = static_cast<int>(0.200 * kRate);
    const int search = static_cast<int>(0.060 * kRate);
    const int frame = static_cast<int>(std::lround(settings.frameMs * kRate / 1000.0));
    const int preRoll = static_cast<int>(std::lround(settings.preRollMs * kRate / 1000.0));
    const int refine = static_cast<int>(std::lround(settings.refineMs * kRate / 1000.0));

    const auto hits = isolatedHits(kit.envelopes[static_cast<size_t>(kSnareTop)], guard, 8.0f);
    GccPhat gcc(fftOrderFor(frame + 2 * refine + 2), settings.weighting);

    for (int mic : { kSnareBottom, kTom1, kHat, kKick, kRoom })
    {
        std::vector<double> priors;
        std::vector<double> refined;

        for (int hit : hits)
        {
            const double at = arrivalNear(kit.envelopes[static_cast<size_t>(kSnareTop)],
                                          kit.floors[static_cast<size_t>(kSnareTop)], hit, search);
            const double other = arrivalNear(kit.envelopes[static_cast<size_t>(mic)],
                                             kit.floors[static_cast<size_t>(mic)], hit, search);
            if (at < 0.0 || other < 0.0)
                continue;

            const double prior = other - at;
            const int from = static_cast<int>(at) - preRoll;
            const int to = static_cast<int>(std::lround(at + prior)) - preRoll;
            if (from < 0 || to < 0 || from + frame > kit.numSamples || to + frame > kit.numSamples)
                continue;

            const auto estimate = gcc.estimate(kit.audio[static_cast<size_t>(kSnareTop)].data() + from,
                                               kit.audio[static_cast<size_t>(mic)].data() + to,
                                               frame, refine, kRate);
            if (!estimate.valid)
                continue;

            priors.push_back(prior);
            refined.push_back(prior + static_cast<double>(estimate.lagSamples));
        }

        INFO(kitNames()[mic]);
        REQUIRE(priors.size() >= 10);

        // Уточнение сдвигает задержку меньше чем на десятую миллиметра звука
        // и не разбрасывает её: разброс остаётся того же порядка, что у
        // предсказания, а не растёт в десятки раз.
        const double priorSpread = toMs(madOf(priors, medianOf(priors)));
        const double refinedSpread = toMs(madOf(refined, medianOf(refined)));
        CHECK(refinedSpread < 0.15);
        CHECK(refinedSpread < priorSpread + 0.10);

        // И не уводит саму задержку: уточнение — это доли миллисекунды.
        CHECK(std::abs(toMs(medianOf(refined) - medianOf(priors))) < settings.refineMs);
    }
}

// Калибровка сессии, ступень 2 лестницы: первый проход собирает статистику
// записи. Проверяется не «посчиталось», а два свойства, которые и делают
// профиль пригодным — он воспроизводит записанную геометрию, и он отказывается
// знать то, чего материал не даёт померить.
namespace
{
SessionProfile calibrateKit(const Kit& kit, std::vector<int> references, CalibrationReport& report)
{
    std::vector<const float*> pointers(kKitMicCount);
    for (int mic = 0; mic < kKitMicCount; ++mic)
        pointers[static_cast<size_t>(mic)] = kit.audio[static_cast<size_t>(mic)].data();

    CalibrationContext context;
    context.sampleRate = kRate;
    context.references = std::move(references);

    SpectralFluxDetector detector;
    SessionCalibration calibration;
    return calibration.run(detector, pointers.data(), kKitMicCount, kit.numSamples,
                           context, &report);
}
} // namespace

TEST_CASE("калибровка воспроизводит записанный протокол задержек", "[real-kit]")
{
    const auto dir = realKitDir();
    if (dir.empty())
        SKIP("BEAT_REAL_KIT_DIR не задан: реальный кит в репозитории не лежит");

    const auto kit = loadKit(dir, kCalibrationStartSec, kCalibrationLengthSec);

    CalibrationReport report;
    const auto profile = calibrateKit(kit, { kSnareTop }, report);

    REQUIRE(report.selected > 10);
    CHECK(report.known == kKitMicCount - 1);

    for (int mic = 0; mic < kKitMicCount; ++mic)
    {
        if (mic == kSnareTop)
            continue;

        const auto& stat = profile.delay(kSnareTop, mic);
        INFO(kitNames()[mic]);
        CHECK(stat.known);
        CHECK(std::abs(toMs(stat.medianSamples) - kProtocolMs[mic]) < 0.10);
        CHECK(toMs(stat.spreadSamples) < 0.15);
    }

    // Просачивание записано вместе с задержками: хэт слышит снейр почти так же
    // громко, как его собственный микрофон, а бочка — на тридцать децибел тише.
    CHECK(profile.bleedDb(kSnareTop, kHat) > profile.bleedDb(kSnareTop, kKick));
    CHECK(profile.bleedDb(kSnareTop, kKick) < -20.0f);
}

TEST_CASE("профиль сходится сам с собой с двух сторон", "[real-kit]")
{
    const auto dir = realKitDir();
    if (dir.empty())
        SKIP("BEAT_REAL_KIT_DIR не задан: реальный кит в репозитории не лежит");

    const auto kit = loadKit(dir, kCalibrationStartSec, kCalibrationLengthSec);

    CalibrationReport report;
    const auto profile = calibrateKit(kit, { kKick, kSnareTop, kRoom }, report);

    // Взаимность: задержка пары, померенная с одного конца, обязана быть
    // минус задержкой с другого. Удары при этом разные — с опоры снейра
    // отбираются одни, с опоры комнаты другие, — поэтому совпадение здесь не
    // тавтология, а проверка самой линейки.
    int checked = 0;
    for (int from = 0; from < kKitMicCount; ++from)
        for (int to = 0; to < kKitMicCount; ++to)
        {
            if (from == to || !profile.knows(from, to) || !profile.knows(to, from))
                continue;

            INFO(kitNames()[from] << " <-> " << kitNames()[to]);
            CHECK(std::abs(toMs(profile.delay(from, to).medianSamples
                                + profile.delay(to, from).medianSamples)) < 0.10);
            ++checked;
        }

    REQUIRE(checked > 0);

    // И ни одна известная строка не спорит с протоколом.
    for (int mic = 0; mic < kKitMicCount; ++mic)
    {
        if (!profile.knows(kSnareTop, mic))
            continue;

        INFO(kitNames()[mic]);
        CHECK(std::abs(toMs(profile.delay(kSnareTop, mic).medianSamples) - kProtocolMs[mic]) < 0.10);
    }
}

TEST_CASE("не всякий канал годится в опору, и профиль это признаёт", "[real-kit]")
{
    const auto dir = realKitDir();
    if (dir.empty())
        SKIP("BEAT_REAL_KIT_DIR не задан: реальный кит в репозитории не лежит");

    const auto kit = loadKit(dir, kCalibrationStartSec, kCalibrationLengthSec);

    CalibrationReport report;
    const auto profile = calibrateKit(kit, { kKick }, report);

    // Ударов бочка даёт много, а задержек по ним не померить: собственный
    // низкочастотный хвост не даёт огибающей вернуться к полу, и откат
    // упирается в край окна. Профиль обязан остаться пустым, а не заполниться
    // медианами по мусору — ноль в априорной задержке означает «микрофоны
    // стоят рядом», и сверка поверит.
    REQUIRE(report.selected > 10);
    CHECK(report.known == 0);

    std::array<double, kMaxChannels> priors {};
    profile.priors(kKick, priors);
    for (int mic = 0; mic < kKitMicCount; ++mic)
        CHECK(priors[static_cast<size_t>(mic)] == 0.0);
}

TEST_CASE("snare source diagnostic separates close pair, bleed and room return",
          "[.real-kit-source][real-kit]")
{
    const auto dir = realKitDir();
    if (dir.empty())
        SKIP("BEAT_REAL_KIT_DIR не задан: реальный кит в репозитории не лежит");

    const auto kit = loadKit(dir, kCalibrationStartSec, kCalibrationLengthSec);

    CalibrationReport calibration;
    const auto profile = calibrateKit(kit, { kSnareTop }, calibration);

    MatchReport match;
    const auto document = detectAndMatchSnareSource(kit, profile, match);
    const int snareEvents = eventsOwnedBy(document, kSnareTop);

    auto returned = document.delays();
    returned.setReturn(kRoom, 1.0f);

    std::array<SourceDelayStats, kKitMicCount> stats {};
    for (int mic = 0; mic < kKitMicCount; ++mic)
        stats[static_cast<size_t>(mic)] = statsForSource(document, kSnareTop, mic, returned);

    const auto& bottom = stats[static_cast<size_t>(kSnareBottom)];
    REQUIRE(document.events().size() > 40);
    REQUIRE(snareEvents > 30);
    REQUIRE(bottom.observations > 30);
    CHECK(std::abs(bottom.rawMedianMs - kProtocolMs[kSnareBottom]) < 0.05);
    CHECK(bottom.rawSpreadMs < 0.05);
    CHECK(std::abs(bottom.residualMedianMs) < 0.05);
    CHECK(std::abs(bottom.fullAlignOffsetMs) < 0.05);

    const auto& room = stats[static_cast<size_t>(kRoom)];
    REQUIRE(room.observations > 10);
    CHECK(room.rawMedianMs > 1000.0 * static_cast<double>(maxLagSeconds(kDefaultMaxDistanceM)));
    CHECK(std::abs(room.rawMedianMs - kProtocolMs[kRoom]) < 0.05);
    CHECK(room.rawSpreadMs < 0.05);
    CHECK(std::abs(room.fullAlignOffsetMs) < 0.05);
    CHECK(std::abs(room.roomReturnOffsetMs - room.rawMedianMs) < 0.05);

    std::cout << "\nYAN9 snare source diagnostic\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "window: " << kCalibrationStartSec << "-"
              << kCalibrationStartSec + kCalibrationLengthSec << " s, ref snare top\n";
    std::cout << "detect/match: " << document.events().size() << " hits, "
              << snareEvents << " snare-owned, " << match.observations << " obs, "
              << calibration.known << " calibrated delays\n";
    std::cout << "mic            obs raw_ms  mad_ms  residual_ms  full_offset  room_return\n";
    std::cout << std::setprecision(3);
    for (int mic = 0; mic < kKitMicCount; ++mic)
    {
        const auto& row = stats[static_cast<size_t>(mic)];
        std::cout << std::left << std::setw(14) << kitNames()[mic] << std::right
                  << std::setw(4) << row.observations
                  << std::setw(8) << row.rawMedianMs
                  << std::setw(8) << row.rawSpreadMs
                  << std::setw(13) << row.residualMedianMs
                  << std::setw(13) << row.fullAlignOffsetMs
                  << std::setw(13) << row.roomReturnOffsetMs << "\n";
    }
}
