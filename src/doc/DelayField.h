#pragma once

#include "doc/Ids.h"
#include "dsp/Constants.h"

#include <array>
#include <map>

namespace beat::doc
{

// Поле задержек d(событие, канал): что измерено и что из этого применяется.
//
// Сырой TDOA хранится и не выбрасывается (инвариант 16). Задержка оверхедов и
// комнаты — часть звука, и выравнивание обязано быть обратимым по каналу:
//
//     applied[i] = max(d) - (1 - r[i]) * d[i]
//
// r = 0 — канал выровнен в ноль, как на этапе 1; r = 1 — задержка возвращена
// целиком, канал стоит там, где его услышал микрофон. Между ними — доля.
class DelayField
{
public:
    void setRaw(EventId event, int channel, double tdoaSamples);
    bool has(EventId event, int channel) const;
    double raw(EventId event, int channel) const;

    void setReturn(int channel, float amount);
    float returnFactor(int channel) const;

    // Максимум сырых задержек события: тот канал, под который ждут остальные.
    double maxRaw(EventId event) const;

    // Всегда >= 0: в realtime канал можно только задержать (инвариант 2).
    double applied(EventId event, int channel) const;

    void eraseEvent(EventId event);
    void clear();
    int eventCount() const;

private:
    struct Row
    {
        std::array<double, kMaxChannels> tdoa {};
        std::array<bool, kMaxChannels> valid {};
    };

    const Row* find(EventId event) const;

    // Упорядоченная карта, а не хеш: один вход обязан давать один выход, и
    // порядок обхода не должен зависеть от истории вставок.
    std::map<EventId, Row> rows;
    std::array<float, kMaxChannels> returns {};
};

} // namespace beat::doc
