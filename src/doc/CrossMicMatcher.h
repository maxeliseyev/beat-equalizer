#pragma once

#include "doc/Document.h"
#include "dsp/Constants.h"

#include <array>

namespace beat::doc
{

struct MatchSettings
{
    float maxDistanceM = kDefaultMaxDistanceM;
    float frameMs = kMatchFrameMs;
    float preRollMs = kMatchPreRollMs;
    float envelopeWindowMs = kMatchEnvelopeWindowMs;
    float minCorrelation = kMatchMinCorrelation;
    float minAudibleDb = kMatchMinAudibleDb;
    float ownerMargin = kMatchOwnerMargin;

    float attackMs = kEnvelopeAttackMs;
    float releaseMs = kEnvelopeReleaseMs;
    float usefulEndMarginDb = kUsefulEndMarginDb;
};

struct MatchContext
{
    double sampleRate = 48000.0;
    SamplePos startSample = 0.0;

    // Априорные задержки каналов относительно опоры, сэмплы. Ноль — «не знаем»:
    // тогда всё окно поиска приходится на разброс расстояний.
    std::array<double, kMaxChannels> prior {};
};

struct MatchReport
{
    int observations = 0;    // сколько наблюдений добавлено
    int rejected = 0;        // сколько кандидатов не прошло сверку
    int reattributed = 0;    // сколько событий сменили владельца
    // Событий, которых нет нигде, кроме канала, где их нашли. Первый кандидат
    // на просачивание: настоящий удар слышен хотя бы ещё в одном микрофоне.
    int singleChannel = 0;
};

// Сверка по всем микрофонам: ступень 3 лестницы.
//
// Событие, найденное на опорном канале, в остальных не ищется заново — оно
// предсказывается как `t + d[j]` и уточняется субсэмплево GCC-PHAT. Дальше
// три дешёвых признака решают, тот ли это удар: корреляция логарифмов
// огибающих, слышимость над собственным полом канала и физическая
// допустимость лага.
//
// Побочный, но главный результат — владелец удара. Просачивание отличается от
// прямого звука тем, что приходит позже и тише относительно собственного
// среднего канала. Канал, где удар и раньше, и энергичнее, и есть его хозяин.
class CrossMicMatcher
{
public:
    explicit CrossMicMatcher(MatchSettings settings = {});

    MatchReport match(Document& document,
                      const float* const* channels,
                      int numChannels,
                      int numSamples,
                      const MatchContext& context);

    const MatchSettings& settings() const { return config; }

private:
    MatchSettings config;
};

} // namespace beat::doc
