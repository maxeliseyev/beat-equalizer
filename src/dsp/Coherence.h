#pragma once

#include "Constants.h"
#include "Fft.h"

#include <complex>
#include <vector>

namespace beat
{

// Когерентность суммы пары: mean|A + B| / mean(|A| + |B|) в 200 Hz…8 kHz.
// 1.0 — сложились в фазе, 0.5 — сумма ничего не выиграла, 0 — вычлись.
// Пара готовится один раз, а варианты выравнивания примеряются в спектре:
// задержка — фазовым наклоном, полярность — знаком, ротатор — своей реакцией.
class Coherence
{
public:
    struct Transform
    {
        float delaySamples = 0.0f; // > 0 задерживает канал относительно опоры
        bool invert = false;
        float rotatorHz = kDefaultRotatorHz;
        float rotatorAmount = 0.0f;
    };

    explicit Coherence(int fftOrder = kDefaultFftOrder);

    int frameSize() const { return fft.size(); }

    void setPair(const float* reference, const float* channel, int numSamples, double sampleRate);
    float measure(const Transform& transform) const;
    float measureRaw() const { return measure({}); }

private:
    Fft fft;
    std::vector<std::complex<float>> spectrumRef;
    std::vector<std::complex<float>> spectrumCh;
    double sampleRate = 48000.0;
    int lowBin = 0;
    int highBin = 0;
    bool ready = false;
};

} // namespace beat
