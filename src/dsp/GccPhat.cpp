#include "GccPhat.h"

#include "Constants.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace beat
{

namespace
{
float hann(int index, int length)
{
    if (length <= 1)
        return 1.0f;

    constexpr float pi = std::numbers::pi_v<float>;
    return 0.5f * (1.0f - std::cos(2.0f * pi * static_cast<float>(index) / static_cast<float>(length - 1)));
}
} // namespace

GccPhat::GccPhat(int fftOrder)
    : fft(fftOrder),
      spectrumRef(static_cast<size_t>(fft.size())),
      spectrumSig(static_cast<size_t>(fft.size()))
{
    searchWindow.reserve(static_cast<size_t>(fft.size()));
}

int GccPhat::lagIndex(int lag) const
{
    const int n = fft.size();
    if (lag < 0)
        return n + lag;
    return lag;
}

GccPhat::Result GccPhat::estimate(const float* reference,
                                  const float* signal,
                                  int numSamples,
                                  int maxLagSamples,
                                  double sampleRate)
{
    Result result;

    if (reference == nullptr || signal == nullptr || numSamples < 16 || maxLagSamples <= 0
        || sampleRate <= 0.0)
        return result;

    const int n = fft.size();
    const int use = std::min(numSamples, n);
    const int maxLag = std::min(maxLagSamples, n / 2 - 2);
    if (maxLag <= 0)
        return result;

    std::fill(spectrumRef.begin(), spectrumRef.end(), std::complex<float>{});
    std::fill(spectrumSig.begin(), spectrumSig.end(), std::complex<float>{});

    for (int i = 0; i < use; ++i)
    {
        const float w = hann(i, use);
        spectrumRef[static_cast<size_t>(i)] = { reference[i] * w, 0.0f };
        spectrumSig[static_cast<size_t>(i)] = { signal[i] * w, 0.0f };
    }

    fft.forward(spectrumRef.data());
    fft.forward(spectrumSig.data());

    const float binHz = static_cast<float>(sampleRate / static_cast<double>(n));

    for (int k = 0; k < n; ++k)
    {
        const float signedHz = (k <= n / 2) ? static_cast<float>(k) * binHz
                                            : static_cast<float>(k - n) * binHz;
        const float hz = std::abs(signedHz);

        if (hz < kAnalysisLowHz || hz > kAnalysisHighHz)
        {
            spectrumRef[static_cast<size_t>(k)] = {};
            continue;
        }

        // Y * conj(X): positive lag means `signal` is later than `reference`.
        const auto cross = spectrumSig[static_cast<size_t>(k)]
                           * std::conj(spectrumRef[static_cast<size_t>(k)]);
        const float mag = std::abs(cross);
        spectrumRef[static_cast<size_t>(k)] = cross / (mag + kPhatEps);
    }

    fft.inverse(spectrumRef.data());

    int bestLag = 0;
    float bestAbs = -1.0f;
    searchWindow.clear();

    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        const float value = spectrumRef[static_cast<size_t>(lagIndex(lag))].real();
        const float absValue = std::abs(value);
        searchWindow.push_back(absValue);
        if (absValue > bestAbs)
        {
            bestAbs = absValue;
            bestLag = lag;
        }
    }

    const auto middle = searchWindow.begin() + static_cast<long>(searchWindow.size() / 2);
    std::nth_element(searchWindow.begin(), middle, searchWindow.end());
    const float median = *middle;

    const float ym1 = std::abs(spectrumRef[static_cast<size_t>(lagIndex(bestLag - 1))].real());
    const float y0 = std::abs(spectrumRef[static_cast<size_t>(lagIndex(bestLag))].real());
    const float yp1 = std::abs(spectrumRef[static_cast<size_t>(lagIndex(bestLag + 1))].real());
    const float denom = ym1 - 2.0f * y0 + yp1;
    float delta = 0.0f;
    if (std::abs(denom) > 1.0e-12f)
        delta = 0.5f * (ym1 - yp1) / denom;
    delta = std::clamp(delta, -1.0f, 1.0f);

    result.lagSamples = static_cast<float>(bestLag) + delta;
    result.phatPeak = y0;
    result.peakRatio = bestAbs / (median + kPhatEps);
    result.valid = std::isfinite(result.lagSamples) && y0 > 0.0f;

    const int integerLag = static_cast<int>(std::lround(result.lagSamples));
    double sum = 0.0;
    for (int i = 0; i < use; ++i)
    {
        const int j = i + integerLag;
        if (j >= 0 && j < use)
            sum += static_cast<double>(reference[i]) * static_cast<double>(signal[j]);
    }

    result.unweightedAtLag = static_cast<float>(sum);
    result.invert = sum < 0.0;
    return result;
}

} // namespace beat
