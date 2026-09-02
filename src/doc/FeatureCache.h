#pragma once

#include "doc/Ids.h"

#include <map>
#include <tuple>
#include <vector>

namespace beat::doc
{

enum class FeatureKind
{
    envelope = 0,   // широкополосная огибающая
    bandEnvelope,   // огибающая одной логарифмической полосы, variant = номер
    spectralFlux,   // полуволновой поток спектра
    noiseFloor      // пол дорожки: низкий процентиль в скользящем окне
};

// Ряд отсчётов с шагом hop. Хранит своё положение во времени, чтобы
// потребитель не пересчитывал его из чужих допущений про кадры.
struct Feature
{
    std::vector<float> data;
    double hopSamples = 1.0;
    SamplePos firstSampleOffset = 0.0;

    bool empty() const { return data.empty(); }
    int size() const { return static_cast<int>(data.size()); }
};

// Признаки живут отдельно от решений (detector-design 1.4). Смена детектора
// или порогов не должна пересчитывать тяжёлое: события выбрасываются, кэш
// остаётся. Инвалидация — только по источнику, потому что только звук и
// делает признак недействительным.
class FeatureCache
{
public:
    const Feature* find(SourceId source,
                        int channel,
                        FeatureKind kind,
                        int variant = 0) const;

    bool contains(SourceId source, int channel, FeatureKind kind, int variant = 0) const;

    void put(SourceId source, int channel, FeatureKind kind, int variant, Feature feature);

    void invalidateSource(SourceId source);
    void clear();
    int size() const;

    // Время точки ряда в сэмплах от нуля сессии и обратно.
    static SamplePos timeOf(const Feature& feature, int index);
    static int indexAt(const Feature& feature, SamplePos position);

private:
    using Key = std::tuple<SourceId, int, FeatureKind, int>;

    std::map<Key, Feature> entries;
};

} // namespace beat::doc
