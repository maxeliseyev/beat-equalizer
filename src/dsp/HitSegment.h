#pragma once

#include "Constants.h"

#include <vector>

namespace beat
{

// Границы одного удара в одной огибающей. Не «отсчёты принадлежат удару»:
// удары перекрываются, и конец полезного — граница вклада, а не владения.
struct HitSegment
{
    int arrival = 0;
    int attackEnd = 0;
    int usefulEnd = 0;
    float peak = 0.0f;
};

// Пик в окне — конец атаки; приход — последняя точка перед подъёмом; конец
// полезного — там, где вклад ушёл под пол дорожки плюс запас.
//
// Мерится одинаково во всех каналах: защищённая зона — объединение атак по
// микрофонам, и складывать в неё числа, посчитанные по-разному, нельзя.
HitSegment segmentHit(const std::vector<float>& envelope,
                      int from,
                      int to,
                      int limit,
                      float floorLevel,
                      float marginDb = kUsefulEndMarginDb);

// Наклон прямой по логарифму огибающей от пика вниз, дБ за секунду.
float decaySlope(const std::vector<float>& envelope, const HitSegment& segment, double sampleRate);

} // namespace beat
