#include "doc/FeatureCache.h"

#include <algorithm>
#include <cmath>

namespace beat::doc
{

const Feature* FeatureCache::find(SourceId source,
                                  int channel,
                                  FeatureKind kind,
                                  int variant) const
{
    const auto it = entries.find(Key { source, channel, kind, variant });
    return it == entries.end() ? nullptr : &it->second;
}

bool FeatureCache::contains(SourceId source, int channel, FeatureKind kind, int variant) const
{
    return find(source, channel, kind, variant) != nullptr;
}

void FeatureCache::put(SourceId source,
                       int channel,
                       FeatureKind kind,
                       int variant,
                       Feature feature)
{
    entries[Key { source, channel, kind, variant }] = std::move(feature);
}

void FeatureCache::invalidateSource(SourceId source)
{
    for (auto it = entries.begin(); it != entries.end();)
        it = std::get<0>(it->first) == source ? entries.erase(it) : std::next(it);
}

void FeatureCache::clear()
{
    entries.clear();
}

int FeatureCache::size() const
{
    return static_cast<int>(entries.size());
}

SamplePos FeatureCache::timeOf(const Feature& feature, int index)
{
    return feature.firstSampleOffset + static_cast<double>(index) * feature.hopSamples;
}

int FeatureCache::indexAt(const Feature& feature, SamplePos position)
{
    if (feature.empty() || feature.hopSamples <= 0.0)
        return 0;

    const double raw = (position - feature.firstSampleOffset) / feature.hopSamples;
    return std::clamp(static_cast<int>(std::floor(raw)), 0, feature.size() - 1);
}

} // namespace beat::doc
