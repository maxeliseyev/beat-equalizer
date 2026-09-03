#include "DetectWorker.h"

#include <algorithm>
#include <vector>

using namespace beat;
using namespace beat::doc;

DetectWorker::DetectWorker()
    : juce::Thread("beat-detect")
{
    startThread(juce::Thread::Priority::low);
}

DetectWorker::~DetectWorker()
{
    signalThreadShouldExit();
    trigger.signal();
    stopThread(5000);
    cancelPendingUpdate();
}

bool DetectWorker::request(const Request& newRequest)
{
    if (newRequest.clip == nullptr || newRequest.length <= 0 || newRequest.sampleRate <= 0.0)
        return false;

    if (busy.exchange(true, std::memory_order_acq_rel))
        return false;

    pending = newRequest;
    trigger.signal();
    return true;
}

void DetectWorker::detect(const Request& request, Result& into)
{
    const auto& clip = *request.clip;
    const int channels = std::min(clip.getNumChannels(), kMaxChannels);
    const int available = clip.getNumSamples();

    const int from = std::clamp(request.from, 0, std::max(0, available - 1));
    const int length = std::clamp(request.length, 0, available - from);
    if (channels <= 0 || length <= 0)
        return;

    std::vector<const float*> pointers(static_cast<size_t>(channels));
    for (int ch = 0; ch < channels; ++ch)
        pointers[static_cast<size_t>(ch)] = clip.getReadPointer(ch) + from;

    const int reference = std::clamp(request.reference, 0, channels - 1);

    // Документ помнит, где лежит звук и что про него известно. Источник здесь
    // один — клип стенда; каналы описываются, чтобы события и поле задержек
    // индексировались тем же номером, что и строки таблицы.
    for (int ch = 0; ch < channels; ++ch)
    {
        Source source;
        source.name = "bench";
        source.sampleRate = request.sampleRate;
        source.numChannels = channels;
        source.numSamples = available;
        const SourceId id = into.document.addSource(source);

        Channel channel;
        channel.source = id;
        channel.sourceChannel = ch;
        into.document.addChannel(channel);
    }

    SpectralFluxDetector detector;

    // Калибровка первой: сверке нужны априорные задержки, иначе дальние
    // микрофоны она честно отсеет, а ближние померит грубее, чем могла бы.
    CalibrationContext calibration;
    calibration.sampleRate = request.sampleRate;
    calibration.startSample = static_cast<double>(from);
    calibration.references = { reference };

    SessionCalibration session;
    into.profile = session.run(detector, pointers.data(), channels, length,
                               calibration, &into.calibration);

    AnalysisContext analysis;
    analysis.sampleRate = request.sampleRate;
    analysis.referenceChannel = reference;
    analysis.startSample = static_cast<double>(from);
    analysis.channels = &into.document.channels();

    into.document.setDetectorStamp({ detector.name(), detector.version(), detector.parameters() });
    for (auto& event : detector.analyze(pointers.data(), channels, length, analysis))
        into.document.addEvent(event);

    MatchContext match;
    match.sampleRate = request.sampleRate;
    match.startSample = static_cast<double>(from);
    into.profile.priors(reference, match.prior);
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto index = static_cast<size_t>(ch);
        const auto& stat = into.profile.delay(reference, ch);
        match.priorKnown[index] = stat.known;
        match.priorSpread[index] = stat.spreadSamples;
    }

    CrossMicMatcher matcher;
    into.match = matcher.match(into.document, pointers.data(), channels, length, match);

    into.generation = request.generation;
    into.from = from;
    into.length = length;
    into.reference = reference;
    into.sampleRate = request.sampleRate;
    into.valid = true;
}

void DetectWorker::run()
{
    while (!threadShouldExit())
    {
        trigger.wait(-1);
        if (threadShouldExit())
            break;

        const auto request = pending;

        finished = Result {};
        detect(request, finished);

        triggerAsyncUpdate();
    }
}

void DetectWorker::handleAsyncUpdate()
{
    // Материал успели сменить, пока считали: числа относятся к другому клипу,
    // и показывать их нельзя.
    const bool stale = currentGeneration != nullptr
                       && currentGeneration() != finished.generation;

    if (!stale)
        published = std::move(finished);

    finished = Result {};
    busy.store(false, std::memory_order_release);

    if (onResult != nullptr)
        onResult();
}
