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
// Показатель отбеливания кросс-спектра: 1 — это PHAT, 0 — обычная взаимная
// корреляция. PHAT резок там, где когерентность высокая; на просачивании
// реального кита она низкая, и отбеливание топит переходный участок в
// некогерентном хвосте (docs/real-kit-protocol.md).
inline constexpr float kPhatWeighting = 1.0f;
inline constexpr float kPlainWeighting = 0.0f;
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

// Сверка по микрофонам. Кусок под GCC-PHAT берётся длиннее удара: короткий
// кусок ловит шум, длинный тянет соседний удар.
// Кадр сверки короткий не из экономии: прямой путь между микрофонами
// когерентен первые единицы миллисекунд, дальше два микрофона слышат разные
// поля. На 40 мс и длиннее оценка начинает уезжать на период инструмента.
inline constexpr float kMatchFrameMs = 20.0f;
// Окно уточнения вокруг предсказанного прихода. Широкий поиск на реальном
// просачивании даёт устойчивый и неверный ответ при любом отбеливании —
// поэтому корреляция здесь только уточняет, а не ищет (инвариант 5).
inline constexpr float kMatchRefineMs = 2.0f;
inline constexpr float kMatchPreRollMs = 5.0f;
// Корреляция логарифмов огибающих: прямой звук и его просачивание похожи по
// форме затухания, случайное совпадение — нет.
inline constexpr float kMatchEnvelopeWindowMs = 60.0f;
inline constexpr float kMatchMinCorrelation = 0.5f;
// Насколько удар обязан быть выше собственного пола канала, чтобы считаться
// услышанным, а не додуманным.
inline constexpr float kMatchMinAudibleDb = 6.0f;
// Владелец удара: канал, где он и раньше, и энергичнее относительно своего же
// среднего. Множитель — насколько энергичнее.
inline constexpr float kMatchOwnerMargin = 2.0f;

// Калибровка сессии — ступень 2 лестницы детекции. Внутри проекта установка,
// микрофоны, комната и барабанщик постоянны, поэтому ошибки систематические:
// первый проход собирает статистику записи, второй пользуется ею как
// настройкой порогов (detector-design 2.2).
//
// В калибровку берутся не все удары, а отобранные: уверенные, одиночные и
// те, где опорный канал громче остальных. На плотной игре огибающая не
// успевает вернуться к полу, и приход измерить нечем — брать оттуда числа
// значит калибровать по шуму.
inline constexpr float kCalibrationMinConfidence = 0.8f;
inline constexpr float kCalibrationIsolationMs = 120.0f;
// Окно калибровки шире окна сверки: она и выясняет, где стоят микрофоны,
// включая те, что дальше четырёх метров.
inline constexpr float kCalibrationDistanceM = kMaxDistanceM;
// Разброс, выше которого строка считается неизвестной. 0.25 мс — это 8.5 см:
// геометрия, которую не удалось померить точнее, не геометрия, а догадка.
inline constexpr float kCalibrationMaxSpreadMs = 0.25f;
inline constexpr int kCalibrationMinHits = 8;
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

// Сколько материала стенда берёт Detect: окно вокруг позиции воспроизведения,
// а не весь клип. Детектор считает огибающие и полосы по всем каналам сразу, и
// на восьми дорожках по четыре минуты на 96 кГц это сотни мегабайт временных
// массивов — стенд для того и стенд, чтобы смотреть кусок.
inline constexpr float kDetectSeconds = 20.0f;
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

// Наименьший порядок БПФ, в который влезает столько отсчётов.
//
// Порядок нельзя держать константой в сэмплах, когда кадр задан в
// миллисекундах: на 96 кГц тот же кадр вдвое длиннее, и вместе с окном поиска
// перестаёт помещаться — свёртка заворачивается, оценка приходит из другого
// конца кадра. На 48 кГц это не проявлялось, поэтому синтетика молчала
// (инвариант 4, docs/real-kit-protocol.md).
inline int fftOrderFor(int samples)
{
    int order = 1;
    while ((1 << order) < samples)
        ++order;

    return order;
}

} // namespace beat
