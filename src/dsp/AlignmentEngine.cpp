#include "AlignmentEngine.h"

#include "LatencyModel.h"

#include <algorithm>
#include <cmath>

namespace beat
{

namespace
{
float rms(const float* x, int count)
{
    if (x == nullptr || count <= 0)
        return 0.0f;

    double sum = 0.0;
    for (int i = 0; i < count; ++i)
        sum += static_cast<double>(x[i]) * static_cast<double>(x[i]);

    return static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
}

float median(std::vector<float>& values)
{
    if (values.empty())
        return 0.0f;

    const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
    std::nth_element(values.begin(), middle, values.end());
    const float upper = *middle;

    if (values.size() % 2 != 0)
        return upper;

    const float lower = *std::max_element(values.begin(), middle);
    return 0.5f * (lower + upper);
}
} // namespace

AlignmentEngine::AlignmentEngine(int fftOrder)
    : gcc(fftOrder),
      frame(1 << fftOrder)
{
}

AlignmentEngine::Result AlignmentEngine::analyze(const float* const* channels,
                                                 int numChannels,
                                                 int numSamples,
                                                 const AnalysisRequest& request)
{
    Result result;
    result.numChannels = std::clamp(numChannels, 0, kMaxChannels);
    result.reference = std::clamp(request.reference, 0, std::max(result.numChannels - 1, 0));

    if (channels == nullptr || result.numChannels < kMinChannels || request.sampleRate <= 0.0)
    {
        result.status = AnalysisStatus::badRequest;
        return result;
    }

    if (numSamples < frame)
    {
        result.status = AnalysisStatus::notEnoughData;
        return result;
    }

    const int maxLag = maxLagSamples(request.maxDistanceM, request.sampleRate);
    if (maxLag <= 0)
    {
        result.status = AnalysisStatus::badRequest;
        return result;
    }

    for (int ch = 0; ch < result.numChannels; ++ch)
    {
        lagsPerChannel[static_cast<size_t>(ch)].clear();
        ratiosPerChannel[static_cast<size_t>(ch)].clear();
        invertVotes[static_cast<size_t>(ch)] = 0;
    }

    const float* reference = channels[result.reference];
    const int hop = hopSize();

    for (int start = 0; start + frame <= numSamples; start += hop)
    {
        ++result.framesTotal;

        if (rms(reference + start, frame) < kAnalysisMinRms)
            continue;

        ++result.framesLoud;

        for (int ch = 0; ch < result.numChannels; ++ch)
        {
            if (ch == result.reference || channels[ch] == nullptr)
                continue;

            const float* signal = channels[ch] + start;
            if (rms(signal, frame) < kAnalysisMinRms)
                continue;

            const auto frameResult =
                gcc.estimate(reference + start, signal, frame, maxLag, request.sampleRate);

            if (!frameResult.valid || frameResult.peakRatio < kAnalysisMinPeakRatio)
                continue;

            lagsPerChannel[static_cast<size_t>(ch)].push_back(frameResult.lagSamples);
            ratiosPerChannel[static_cast<size_t>(ch)].push_back(frameResult.peakRatio);
            invertVotes[static_cast<size_t>(ch)] += frameResult.invert ? 1 : -1;
        }
    }

    if (result.framesTotal == 0)
    {
        result.status = AnalysisStatus::notEnoughData;
        return result;
    }

    if (result.framesLoud == 0)
    {
        result.status = AnalysisStatus::tooQuiet;
        return result;
    }

    float tdoa[kMaxChannels] {};
    bool enabled[kMaxChannels] {};

    for (int ch = 0; ch < result.numChannels; ++ch)
    {
        auto& estimate = result.channels[static_cast<size_t>(ch)];
        enabled[ch] = true;

        if (ch == result.reference)
        {
            estimate.valid = true;
            estimate.confidence = 1.0f;
            estimate.framesUsed = result.framesLoud;
            continue;
        }

        auto& lags = lagsPerChannel[static_cast<size_t>(ch)];
        estimate.framesUsed = static_cast<int>(lags.size());

        if (estimate.framesUsed < kAnalysisMinFrames)
            continue;

        // Канал без внятной оценки остаётся с tdoa = 0: он поедет вместе с
        // опорой, а не останется на месте относительно остального кита.
        estimate.tdoaSamples = median(lags);
        estimate.confidence = median(ratiosPerChannel[static_cast<size_t>(ch)]);
        estimate.invert = invertVotes[static_cast<size_t>(ch)] > 0;
        estimate.valid = true;
        tdoa[ch] = estimate.tdoaSamples;
    }

    auto& snapshot = result.snapshot;
    snapshot = AlignmentSnapshot::identity(result.numChannels);
    snapshot.reference = result.reference;

    LatencyModel::applyTdoa(tdoa,
                            enabled,
                            result.numChannels,
                            snapshot.delaySamples,
                            snapshot.latencySamples);

    for (int ch = 0; ch < result.numChannels; ++ch)
        snapshot.invert[ch] = result.channels[static_cast<size_t>(ch)].invert;

    result.status = AnalysisStatus::ok;
    return result;
}

} // namespace beat
