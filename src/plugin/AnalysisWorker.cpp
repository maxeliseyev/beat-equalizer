#include "AnalysisWorker.h"

#include <algorithm>

AnalysisWorker::AnalysisWorker(const beat::AnalysisRing& ringToRead)
    : juce::Thread("beat-analysis"),
      ring(ringToRead)
{
    startThread(juce::Thread::Priority::low);
}

AnalysisWorker::~AnalysisWorker()
{
    signalThreadShouldExit();
    trigger.signal();
    stopThread(2000);
    cancelPendingUpdate();
}

void AnalysisWorker::prepare(int numChannels, int windowSamples)
{
    desiredChannels.store(std::clamp(numChannels, 0, beat::kMaxChannels), std::memory_order_relaxed);
    desiredWindow.store(std::max(windowSamples, 0), std::memory_order_relaxed);
}

bool AnalysisWorker::request(const beat::AnalysisRequest& newRequest)
{
    if (busy.exchange(true, std::memory_order_acq_rel))
        return false;

    pending = newRequest;
    trigger.signal();
    return true;
}

void AnalysisWorker::run()
{
    while (!threadShouldExit())
    {
        trigger.wait(-1);
        if (threadShouldExit())
            break;

        const int channels = desiredChannels.load(std::memory_order_relaxed);
        const int window = desiredWindow.load(std::memory_order_relaxed);
        const auto request = pending;

        beat::AlignmentEngine::Result result;

        if (channels >= beat::kMinChannels && window > 0)
        {
            const auto needed = static_cast<size_t>(channels) * static_cast<size_t>(window);
            if (scratch.size() != needed)
                scratch.assign(needed, 0.0f);

            pointers.resize(static_cast<size_t>(channels));
            for (int ch = 0; ch < channels; ++ch)
                pointers[static_cast<size_t>(ch)] =
                    scratch.data() + static_cast<std::ptrdiff_t>(ch) * window;

            const int available = ring.readLast(scratch.data(), channels, window);
            result = engine.analyze(pointers.data(), channels, available, request);
        }
        else
        {
            result.status = beat::AnalysisStatus::badRequest;
        }

        published = result;
        triggerAsyncUpdate();
    }
}

void AnalysisWorker::handleAsyncUpdate()
{
    busy.store(false, std::memory_order_release);

    if (onResult != nullptr)
        onResult();
}
