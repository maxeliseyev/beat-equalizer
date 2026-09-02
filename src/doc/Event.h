#pragma once

#include "doc/Ids.h"
#include "dsp/Constants.h"

#include <array>
#include <string>

namespace beat::doc
{

// Кто поставил событие. Нужно не для красоты: правка пользователя не должна
// затираться переанализом, а будущий датасет без происхождения — мусор.
enum class Origin
{
    detector = 0,
    templateMatch,
    model,
    user
};

// Тип удара — подпись, а не ветка алгоритма. Ошибка здесь не портит звук.
enum class HitKind
{
    unknown = 0,
    kick,
    snare,
    tom,
    hat,
    ride,
    crash
};

// Что известно про один и тот же удар в одном микрофоне.
//
// Приход и перцептивная атака — два разных числа и хранятся порознь: фаза
// выравнивается по физическому приходу, сетка — по атаке (инвариант 17).
// Интервала владения отсчётами здесь нет и быть не может: удары
// перекрываются, и «конец полезного» — граница вклада, а не собственности.
struct ChannelObservation
{
    bool present = false;

    SamplePos arrivalSamples = 0.0;
    SamplePos perceptualAttackSamples = 0.0;
    SamplePos attackEndSamples = 0.0;
    SamplePos usefulEndSamples = 0.0;

    // Наклон прямой, подогнанной к логарифму огибающей на затухании.
    float decayDbPerSecond = 0.0f;

    float confidence = 0.0f;
    Origin origin = Origin::detector;
};

// Удар как решение: где, что, насколько уверенно и по каким признакам.
struct Event
{
    EventId id = kInvalidId;

    // Канал, на котором событие найдено. Остальные каналы не ищут его заново,
    // а предсказывают по d[i] и уточняют субсэмплево.
    int referenceChannel = 0;

    // Положение события = приход на опорном канале, сэмплы от нуля сессии.
    SamplePos timeSamples = 0.0;

    HitKind kind = HitKind::unknown;
    float confidence = 0.0f;
    Origin origin = Origin::detector;

    std::array<ChannelObservation, kMaxChannels> channels {};

    // Признак, по которому принято решение: нормированная энергия по каналам.
    // Без него потом невозможно понять, почему детектор ошибся.
    std::array<float, kMaxChannels> energy {};
};

// Сколько каналов видели удар. Событие, которое видно ровно в одном канале и
// при этом не в близком, — кандидат на просачивание, а не на удар.
int observationCount(const Event& event);

} // namespace beat::doc
