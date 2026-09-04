#include "doc/CrossfadeEditAdapter.h"

#include "doc/Document.h"
#include "dsp/CrossfadeRenderer.h"
#include "dsp/Constants.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

using Catch::Approx;
using namespace beat;
using namespace beat::doc;

namespace
{
constexpr double kRate = 48000.0;

SourceId addSource(Document& doc)
{
    Source source;
    source.sampleRate = kRate;
    source.numChannels = 2;
    return doc.addSource(std::move(source));
}

void addChannels(Document& doc, SourceId source, int count)
{
    for (int ch = 0; ch < count; ++ch)
    {
        Channel channel;
        channel.source = source;
        channel.sourceChannel = ch;
        doc.addChannel(channel);
    }
}

void observe(Event& event, int channel, double arrival, double attackEnd)
{
    auto& observation = event.channels[static_cast<size_t>(channel)];
    observation.present = true;
    observation.arrivalSamples = arrival;
    observation.attackEndSamples = attackEnd;
    observation.usefulEndSamples = attackEnd + 4800.0;
}

Event hit(double time, double attackEnd)
{
    Event event;
    event.timeSamples = time;
    event.referenceChannel = 0;
    observe(event, 0, time, attackEnd);
    observe(event, 1, time + 20.0, attackEnd + 40.0);
    return event;
}

std::vector<const float*> readPointers(const std::vector<std::vector<float>>& channels)
{
    std::vector<const float*> pointers;
    pointers.reserve(channels.size());
    for (const auto& channel : channels)
        pointers.push_back(channel.data());
    return pointers;
}

std::vector<float*> writePointers(std::vector<std::vector<float>>& channels)
{
    std::vector<float*> pointers;
    pointers.reserve(channels.size());
    for (auto& channel : channels)
        pointers.push_back(channel.data());
    return pointers;
}
} // namespace

TEST_CASE("crossfade edit adapter emits a seed point and a joined target point")
{
    Document doc;
    const auto source = addSource(doc);
    addChannels(doc, source, 2);

    const auto first = doc.addEvent(hit(1000.0, 1200.0));
    const auto second = doc.addEvent(hit(5000.0, 5200.0));
    doc.delays().setRaw(first, 0, 0.0);
    doc.delays().setRaw(first, 1, 50.0);
    doc.delays().setRaw(second, 0, 0.0);
    doc.delays().setRaw(second, 1, 80.0);

    const auto plan = buildCrossfadeEditPlan(doc, { 2, { 0.0f, 5.0f } });

    REQUIRE(plan.editPoints.size() == 2);
    CHECK(plan.regionsConsidered == 1);
    CHECK(plan.crossfadesPrepared == 1);
    CHECK(plan.regionsSkipped == 0);
    CHECK(plan.eventsWithoutDelay == 0);

    const auto& seed = plan.editPoints[0];
    CHECK(seed.timeSamples == Approx(1000.0));
    CHECK(seed.valid[0]);
    CHECK(seed.valid[1]);
    CHECK(seed.delaySamples[0] == Approx(50.0f));
    CHECK(seed.delaySamples[1] == Approx(0.0f));

    const auto& target = plan.editPoints[1];
    CHECK(target.timeSamples == Approx(5000.0));
    CHECK(target.joinStartSamples == Approx(5000.0 - 240.0));
    CHECK(target.joinEndSamples == Approx(5000.0));
    CHECK(target.delaySamples[0] == Approx(80.0f));
    CHECK(target.delaySamples[1] == Approx(0.0f));
}

TEST_CASE("crossfade edit adapter skips regions that have no crossfade budget")
{
    Document doc;
    const auto source = addSource(doc);
    addChannels(doc, source, 1);

    const auto first = doc.addEvent(hit(1000.0, 2200.0));
    const auto second = doc.addEvent(hit(2100.0, 2300.0));
    doc.delays().setRaw(first, 0, 0.0);
    doc.delays().setRaw(second, 0, 10.0);

    const auto plan = buildCrossfadeEditPlan(doc, { 1, { 0.0f, 3.0f } });

    CHECK(plan.regionsConsidered == 1);
    CHECK(plan.crossfadesPrepared == 0);
    CHECK(plan.regionsSkipped == 1);
    CHECK(plan.editPoints.empty());
}

TEST_CASE("crossfade edit adapter reports events without delay rows")
{
    Document doc;
    const auto source = addSource(doc);
    addChannels(doc, source, 1);

    const auto first = doc.addEvent(hit(1000.0, 1200.0));
    doc.addEvent(hit(5000.0, 5200.0));
    doc.delays().setRaw(first, 0, 0.0);

    const auto plan = buildCrossfadeEditPlan(doc, { 1, { 0.0f, 3.0f } });

    CHECK(plan.regionsConsidered == 1);
    CHECK(plan.crossfadesPrepared == 0);
    CHECK(plan.eventsWithoutDelay == 1);
    CHECK(plan.regionsSkipped == 1);
    CHECK(plan.editPoints.empty());
}

TEST_CASE("crossfade edit adapter output can drive the crossfade renderer")
{
    constexpr int samples = 256;
    Document doc;
    const auto source = addSource(doc);
    addChannels(doc, source, 1);

    const auto first = doc.addEvent(hit(16.0, 40.0));
    const auto second = doc.addEvent(hit(128.0, 150.0));
    doc.delays().setRaw(first, 0, 0.0);
    doc.delays().setRaw(second, 0, -10.0);

    const auto plan = buildCrossfadeEditPlan(doc, { 1, { 0.0f, 10.0f } });
    REQUIRE(plan.editPoints.size() == 2);
    CHECK(plan.editPoints[1].joinStartSamples == Approx(80.0));
    CHECK(plan.editPoints[1].joinEndSamples == Approx(128.0));

    std::vector<std::vector<float>> input(1, std::vector<float>(samples, 0.0f));
    for (int i = 0; i < samples; ++i)
        input[0][static_cast<size_t>(i)] = static_cast<float>(i);

    std::vector<std::vector<float>> output(1, std::vector<float>(samples, 0.0f));
    auto in = readPointers(input);
    auto out = writePointers(output);

    CrossfadeRenderer::Options options;
    options.sampleRate = kRate;
    options.numChannels = 1;
    options.numSamples = samples;

    CrossfadeRenderer renderer;
    const auto result = renderer.render(in.data(), out.data(), options, plan.editPoints);

    REQUIRE(result.editPointsUsed == 2);
    CHECK(result.crossfadesRendered == 1);
    CHECK(output[0][79] == Approx(79.0f - kInterpolatorLatencySamples).margin(0.01f));
    CHECK(output[0][128] == Approx(128.0f - 10.0f - kInterpolatorLatencySamples).margin(0.01f));
}
