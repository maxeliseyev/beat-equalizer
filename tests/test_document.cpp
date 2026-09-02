#include "doc/Document.h"

#include <catch2/catch_test_macros.hpp>

using namespace beat;
using namespace beat::doc;

namespace
{
Event hitAt(SamplePos time, int reference = 0)
{
    Event event;
    event.timeSamples = time;
    event.referenceChannel = reference;
    event.channels[static_cast<size_t>(reference)].present = true;
    event.channels[static_cast<size_t>(reference)].arrivalSamples = time;
    return event;
}
} // namespace

TEST_CASE("document keeps events ordered by time regardless of insertion order")
{
    Document doc;
    const auto late = doc.addEvent(hitAt(48000.0));
    const auto early = doc.addEvent(hitAt(1000.0));
    const auto middle = doc.addEvent(hitAt(24000.0));

    REQUIRE(doc.events().size() == 3);
    CHECK(doc.events()[0].id == early);
    CHECK(doc.events()[1].id == middle);
    CHECK(doc.events()[2].id == late);
}

TEST_CASE("event ids are never reused: the edit log points at them from outside")
{
    Document doc;
    const auto first = doc.addEvent(hitAt(100.0));
    REQUIRE(doc.removeEvent(first));

    const auto second = doc.addEvent(hitAt(100.0));
    CHECK(second != first);
    CHECK(doc.event(first) == nullptr);
}

TEST_CASE("channels are added in order and capped at the plugin maximum")
{
    Document doc;
    Source source;
    source.name = "kit";
    source.sampleRate = 48000.0;
    source.numChannels = kMaxChannels;
    const auto id = doc.addSource(std::move(source));

    for (int i = 0; i < kMaxChannels; ++i)
    {
        Channel channel;
        channel.source = id;
        channel.sourceChannel = i;
        channel.role = i == 0 ? ChannelRole::close : ChannelRole::overhead;
        CHECK(doc.addChannel(channel) == i);
    }

    Channel overflow;
    overflow.source = id;
    CHECK(doc.addChannel(overflow) == kInvalidId);
    CHECK(doc.channelCount() == kMaxChannels);
    CHECK(doc.channel(0)->role == ChannelRole::close);
    CHECK(doc.sampleRate() == 48000.0);
}

TEST_CASE("changing thresholds throws away decisions and keeps features")
{
    Document doc;
    Source source;
    source.sampleRate = 48000.0;
    const auto id = doc.addSource(std::move(source));

    Feature envelope;
    envelope.data = { 0.0f, 1.0f, 0.5f };
    envelope.hopSamples = 64.0;
    doc.features().put(id, 0, FeatureKind::envelope, 0, envelope);

    const auto event = doc.addEvent(hitAt(500.0));
    doc.delays().setRaw(event, 1, 12.0);
    REQUIRE(doc.delays().eventCount() == 1);

    doc.clearEvents();

    CHECK(doc.events().empty());
    CHECK(doc.delays().eventCount() == 0);
    CHECK(doc.features().contains(id, 0, FeatureKind::envelope));
}

TEST_CASE("feature cache invalidates by source only")
{
    Document doc;
    Source a;
    a.sampleRate = 48000.0;
    Source b = a;
    const auto first = doc.addSource(std::move(a));
    const auto second = doc.addSource(std::move(b));

    Feature feature;
    feature.data = { 1.0f };
    doc.features().put(first, 0, FeatureKind::envelope, 0, feature);
    doc.features().put(second, 0, FeatureKind::envelope, 0, feature);
    doc.features().put(second, 1, FeatureKind::spectralFlux, 0, feature);
    REQUIRE(doc.features().size() == 3);

    doc.features().invalidateSource(second);

    CHECK(doc.features().size() == 1);
    CHECK(doc.features().contains(first, 0, FeatureKind::envelope));
    CHECK_FALSE(doc.features().contains(second, 0, FeatureKind::envelope));
}

TEST_CASE("feature positions round-trip through the hop grid")
{
    Feature feature;
    feature.data.assign(100, 0.0f);
    feature.hopSamples = 128.0;
    feature.firstSampleOffset = 64.0;

    CHECK(FeatureCache::timeOf(feature, 0) == 64.0);
    CHECK(FeatureCache::timeOf(feature, 2) == 320.0);
    CHECK(FeatureCache::indexAt(feature, 320.0) == 2);
    CHECK(FeatureCache::indexAt(feature, 400.0) == 2);
    CHECK(FeatureCache::indexAt(feature, 0.0) == 0);
    CHECK(FeatureCache::indexAt(feature, 1.0e9) == feature.size() - 1);
}

TEST_CASE("only a mistake verdict reaches the training set")
{
    EditLog log;

    EditEntry mistake;
    mistake.action = EditAction::removedEvent;
    mistake.verdict = EditVerdict::detectorWasWrong;
    mistake.detector = "spectral-flux/1";

    EditEntry deliberate = mistake;
    deliberate.verdict = EditVerdict::leaveAlone;

    EditEntry groove = mistake;
    groove.verdict = EditVerdict::artistic;

    CHECK(log.append(mistake) == 1);
    CHECK(log.append(deliberate) == 2);
    CHECK(log.append(groove) == 3);

    CHECK(log.size() == 3);
    REQUIRE(log.mistakes().size() == 1);
    CHECK(log.mistakes().front().sequence == 1);
}

TEST_CASE("detector stamp travels with the results")
{
    Document doc;
    doc.setDetectorStamp({ "spectral-flux", "1.2.0", "threshold=0.3;bands=8" });

    CHECK(doc.detectorStamp().name == "spectral-flux");
    CHECK(doc.detectorStamp().version == "1.2.0");

    doc.clear();
    CHECK(doc.detectorStamp().name.empty());
}

TEST_CASE("observation count separates a real hit from a single bleeding channel")
{
    auto event = hitAt(1000.0);
    CHECK(observationCount(event) == 1);

    event.channels[3].present = true;
    event.channels[7].present = true;
    CHECK(observationCount(event) == 3);
}
