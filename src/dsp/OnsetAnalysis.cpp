#include "OnsetAnalysis.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace beat
{

namespace
{
int fftOrderFor(int windowSamples)
{
    int order = 1;
    while ((1 << order) < windowSamples)
        ++order;

    return std::clamp(order, 6, 16);
}

int msToSamples(float ms, double sampleRate)
{
    return std::max(1, static_cast<int>(std::lround(0.001 * static_cast<double>(ms) * sampleRate)));
}
} // namespace

OnsetAnalysis::OnsetAnalysis(const OnsetAnalysisConfig& config)
    : settings(config),
      fft(fftOrderFor(msToSamples(config.windowMs, config.sampleRate)))
{
    window = std::min(msToSamples(settings.windowMs, settings.sampleRate), fft.size());
    hop = std::max(1, msToSamples(settings.hopMs, settings.sampleRate));

    hann.resize(static_cast<size_t>(window));
    for (int i = 0; i < window; ++i)
    {
        const double phase = 2.0 * std::numbers::pi * static_cast<double>(i)
                             / static_cast<double>(std::max(1, window - 1));
        hann[static_cast<size_t>(i)] = static_cast<float>(0.5 - 0.5 * std::cos(phase));
    }

    const double binHz = settings.sampleRate / static_cast<double>(fft.size());
    firstBin = std::max(1, static_cast<int>(std::floor(settings.lowHz / binHz)));
    lastBin = std::min(fft.size() / 2 - 1, static_cast<int>(std::ceil(settings.highHz / binHz)));
    lastBin = std::max(lastBin, firstBin + 1);

    buildBandEdges();

    spectrum.resize(static_cast<size_t>(fft.size()));
    magnitude.resize(static_cast<size_t>(fft.size() / 2 + 1));
    previous.assign(magnitude.size(), 0.0f);
}

void OnsetAnalysis::buildBandEdges()
{
    const int numBands = std::max(1, settings.numBands);
    bandEdges.assign(static_cast<size_t>(numBands) + 1, firstBin);

    const double low = std::log(static_cast<double>(firstBin));
    const double high = std::log(static_cast<double>(lastBin));

    for (int b = 0; b <= numBands; ++b)
    {
        const double t = static_cast<double>(b) / static_cast<double>(numBands);
        const int edge = static_cast<int>(std::lround(std::exp(low + t * (high - low))));
        bandEdges[static_cast<size_t>(b)] = std::clamp(edge, firstBin, lastBin);
    }

    // Полосы не должны схлопываться в ноль на низком краю, где бинов мало.
    for (size_t b = 1; b < bandEdges.size(); ++b)
        bandEdges[b] = std::max(bandEdges[b], bandEdges[b - 1] + 1);
}

OnsetFeatures OnsetAnalysis::run(const float* samples, int numSamples)
{
    OnsetFeatures out;
    out.hopSamples = hop;
    out.windowSamples = window;
    out.firstSampleOffset = 0.5 * static_cast<double>(window);

    if (samples == nullptr || numSamples < window)
        return out;

    const int numBands = static_cast<int>(bandEdges.size()) - 1;
    out.numFrames = (numSamples - window) / hop + 1;
    out.flux.assign(static_cast<size_t>(out.numFrames), 0.0f);
    out.loudness.assign(static_cast<size_t>(out.numFrames), 0.0f);
    out.bands.assign(static_cast<size_t>(numBands),
                     std::vector<float>(static_cast<size_t>(out.numFrames), 0.0f));

    std::fill(previous.begin(), previous.end(), 0.0f);

    for (int frame = 0; frame < out.numFrames; ++frame)
    {
        const int start = frame * hop;
        std::fill(spectrum.begin(), spectrum.end(), std::complex<float> { 0.0f, 0.0f });
        for (int i = 0; i < window; ++i)
            spectrum[static_cast<size_t>(i)] = { samples[start + i] * hann[static_cast<size_t>(i)],
                                                 0.0f };

        fft.forward(spectrum.data());

        for (size_t bin = 0; bin < magnitude.size(); ++bin)
            magnitude[bin] = std::abs(spectrum[bin]);

        // Логарифм перед разностью: иначе поток меряет громкие удары, а не
        // приросты, и тихий гост-нот рядом с бочкой не виден вовсе.
        float flux = 0.0f;
        float loudness = 0.0f;
        for (int bin = firstBin; bin <= lastBin; ++bin)
        {
            const auto index = static_cast<size_t>(bin);
            const float now = std::log1p(1000.0f * magnitude[index]);
            const float before = std::log1p(1000.0f * previous[index]);
            flux += std::max(0.0f, now - before);
            loudness += magnitude[index];
        }

        out.flux[static_cast<size_t>(frame)] = flux;
        out.loudness[static_cast<size_t>(frame)] = loudness;

        for (int b = 0; b < numBands; ++b)
        {
            float sum = 0.0f;
            for (int bin = bandEdges[static_cast<size_t>(b)];
                 bin < bandEdges[static_cast<size_t>(b) + 1];
                 ++bin)
                sum += magnitude[static_cast<size_t>(bin)];

            out.bands[static_cast<size_t>(b)][static_cast<size_t>(frame)] = sum;
        }

        std::swap(previous, magnitude);
    }

    return out;
}

} // namespace beat
