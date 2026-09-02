#pragma once

#include "doc/Event.h"
#include "doc/Ids.h"

#include <cstdint>
#include <string>
#include <vector>

namespace beat::doc
{

enum class EditAction
{
    accepted = 0,   // предложение принято как есть
    addedEvent,     // удар был, детектор промолчал
    removedEvent,   // маркер снят
    movedEvent,     // время поправлено руками
    retyped         // изменён тип удара
};

// Почему пользователь снял или подвинул маркер. Без этого различия любой
// будущий датасет — мусор, и понять это можно будет только после обучения
// (AGENTS, инвариант 13).
enum class EditVerdict
{
    none = 0,
    detectorWasWrong,   // события не было / оно не там — единственный сигнал об ошибке
    leaveAlone,         // событие есть, но трогать его не надо
    artistic            // событие есть и стоит не по сетке намеренно
};

struct EditEntry
{
    std::int64_t sequence = 0;
    EventId event = kInvalidId;

    EditAction action = EditAction::accepted;
    EditVerdict verdict = EditVerdict::none;

    // -1 — правка события целиком, иначе номер канала.
    int channel = -1;

    SamplePos proposedTimeSamples = 0.0;
    SamplePos userTimeSamples = 0.0;
    float proposedConfidence = 0.0f;

    HitKind proposedKind = HitKind::unknown;
    HitKind userKind = HitKind::unknown;

    // Кто предложил: имя и версия детектора на момент предложения.
    std::string detector;
};

// Журнал правок ведётся с первого дня, даже пока никуда не отправляется:
// собрать его задним числом невозможно (detector-design 1.7).
class EditLog
{
public:
    std::int64_t append(EditEntry entry);

    const std::vector<EditEntry>& entries() const { return items; }
    int size() const { return static_cast<int>(items.size()); }
    void clear();

    // Только это годится в обучающую выборку. Всё остальное — художественные
    // решения, и обучение на них портит модель, а не улучшает.
    std::vector<EditEntry> mistakes() const;

private:
    std::vector<EditEntry> items;
    std::int64_t nextSequence = 1;
};

} // namespace beat::doc
