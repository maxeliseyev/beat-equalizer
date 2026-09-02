#pragma once

#include "Constants.h"
#include "Fft.h"

#include <complex>
#include <vector>

namespace beat
{

struct OnsetAnalysisConfig
{
    double sampleRate = 48000.0;
    float windowMs = kOnsetWindowMs;
    float hopMs = kOnsetHopMs;
    int numBands = kOnsetBands;
    float lowHz = kAnalysisLowHz;
    float highHz = kAnalysisHighHz;
};

// Кадровые признаки одного канала. Тяжёлая часть анализа: считается один раз и
// живёт в кэше признаков, отдельно от решений детектора.
struct OnsetFeatures
{
    int hopSamples = 0;
    int windowSamples = 0;
    int numFrames = 0;
    // Центр первого кадра в сэмплах от начала переданного блока.
    double firstSampleOffset = 0.0;

    // Полуволновой поток спектра: сумма прироста логарифма магнитуд.
    std::vector<float> flux;
    // Сумма магнитуд в полосе анализа — громкость кадра.
    std::vector<float> loudness;
    // Полосовые огибающие, numBands рядов по numFrames точек.
    std::vector<std::vector<float>> bands;

    double timeOfFrame(int frame) const
    {
        return firstSampleOffset + static_cast<double>(frame) * static_cast<double>(hopSamples);
    }
};

// Поток спектра и полосовые огибающие. Полосы логарифмические: щелчок
// колотушки и тело бочки живут в разных, и разводить их линейной сеткой
// бессмысленно.
class OnsetAnalysis
{
public:
    explicit OnsetAnalysis(const OnsetAnalysisConfig& config);

    OnsetFeatures run(const float* samples, int numSamples);

    int hopSamples() const { return hop; }
    int windowSamples() const { return window; }
    int fftSize() const { return fft.size(); }

private:
    void buildBandEdges();

    OnsetAnalysisConfig settings;
    Fft fft;
    int window = 0;
    int hop = 0;
    int firstBin = 0;
    int lastBin = 0;
    std::vector<float> hann;
    std::vector<int> bandEdges;
    std::vector<std::complex<float>> spectrum;
    std::vector<float> magnitude;
    std::vector<float> previous;
};

} // namespace beat
