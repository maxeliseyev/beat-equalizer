#pragma once

#include "doc/Event.h"
#include "doc/FeatureCache.h"
#include "doc/Source.h"

#include <string>
#include <vector>

namespace beat::doc
{

struct AnalysisContext
{
    double sampleRate = 48000.0;

    // Канал, по которому ищутся события. В остальных удар не ищется заново, а
    // предсказывается по d[i] — это следующий шаг, не дело детектора.
    int referenceChannel = 0;

    // Положение переданного блока от нуля сессии.
    SamplePos startSample = 0.0;

    // Куда сложить тяжёлые признаки; nullptr — не складывать.
    FeatureCache* features = nullptr;
    SourceId source = kInvalidId;

    // Роли каналов: метаданные, не ветка алгоритма (инвариант 8).
    const std::vector<Channel>* channels = nullptr;
};

// Детектор — сменный блок. Всё, что выше по стеку, работает с Event и не
// знает, кто его породил: поток спектра, шаблон, NMF или однажды модель.
// Субсэмплевое время детектор не выдаёт и не должен: это всегда GCC-PHAT.
class IOnsetDetector
{
public:
    virtual ~IOnsetDetector() = default;

    virtual std::string name() const = 0;
    virtual std::string version() const = 0;

    // Пороги как они были на момент анализа: пишутся в проект вместе с
    // результатами, иначе переанализ невоспроизводим.
    virtual std::string parameters() const = 0;

    // channels — channel-major, у каждого канала numSamples сэмплов.
    virtual std::vector<Event> analyze(const float* const* channels,
                                       int numChannels,
                                       int numSamples,
                                       const AnalysisContext& context) = 0;
};

} // namespace beat::doc
