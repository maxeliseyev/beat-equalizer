#include "PeakPicker.h"

#include <algorithm>
#include <cmath>

namespace beat
{

namespace
{
float medianOf(std::vector<float>& scratch)
{
    if (scratch.empty())
        return 0.0f;

    const auto middle = scratch.begin() + static_cast<long>(scratch.size() / 2);
    std::nth_element(scratch.begin(), middle, scratch.end());
    return *middle;
}
} // namespace

std::vector<FluxPeak> pickPeaks(const std::vector<float>& flux, const PeakPickConfig& config)
{
    const int numFrames = static_cast<int>(flux.size());
    if (numFrames < 3 || config.hopSamples <= 0 || config.sampleRate <= 0.0)
        return {};

    const double framesPerMs = 0.001 * config.sampleRate / static_cast<double>(config.hopSamples);
    const int half = std::max(1, static_cast<int>(std::lround(
                                    0.5 * static_cast<double>(config.medianWindowMs) * framesPerMs)));
    const int minGap = std::max(1, static_cast<int>(std::lround(
                                     static_cast<double>(config.minIntervalMs) * framesPerMs)));

    double sum = 0.0;
    for (float value : flux)
        sum += static_cast<double>(value);

    const float mean = static_cast<float>(sum / static_cast<double>(numFrames));

    std::vector<FluxPeak> peaks;
    std::vector<float> scratch;
    scratch.reserve(static_cast<size_t>(2 * half + 1));

    for (int i = 1; i < numFrames - 1; ++i)
    {
        const float value = flux[static_cast<size_t>(i)];
        if (value <= flux[static_cast<size_t>(i - 1)] || value < flux[static_cast<size_t>(i + 1)])
            continue;

        scratch.clear();
        for (int j = std::max(0, i - half); j < std::min(numFrames, i + half + 1); ++j)
            scratch.push_back(flux[static_cast<size_t>(j)]);

        const float threshold = config.thresholdFactor * medianOf(scratch)
                                + config.thresholdBias * mean;

        if (value <= threshold || threshold <= 0.0f)
            continue;

        // Флэм разводится, дребезг одного удара — нет: из двух пиков ближе
        // минимального интервала остаётся больший.
        if (!peaks.empty() && i - peaks.back().frame < minGap)
        {
            if (value > peaks.back().value)
                peaks.back() = { i, value, threshold };

            continue;
        }

        peaks.push_back({ i, value, threshold });
    }

    return peaks;
}

} // namespace beat
