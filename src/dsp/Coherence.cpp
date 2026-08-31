#include "Coherence.h"

#include "AllpassRotator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace beat
{

namespace
{
constexpr float kPi = std::numbers::pi_v<float>;

float hann(int index, int length)
{
    if (length <= 1)
        return 1.0f;

    return 0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(index)
                                   / static_cast<float>(length - 1)));
}
} // namespace

Coherence::Coherence(int fftOrder)
    : fft(fftOrder),
      spectrumRef(static_cast<size_t>(fft.size())),
      spectrumCh(static_cast<size_t>(fft.size()))
{
}

void Coherence::setPair(const float* reference,
                        const float* channel,
                        int numSamples,
                        double newSampleRate)
{
    ready = false;

    if (reference == nullptr || channel == nullptr || numSamples < 16 || newSampleRate <= 0.0)
        return;

    sampleRate = newSampleRate;

    const int n = fft.size();
    const int use = std::min(numSamples, n);

    std::fill(spectrumRef.begin(), spectrumRef.end(), std::complex<float>{});
    std::fill(spectrumCh.begin(), spectrumCh.end(), std::complex<float>{});

    for (int i = 0; i < use; ++i)
    {
        const float w = hann(i, use);
        spectrumRef[static_cast<size_t>(i)] = { reference[i] * w, 0.0f };
        spectrumCh[static_cast<size_t>(i)] = { channel[i] * w, 0.0f };
    }

    fft.forward(spectrumRef.data());
    fft.forward(spectrumCh.data());

    const double binHz = sampleRate / static_cast<double>(n);
    lowBin = std::max(1, static_cast<int>(std::ceil(kCoherenceLowHz / binHz)));
    highBin = std::min(n / 2 - 1, static_cast<int>(std::floor(kCoherenceHighHz / binHz)));
    ready = highBin > lowBin;
}

float Coherence::measure(const Transform& transform) const
{
    if (!ready)
        return 0.0f;

    const int n = fft.size();
    const float binHz = static_cast<float>(sampleRate / static_cast<double>(n));
    const float sign = transform.invert ? -1.0f : 1.0f;
    const bool rotate = transform.rotatorAmount > 0.0f;

    double sumTogether = 0.0;
    double sumApart = 0.0;

    for (int k = lowBin; k <= highBin; ++k)
    {
        const auto a = spectrumRef[static_cast<size_t>(k)];
        auto b = spectrumCh[static_cast<size_t>(k)];

        // Задержка на d сэмплов — множитель exp(-j 2 pi k d / N).
        const float phase = -2.0f * kPi * static_cast<float>(k) * transform.delaySamples
                            / static_cast<float>(n);
        b *= std::complex<float>(std::cos(phase), std::sin(phase)) * sign;

        if (rotate)
            b *= AllpassRotator::response(transform.rotatorHz,
                                          transform.rotatorAmount,
                                          static_cast<float>(k) * binHz,
                                          sampleRate);

        sumTogether += static_cast<double>(std::abs(a + b));
        sumApart += static_cast<double>(std::abs(a)) + static_cast<double>(std::abs(b));
    }

    if (sumApart <= 0.0)
        return 0.0f;

    return static_cast<float>(sumTogether / sumApart);
}

} // namespace beat
