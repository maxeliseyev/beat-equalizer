#pragma once

#include "doc/OnsetDetector.h"
#include "dsp/Constants.h"

namespace beat::doc
{

struct SpectralFluxSettings
{
    float windowMs = kOnsetWindowMs;
    float hopMs = kOnsetHopMs;
    int numBands = kOnsetBands;

    float thresholdFactor = kOnsetThresholdFactor;
    float thresholdBias = kOnsetThresholdBias;
    float minIntervalMs = kOnsetMinIntervalMs;
    float minConfidence = kOnsetMinConfidence;

    float attackMs = kEnvelopeAttackMs;
    float releaseMs = kEnvelopeReleaseMs;
    float usefulEndMarginDb = kUsefulEndMarginDb;
    float energyWindowMs = kOnsetEnergyWindowMs;
};

// Ступени 1–2 лестницы: широкополосная огибающая и полосовой поток спектра.
// Ни данных, ни обучения, ни лицензий — и на изолированных ударах этого хватает.
//
// Что детектор делает: находит удары на опорном канале, меряет по огибающей
// границы удара в этом канале и складывает тяжёлые признаки в кэш.
// Чего не делает: не уточняет время субсэмплево, не решает, тот ли это удар в
// соседнем микрофоне, и не называет тип удара.
class SpectralFluxDetector final : public IOnsetDetector
{
public:
    explicit SpectralFluxDetector(SpectralFluxSettings settings = {});

    std::string name() const override { return "spectral-flux"; }
    std::string version() const override { return "1"; }
    std::string parameters() const override;

    std::vector<Event> analyze(const float* const* channels,
                               int numChannels,
                               int numSamples,
                               const AnalysisContext& context) override;

    const SpectralFluxSettings& settings() const { return config; }

private:
    SpectralFluxSettings config;
};

} // namespace beat::doc
