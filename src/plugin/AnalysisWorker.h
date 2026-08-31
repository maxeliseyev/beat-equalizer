#pragma once

#include "dsp/AlignmentEngine.h"
#include "dsp/AnalysisRing.h"

#include <atomic>
#include <functional>
#include <vector>

#include <juce_events/juce_events.h>

// Анализ живёт здесь, а не в processBlock: снимок кольцевого буфера, кадры,
// FFT. Результат приезжает в message thread через AsyncUpdater.
class AnalysisWorker final : private juce::Thread,
                             private juce::AsyncUpdater
{
public:
    explicit AnalysisWorker(const beat::AnalysisRing& ringToRead);
    ~AnalysisWorker() override;

    // Message thread. Сколько каналов и сэмплов брать из буфера.
    void prepare(int numChannels, int windowSamples);

    // Message thread. Пока идёт предыдущий проход — запрос игнорируется.
    bool request(const beat::AnalysisRequest& request);

    bool isBusy() const { return busy.load(std::memory_order_acquire); }
    const beat::AlignmentEngine::Result& result() const { return published; }
    int frameSize() const { return engine.frameSize(); }

    std::function<void()> onResult;

private:
    void run() override;
    void handleAsyncUpdate() override;

    const beat::AnalysisRing& ring;
    beat::AlignmentEngine engine;
    std::vector<float> scratch;
    std::vector<const float*> pointers;
    beat::AnalysisRequest pending;
    beat::AlignmentEngine::Result published;
    juce::WaitableEvent trigger;
    std::atomic<bool> busy { false };
    std::atomic<int> desiredChannels { 0 };
    std::atomic<int> desiredWindow { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalysisWorker)
};
