#pragma once

#include "Constants.h"

#include <vector>

namespace beat
{

struct PeakPickConfig
{
    double sampleRate = 48000.0;
    int hopSamples = 1;
    float medianWindowMs = kOnsetMedianWindowMs;
    float thresholdFactor = kOnsetThresholdFactor;
    float thresholdBias = kOnsetThresholdBias;
    float minIntervalMs = kOnsetMinIntervalMs;
};

struct FluxPeak
{
    int frame = 0;
    float value = 0.0f;
    float threshold = 0.0f;

    // Во сколько раз пик выше порога. Число, а не флаг: пороги отсечения
    // настраиваются потом, без нового анализа.
    float strength() const { return threshold > 0.0f ? value / threshold : 0.0f; }
};

// Адаптивный порог и локальный максимум. Детектируем щедро, отсеиваем потом:
// отсев дешёвый и объяснимый, в отличие от одного жёсткого порога.
std::vector<FluxPeak> pickPeaks(const std::vector<float>& flux, const PeakPickConfig& config);

} // namespace beat
