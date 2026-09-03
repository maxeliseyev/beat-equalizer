#include "GlideRenderer.h"

#include "Coherence.h"
#include "FractionalDelay.h"

#include <algorithm>
#include <cmath>

namespace beat
{

namespace
{
constexpr float kReachedToleranceSamples = 0.1f;

bool validChannel(int channel, int numChannels)
{
    return channel >= 0 && channel < numChannels;
}

int sampleOf(double position, int numSamples)
{
    if (!std::isfinite(position) || numSamples <= 0)
        return 0;

    return std::clamp(static_cast<int>(std::lround(position)), 0, numSamples - 1);
}

float clampDelay(float delaySamples, double sampleRate)
{
    const float maxDelay = sampleRate > 0.0
                               ? kMaxDelayMs * 0.001f * static_cast<float>(sampleRate)
                               : 0.0f;
    return std::clamp(delaySamples, 0.0f, maxDelay);
}

int msToSamples(float ms, double sampleRate)
{
    if (sampleRate <= 0.0)
        return 0;

    return std::max(1, static_cast<int>(std::lround(0.001 * static_cast<double>(ms) * sampleRate)));
}

void copyThrough(const float* const* source, float* const* destination, int channels, int samples)
{
    for (int ch = 0; ch < channels; ++ch)
    {
        if (source[ch] == nullptr || destination[ch] == nullptr)
            continue;

        std::copy(source[ch], source[ch] + samples, destination[ch]);
    }
}

std::vector<GlideRenderer::EventDelay> sortedEvents(
    const std::vector<GlideRenderer::EventDelay>& events,
    int numChannels,
    int numSamples)
{
    std::vector<GlideRenderer::EventDelay> usable;
    usable.reserve(events.size());

    for (auto event : events)
    {
        if (!std::isfinite(event.timeSamples) || event.timeSamples < 0.0
            || event.timeSamples >= static_cast<double>(numSamples))
            continue;

        bool hasDelay = false;
        for (int ch = 0; ch < numChannels; ++ch)
            hasDelay = hasDelay || event.valid[static_cast<size_t>(ch)];

        if (!hasDelay)
            continue;

        event.referenceChannel = std::clamp(event.referenceChannel, 0, numChannels - 1);
        event.protectUntilSamples = std::max(event.protectUntilSamples, event.timeSamples);
        usable.push_back(event);
    }

    std::sort(usable.begin(),
              usable.end(),
              [](const auto& a, const auto& b) { return a.timeSamples < b.timeSamples; });

    return usable;
}

std::vector<std::array<float, kMaxChannels>> buildTargets(
    const std::vector<GlideRenderer::EventDelay>& events,
    int numChannels,
    double sampleRate,
    float strength)
{
    std::vector<std::array<float, kMaxChannels>> targets(events.size());
    std::array<float, kMaxChannels> current {};
    const float amount = std::clamp(strength, 0.0f, 1.0f);

    for (size_t index = 0; index < events.size(); ++index)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto channel = static_cast<size_t>(ch);
            if (events[index].valid[channel])
                current[channel] =
                    clampDelay(events[index].delaySamples[channel], sampleRate) * amount;
        }

        targets[index] = current;
    }

    return targets;
}

GlideRenderer::EventMetric measureEvent(const float* const* source,
                                         int numChannels,
                                         int numSamples,
                                         double sampleRate,
                                         const GlideRenderer::EventDelay& event,
                                         const std::array<float, kMaxChannels>& actualDelay,
                                         const std::array<float, kMaxChannels>& targetDelay)
{
    GlideRenderer::EventMetric metric;
    metric.timeSamples = event.timeSamples;
    metric.referenceChannel = event.referenceChannel;
    metric.actualDelaySamples = actualDelay;
    metric.targetDelaySamples = targetDelay;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto channel = static_cast<size_t>(ch);
        if (event.valid[channel]
            && std::abs(actualDelay[channel] - targetDelay[channel]) > kReachedToleranceSamples)
            metric.limited = true;
    }

    const int reference = event.referenceChannel;
    if (source == nullptr || !validChannel(reference, numChannels) || source[reference] == nullptr)
        return metric;

    const int frame = msToSamples(kMatchFrameMs, sampleRate);
    if (frame < 16 || numSamples < 16)
        return metric;

    const int preRoll = msToSamples(kMatchPreRollMs, sampleRate);
    const int eventSample = sampleOf(event.timeSamples, numSamples);
    const int start = std::clamp(eventSample - preRoll, 0, std::max(0, numSamples - frame));
    const int count = std::min(frame, numSamples - start);
    if (count < 16)
        return metric;

    Coherence coherence(fftOrderFor(count));
    double before = 0.0;
    double after = 0.0;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (ch == reference || source[ch] == nullptr)
            continue;

        const auto channel = static_cast<size_t>(ch);
        if (!event.valid[channel])
            continue;

        coherence.setPair(source[reference] + start, source[ch] + start, count, sampleRate);
        before += static_cast<double>(coherence.measureRaw());

        Coherence::Transform transform;
        transform.delaySamples = actualDelay[channel]
                                 - actualDelay[static_cast<size_t>(reference)];
        after += static_cast<double>(coherence.measure(transform));
        ++metric.channelsMeasured;
    }

    if (metric.channelsMeasured > 0)
    {
        const double scale = 1.0 / static_cast<double>(metric.channelsMeasured);
        metric.coherenceBefore = static_cast<float>(before * scale);
        metric.coherenceAfter = static_cast<float>(after * scale);
    }

    return metric;
}
} // namespace

void GlideRenderer::EventDelay::setDelay(int channel, float delay)
{
    if (channel < 0 || channel >= kMaxChannels)
        return;

    const auto index = static_cast<size_t>(channel);
    delaySamples[index] = delay;
    valid[index] = true;
}

GlideRenderer::Result GlideRenderer::render(const float* const* source,
                                            float* const* destination,
                                            const Options& options,
                                            const std::vector<EventDelay>& events) const
{
    Result result;
    result.numChannels = std::clamp(options.numChannels, 0, kMaxChannels);
    result.numSamples = std::max(0, options.numSamples);

    if (source == nullptr || destination == nullptr || result.numChannels <= 0
        || result.numSamples <= 0 || options.sampleRate <= 0.0)
        return result;

    auto usableEvents = sortedEvents(events, result.numChannels, result.numSamples);
    if (options.strength <= 0.0f || usableEvents.empty())
    {
        copyThrough(source, destination, result.numChannels, result.numSamples);

        const std::array<float, kMaxChannels> zeroDelay {};
        result.events.reserve(usableEvents.size());
        for (const auto& event : usableEvents)
        {
            result.events.push_back(measureEvent(source,
                                                 result.numChannels,
                                                 result.numSamples,
                                                 options.sampleRate,
                                                 event,
                                                 zeroDelay,
                                                 zeroDelay));
        }
        result.eventsMeasured = static_cast<int>(result.events.size());
        return result;
    }

    auto targets = buildTargets(usableEvents,
                                result.numChannels,
                                options.sampleRate,
                                options.strength);
    std::vector<int> eventSamples(usableEvents.size());
    std::vector<int> protectUntil(usableEvents.size());
    for (size_t i = 0; i < usableEvents.size(); ++i)
    {
        eventSamples[i] = sampleOf(usableEvents[i].timeSamples, result.numSamples);
        protectUntil[i] = sampleOf(usableEvents[i].protectUntilSamples, result.numSamples);
    }

    std::array<float, kMaxChannels> currentDelay = targets.front();
    const float maxStep = std::max(0.0f, options.maxSlew);

    FractionalDelay delay;
    delay.prepare(options.sampleRate, result.numChannels);
    delay.reset();

    result.events.reserve(usableEvents.size());
    size_t nextEvent = 0;
    int activeEvent = -1;
    int frozenUntil = -1;

    for (int sample = 0; sample < result.numSamples; ++sample)
    {
        while (nextEvent < usableEvents.size() && eventSamples[nextEvent] <= sample)
        {
            activeEvent = static_cast<int>(nextEvent);
            frozenUntil = std::max(frozenUntil, protectUntil[nextEvent]);

            auto metric = measureEvent(source,
                                       result.numChannels,
                                       result.numSamples,
                                       options.sampleRate,
                                       usableEvents[nextEvent],
                                       currentDelay,
                                       targets[nextEvent]);
            if (metric.limited)
                ++result.limitedEvents;
            result.events.push_back(metric);
            ++nextEvent;
        }

        const bool frozen = activeEvent >= 0 && sample <= frozenUntil;
        if (!frozen)
        {
            const size_t targetIndex =
                nextEvent < targets.size()
                    ? nextEvent
                    : static_cast<size_t>(std::max(activeEvent, 0));
            const auto& target = targets[targetIndex];

            for (int ch = 0; ch < result.numChannels; ++ch)
            {
                const auto channel = static_cast<size_t>(ch);
                const float delta = target[channel] - currentDelay[channel];
                const float step = std::clamp(delta, -maxStep, maxStep);
                currentDelay[channel] += step;
                result.maxSlewObserved = std::max(result.maxSlewObserved, std::abs(step));
            }
        }

        for (int ch = 0; ch < result.numChannels; ++ch)
        {
            if (source[ch] == nullptr || destination[ch] == nullptr)
                continue;

            destination[ch][sample] = delay.processSampleAtDelay(
                ch,
                source[ch][sample],
                currentDelay[static_cast<size_t>(ch)]);
        }
    }

    result.finalDelaySamples = currentDelay;
    result.eventsMeasured = static_cast<int>(result.events.size());
    return result;
}

} // namespace beat
