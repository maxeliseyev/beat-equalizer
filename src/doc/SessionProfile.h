#pragma once

#include "doc/Ids.h"
#include "dsp/Constants.h"

#include <array>
#include <vector>

namespace beat::doc
{

// Что известно про задержку между парой каналов. Неизвестность — состояние, а
// не ноль: ноль означает «микрофоны стоят рядом», и подставлять его вместо
// «не померили» значит врать сверке ровно там, где она и так слаба.
struct DelayStat
{
    double medianSamples = 0.0;
    // Медиана абсолютных отклонений. Разброс здесь — не украшение отчёта, а
    // условие доверия: геометрия пары постоянна, и если числа разъезжаются,
    // мерили не то.
    double spreadSamples = 0.0;
    int observations = 0;
    bool known = false;
};

struct ChannelStat
{
    float noiseFloor = 0.0f;
    float rms = 0.0f;
    // Сколько отобранных ударов калибровка приписала этому каналу.
    int owned = 0;
};

// Статистика записи: уровни каналов, задержки между микрофонами, типичное
// просачивание. Собирается первым проходом, используется вторым — порогами
// сверки и априорными задержками (detector-design 2.2).
//
// Ключ задержки — пара каналов, а не пара «инструмент, канал». Типов ударов
// проект пока не знает, но в близко подзвученном ките канал-хозяин и есть
// инструмент: у бочки и снейра до комнатного микрофона разный путь, и матрица
// по каналам это различие уже несёт.
class SessionProfile
{
public:
    void setChannelCount(int count);
    int channelCount() const { return channels; }

    void setDelay(int from, int to, DelayStat stat);
    const DelayStat& delay(int from, int to) const;
    bool knows(int from, int to) const;

    void setChannel(int index, ChannelStat stat);
    const ChannelStat& channel(int index) const;

    // Насколько тише канал `to` слышит удар, которым владеет `from`, дБ.
    // Ноль — не измерено.
    void setBleedDb(int from, int to, float db);
    float bleedDb(int from, int to) const;

    // Априорные задержки для сверки. Неизвестные строки остаются нулём:
    // сверка тогда ищет в физически допустимом окне, как и раньше.
    void priors(int from, std::array<double, kMaxChannels>& out) const;

    // Есть ли хоть одна известная строка: пустой профиль применять незачем.
    bool empty() const;

    void clear();

private:
    int index(int from, int to) const;

    int channels = 0;
    std::vector<DelayStat> delays;
    std::vector<float> bleed;
    std::vector<ChannelStat> stats;
};

} // namespace beat::doc
