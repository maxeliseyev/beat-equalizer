#include "AlignmentEngine.h"

#include "AllpassRotator.h"
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
    // Корреляции БПФ на порядок больше кадра: кадр здесь равен `1 << fftOrder`
    // целиком, и вместе с окном поиска в БПФ того же размера он не помещается
    // — свёртка круговая, и лаг считался бы по куску из другого конца кадра.
    // Когерентности лишний порядок не нужен: там нет поиска по лагу.
    : gcc(fftOrder + 1),
      coherence(fftOrder),
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
    int loudestFrame = -1;
    float loudestRms = 0.0f;

    for (int start = 0; start + frame <= numSamples; start += hop)
    {
        ++result.framesTotal;

        const float frameRms = rms(reference + start, frame);
        if (frameRms < kAnalysisMinRms)
            continue;

        ++result.framesLoud;

        // Когерентность и перебор ротатора считаем на одном кадре: самом
        // громком. Так метрика не размывается тишиной между ударами.
        if (frameRms > loudestRms)
        {
            loudestRms = frameRms;
            loudestFrame = start;
        }

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

    measureAndRotate(channels, loudestFrame, request, result);

    for (int ch = 0; ch < result.numChannels; ++ch)
    {
        const auto& estimate = result.channels[static_cast<size_t>(ch)];
        snapshot.rotatorAmount[ch] = estimate.rotatorAmount;
        snapshot.rotatorCoeff[ch] =
            (estimate.rotatorAmount > 0.0f)
                ? AllpassRotator::coefficient(estimate.rotatorHz, request.sampleRate)
                : 0.0f;
    }

    result.status = AnalysisStatus::ok;
    return result;
}

void AlignmentEngine::measureAndRotate(const float* const* channels,
                                       int loudestFrame,
                                       const AnalysisRequest& request,
                                       Result& result)
{
    if (loudestFrame < 0)
        return;

    const float* reference = channels[result.reference] + loudestFrame;
    constexpr float amounts[] = { 0.25f, 0.5f, 0.75f, 1.0f };
    const float hzRatio = kRotatorSearchHighHz / kRotatorSearchLowHz;

    double sumBefore = 0.0;
    double sumAfter = 0.0;
    int measured = 0;

    for (int ch = 0; ch < result.numChannels; ++ch)
    {
        if (ch == result.reference || channels[ch] == nullptr)
            continue;

        auto& estimate = result.channels[static_cast<size_t>(ch)];
        coherence.setPair(reference, channels[ch] + loudestFrame, frame, request.sampleRate);

        Coherence::Transform transform;
        // Канал звучит позже опоры, поэтому в метрике двигаем его вперёд.
        transform.delaySamples = -estimate.tdoaSamples;
        transform.invert = estimate.invert;

        const float before = coherence.measureRaw();
        const float aligned = coherence.measure(transform);

        float bestValue = aligned;
        float bestHz = kDefaultRotatorHz;
        float bestAmount = 0.0f;

        if (estimate.valid)
        {
            for (int step = 0; step < kRotatorSearchSteps; ++step)
            {
                const float t = static_cast<float>(step)
                                / static_cast<float>(kRotatorSearchSteps - 1);
                const float hz = kRotatorSearchLowHz * std::pow(hzRatio, t);

                for (float amount : amounts)
                {
                    auto candidate = transform;
                    candidate.rotatorHz = hz;
                    candidate.rotatorAmount = amount;

                    const float value = coherence.measure(candidate);
                    if (value > bestValue)
                    {
                        bestValue = value;
                        bestHz = hz;
                        bestAmount = amount;
                    }
                }
            }
        }

        // Вращать фазу ради сотых долей процента не стоит: остаёмся на bypass.
        const bool worthRotating = bestAmount > 0.0f && bestValue > aligned + kRotatorMinGain;

        estimate.coherenceBefore = before;
        estimate.coherenceAfter = worthRotating ? bestValue : aligned;
        estimate.rotatorHz = worthRotating ? bestHz : kDefaultRotatorHz;
        estimate.rotatorAmount = worthRotating ? bestAmount : 0.0f;

        sumBefore += static_cast<double>(before);
        sumAfter += static_cast<double>(estimate.coherenceAfter);
        ++measured;
    }

    if (measured > 0)
    {
        result.coherenceBefore = static_cast<float>(sumBefore / measured);
        result.coherenceAfter = static_cast<float>(sumAfter / measured);
    }
}

} // namespace beat
