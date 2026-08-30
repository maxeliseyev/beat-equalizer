#pragma once

#include "Constants.h"
#include "Fft.h"

#include <complex>
#include <vector>

namespace beat
{

class GccPhat
{
public:
    struct Result
    {
        float lagSamples = 0.0f;
        float phatPeak = 0.0f;
        float unweightedAtLag = 0.0f;
        bool invert = false;
        bool valid = false;
    };

    explicit GccPhat(int fftOrder = kDefaultFftOrder);

    Result estimate(const float* reference,
                    const float* signal,
                    int numSamples,
                    int maxLagSamples,
                    double sampleRate);

private:
    int lagIndex(int lag) const;

    Fft fft;
    std::vector<std::complex<float>> spectrumRef;
    std::vector<std::complex<float>> spectrumSig;
};

} // namespace beat
