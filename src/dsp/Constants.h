#pragma once

#include <cmath>

namespace beat
{

inline constexpr int kMinChannels = 2;
inline constexpr int kMaxChannels = 24;
inline constexpr float kMinDistanceM = 0.5f;
inline constexpr float kMaxDistanceM = 10.0f;
inline constexpr float kDefaultMaxDistanceM = 4.0f;
inline constexpr float kSpeedOfSoundMps = 343.0f;
// Линия задержки обязана покрывать самую дальнюю дистанцию поиска, иначе
// автовыравнивание упрётся в потолок параметра на комнатном микрофоне.
inline constexpr float kMaxDelayMs = 1000.0f * kMaxDistanceM / kSpeedOfSoundMps + 1.0f;
inline constexpr float kAnalysisLowHz = 100.0f;
inline constexpr float kAnalysisHighHz = 8000.0f;
// Когерентность суммы считается уже, чем ищется задержка: ниже 200 Hz пара
// микрофонов складывается почти всегда и метрика перестаёт что-либо различать.
inline constexpr float kCoherenceLowHz = 200.0f;
inline constexpr float kCoherenceHighHz = 8000.0f;
inline constexpr int kDefaultFftOrder = 13;
inline constexpr float kPhatEps = 1.0e-12f;
inline constexpr int kLagrangeOrder = 5;
inline constexpr int kInterpolatorLatencySamples = 2;
inline constexpr float kDelaySmoothMs = 5.0f;
// Запас по краям защищённой зоны варпа: край зоны — не обрыв, а склейка,
// и кроссфейду нужно место вне атаки (инвариант 17).
inline constexpr float kProtectedMarginMs = 3.0f;

// Детекция онсетов. Окно и шаг заданы в миллисекундах, а не в сэмплах: на
// 96 кГц кадр обязан остаться тем же по времени, иначе поток спектра меняет
// масштаб вместе с частотой дискретизации (инвариант 4).
inline constexpr float kOnsetWindowMs = 20.0f;
inline constexpr float kOnsetHopMs = 2.5f;
inline constexpr int kOnsetBands = 8;
// Порог: медиана окрестности умножается, к ней добавляется доля от среднего по
// всему потоку. Медиана одна ловит шум в тихих местах, среднее одно глохнет на
// громких — нужны оба.
inline constexpr float kOnsetMedianWindowMs = 100.0f;
inline constexpr float kOnsetThresholdFactor = 1.7f;
inline constexpr float kOnsetThresholdBias = 0.10f;
// Флэм — это два удара, а не один: разводить их надо, а не склеивать.
inline constexpr float kOnsetMinIntervalMs = 12.0f;
// Пик впритык к порогу — не событие. Ложное срабатывание хуже пропуска
// (инвариант 13), поэтому запас несимметричный: 0.2 по уверенности это
// требование быть на четверть выше адаптивного порога.
inline constexpr float kOnsetMinConfidence = 0.2f;
// Огибающая ступени 1: быстрый подъём, чтобы не съесть атаку, медленный спад,
// чтобы не считать каждый период низкой частоты отдельным ударом.
inline constexpr float kEnvelopeAttackMs = 2.0f;
inline constexpr float kEnvelopeReleaseMs = 20.0f;
// Конец полезного: вклад ушёл под пол дорожки плюс запас.
inline constexpr float kUsefulEndMarginDb = 6.0f;
// Окно, в котором меряется энергия удара по каналам для энергетического
// вектора: короче — ловит только атаку, длиннее — тянет соседний удар.
inline constexpr float kOnsetEnergyWindowMs = 30.0f;
inline constexpr float kMinScopeTimeMs = 10.0f;
inline constexpr float kMaxScopeTimeMs = 2000.0f;
inline constexpr float kDefaultScopeTimeMs = 40.0f;

// Кольцо живого входа держит секунду: длинное окно нужно стенду, где материал
// лежит в клипе целиком, а не осциллографу в хосте. Иначе на 24 каналах и
// 96 кГц кольцо выросло бы вдвое ради картинки, которую там никто не смотрит.
inline constexpr float kMaxRingTimeMs = 1000.0f;

// Отрисовка берёт не больше этого числа точек на канал: полоса осциллограммы
// втрое уже, а память и работа в message thread растут с окном линейно.
// Точка — абсолютный максимум своей группы, поэтому транзиент не теряется.
inline constexpr int kMaxDisplayPoints = 4096;

// Монитор-микс стенда: уровень и панорама на канал. Мониторинг, в экспорт и в
// N-out passthrough не попадают.
inline constexpr float kMinMonitorLevelDb = -60.0f;
inline constexpr float kMaxMonitorLevelDb = 12.0f;

// Темп для сетки. Верх и низ — просто здравые границы ввода, алгоритм от них
// не зависит.
inline constexpr float kMinTempoBpm = 40.0f;
inline constexpr float kMaxTempoBpm = 300.0f;
inline constexpr float kDefaultTempoBpm = 120.0f;

// Analysis window: кадр FFT, hop 50 %, кольцевой буфер сырого входа.
inline constexpr float kAnalysisSeconds = 8.0f;
inline constexpr float kAnalysisMinRms = 0.0005f;
// Пик GCC к медиане окна поиска: ниже — кадр без внятного пика, не считаем.
// На синтетике пара «шум и его копия» даёт ~380, две независимые дорожки ~5.
// Порог калибруется на реальных китах (plan.md, PR 8).
inline constexpr float kAnalysisMinPeakRatio = 8.0f;
inline constexpr int kAnalysisMinFrames = 3;

// Грубый перебор ротатора на Analyze: сетка частот и глубин.
// Меньший выигрыш, чем kRotatorMinGain, не стоит вращения фазы — оставляем bypass.
inline constexpr float kRotatorSearchLowHz = 60.0f;
inline constexpr float kRotatorSearchHighHz = 8000.0f;
inline constexpr int kRotatorSearchSteps = 12;
inline constexpr float kRotatorMinGain = 0.005f;
inline constexpr float kDefaultRotatorHz = 600.0f;

enum class ChannelRole
{
    unknown = 0,
    close,
    overhead,
    room,
    hats
};

enum class PolarityMode
{
    automatic = 0,
    positive,
    invert
};

inline float maxLagSeconds(float distanceM, float speedOfSound = kSpeedOfSoundMps)
{
    if (distanceM <= 0.0f || speedOfSound <= 0.0f)
        return 0.0f;

    return distanceM / speedOfSound;
}

inline int maxLagSamples(float distanceM, double sampleRate, float speedOfSound = kSpeedOfSoundMps)
{
    if (sampleRate <= 0.0)
        return 0;

    return static_cast<int>(std::ceil(static_cast<double>(maxLagSeconds(distanceM, speedOfSound)) * sampleRate));
}

} // namespace beat
