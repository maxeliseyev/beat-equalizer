#pragma once

#include "doc/OnsetDetector.h"
#include "doc/SessionProfile.h"
#include "dsp/Constants.h"

#include <vector>

namespace beat::doc
{

struct CalibrationSettings
{
    // Какие удары годятся в калибровку.
    float minConfidence = kCalibrationMinConfidence;
    float isolationMs = kCalibrationIsolationMs;
    // Опора обязана быть громче остальных каналов: иначе удар, скорее всего,
    // не её. Запас нулевой — на близко подзвученном ките нижний микрофон
    // барабана отстаёт от верхнего на единицы децибел, и требовать больше
    // значит остаться без ударов вовсе.
    float dominanceDb = 0.0f;

    float searchDistanceM = kCalibrationDistanceM;
    float maxSpreadMs = kCalibrationMaxSpreadMs;
    int minHits = kCalibrationMinHits;

    float attackMs = kEnvelopeAttackMs;
    float releaseMs = kEnvelopeReleaseMs;
    float arrivalMarginDb = kUsefulEndMarginDb;
    float energyWindowMs = kOnsetEnergyWindowMs;
};

struct CalibrationContext
{
    double sampleRate = 48000.0;
    SamplePos startSample = 0.0;

    // Какие каналы брать опорами. Пусто — все. Это не ветка по роли
    // (инвариант 8), а параметр вызывающего: считать матрицу от комнатного
    // микрофона можно, просто обычно незачем, и каждая опора стоит одного
    // полного прохода детектора.
    std::vector<int> references;
};

struct CalibrationReport
{
    int detected = 0;  // сколько событий нашёл детектор по всем опорам
    int selected = 0;  // сколько прошло отбор
    int known = 0;     // сколько строк матрицы задержек удалось измерить
    int rejected = 0;  // сколько строк отброшено по разбросу или числу ударов
};

// Первый проход калибровки: собрать статистику записи.
//
// Каждый канал по очереди становится опорой, детектор ищет в нём удары, из них
// отбираются уверенные, одиночные и те, где опора громче остальных. По ним
// меряются задержки до всех прочих микрофонов — приходом по абсолютному порогу
// над полом канала, в широком окне: калибровка и выясняет, где стоят
// микрофоны, включая те, что дальше окна сверки.
//
// Субсэмплевого уточнения здесь нет намеренно. Корреляция сдвигает задержку на
// доли миллисекунды в сторону, своя у каждого инструмента: это групповая
// задержка тракта просачивания, а не геометрия. Профиль хранит геометрию,
// уточнение остаётся сверке — по каждому удару отдельно.
class SessionCalibration
{
public:
    explicit SessionCalibration(CalibrationSettings settings = {});

    SessionProfile run(IOnsetDetector& detector,
                       const float* const* channels,
                       int numChannels,
                       int numSamples,
                       const CalibrationContext& context,
                       CalibrationReport* report = nullptr);

    const CalibrationSettings& settings() const { return config; }

private:
    CalibrationSettings config;
};

} // namespace beat::doc
