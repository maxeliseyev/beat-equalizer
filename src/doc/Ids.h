#pragma once

#include <cstdint>

namespace beat::doc
{

using SourceId = int;
using EventId = int;

inline constexpr int kInvalidId = -1;

// Время в документе — сэмплы от нуля сессии, дробные там, где число получено
// субсэмплево (GCC-PHAT). Секунды не берём: сэмпл-рейт источников один на
// проект, а округление до миллисекунд стоит дороже, чем весь выигрыш.
using SamplePos = double;

} // namespace beat::doc
