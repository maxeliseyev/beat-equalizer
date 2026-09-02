#pragma once

#include "doc/Ids.h"
#include "dsp/Constants.h"

#include <cstdint>
#include <string>

namespace beat::doc
{

// Стем на диске. Документ не держит звук: он держит то, что нужно, чтобы звук
// найти и правильно поставить во времени.
struct Source
{
    SourceId id = kInvalidId;
    std::string path;
    std::string name;
    double sampleRate = 0.0;
    int numChannels = 0;
    std::int64_t numSamples = 0;

    // Смещение начала файла от нуля сессии. Экспорт «от одного нуля» даёт
    // здесь ноль у всех; поле есть, потому что проверить это можно только
    // числом, а обнаружить нарушение потом — только по разъехавшемуся киту.
    std::int64_t sessionOffsetSamples = 0;
};

// Канал документа: то, что видно строкой в таблице и что нумеруют инварианты
// выравнивания. Один канал — ровно один канал одного источника.
struct Channel
{
    int index = kInvalidId;
    SourceId source = kInvalidId;
    int sourceChannel = 0;
    std::string name;

    // Роль — метаданные, не ветка алгоритма (AGENTS, инвариант 8).
    ChannelRole role = ChannelRole::unknown;
};

} // namespace beat::doc
