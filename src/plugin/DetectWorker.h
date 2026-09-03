#pragma once

#include "doc/CrossMicMatcher.h"
#include "doc/Document.h"
#include "doc/SessionCalibration.h"
#include "doc/SpectralFluxDetector.h"

#include <atomic>
#include <functional>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

// Detect на стенде: калибровка, детектор, сверка по микрофонам — всё в своём
// потоке. В message thread приезжает готовый документ.
//
// Порядок не переставить: сверке нужны априорные задержки, их считает
// калибровка. Опорный канал у калибровки один — тот же, по которому потом
// ищутся события: строка профиля нужна ровно эта, а каждая лишняя опора стоит
// целого прохода детектора.
class DetectWorker final : private juce::Thread,
                           private juce::AsyncUpdater
{
public:
    struct Request
    {
        // Клип живёт в FilePlayer и не копируется: поколение проверяется
        // перед публикацией, и результат по устаревшему материалу выбрасывается.
        const juce::AudioBuffer<float>* clip = nullptr;
        int generation = -1;
        double sampleRate = 48000.0;
        int reference = 0;
        // Окно клипа, по которому считать.
        int from = 0;
        int length = 0;
    };

    struct Result
    {
        beat::doc::Document document;
        beat::doc::SessionProfile profile;
        beat::doc::CalibrationReport calibration;
        beat::doc::MatchReport match;
        int generation = -1;
        int from = 0;
        int length = 0;
        int reference = 0;
        double sampleRate = 0.0;
        bool valid = false;
    };

    DetectWorker();
    ~DetectWorker() override;

    // Message thread. Пока идёт предыдущий проход — запрос игнорируется.
    bool request(const Request& request);

    bool isBusy() const { return busy.load(std::memory_order_acquire); }
    const Result& result() const { return published; }

    std::function<void()> onResult;
    // Поколение материала на момент публикации: если оно уже другое, значит
    // стенд успели перезагрузить, и считать было не по чему.
    std::function<int()> currentGeneration;

    // Сам конвейер без потока и без message loop: калибровка, детектор,
    // сверка. Публичный, потому что проверять его надо числами, а не через
    // очередь сообщений.
    static void detect(const Request& request, Result& into);

private:
    void run() override;
    void handleAsyncUpdate() override;

    Request pending;
    Result finished;
    Result published;
    juce::WaitableEvent trigger;
    std::atomic<bool> busy { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DetectWorker)
};
