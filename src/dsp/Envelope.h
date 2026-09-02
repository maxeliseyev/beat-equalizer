#pragma once

#include "Constants.h"

#include <vector>

namespace beat
{

// Огибающая ступени 1 лестницы сегментации: выпрямление и сглаживание с разной
// постоянной на подъём и на спад. Сэмплового разрешения, в отличие от кадровой
// STFT: конец атаки — это пик, и мерить его по сетке кадров нечестно.
std::vector<float> followEnvelope(const float* samples,
                                  int numSamples,
                                  double sampleRate,
                                  float attackMs = kEnvelopeAttackMs,
                                  float releaseMs = kEnvelopeReleaseMs);

// Пол дорожки: низкий процентиль по окну. Не минимум — минимум ловит паузу
// между периодами, а не шум преампа.
float noiseFloorOf(const std::vector<float>& envelope, float percentile = 0.1f);

} // namespace beat
