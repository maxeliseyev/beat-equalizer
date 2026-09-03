#pragma once

#include "Constants.h"
#include "Fft.h"

#include <complex>
#include <vector>

namespace beat
{

// Обобщённая взаимная корреляция. Отбеливание — параметр, а не свойство
// класса: `kPhatWeighting` даёт классический PHAT, `kPlainWeighting` —
// обычную корреляцию, где переходный участок весит столько, сколько он
// громкий. На высокой когерентности лучше первое, на просачивании реального
// кита — второе (docs/real-kit-protocol.md).
class GccPhat
{
public:
    struct Result
    {
        float lagSamples = 0.0f;
        float phatPeak = 0.0f;
        // Высота пика к медиане окна поиска: уверенность кадра, не громкость.
        float peakRatio = 0.0f;
        float unweightedAtLag = 0.0f;
        bool invert = false;
        bool valid = false;
    };

    explicit GccPhat(int fftOrder = kDefaultFftOrder, float weighting = kPhatWeighting);

    // Сколько отсчётов кадра реально возьмётся при таком окне поиска: кадр
    // плюс лаг обязаны помещаться в БПФ, иначе свёртка заворачивается.
    int usableSamples(int maxLagSamples) const;

    Result estimate(const float* reference,
                    const float* signal,
                    int numSamples,
                    int maxLagSamples,
                    double sampleRate);

private:
    int lagIndex(int lag) const;

    Fft fft;
    float phatWeighting = kPhatWeighting;
    std::vector<std::complex<float>> spectrumRef;
    std::vector<std::complex<float>> spectrumSig;
    std::vector<float> searchWindow;
};

} // namespace beat
