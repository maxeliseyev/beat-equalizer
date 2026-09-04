#include "doc/EditRegionPlan.h"

#include "doc/Document.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <utility>

using Catch::Approx;
using namespace beat;
using namespace beat::doc;

namespace
{
constexpr double kRate = 48000.0;

void addSource(Document& doc, double sampleRate = kRate)
{
    Source source;
    source.sampleRate = sampleRate;
    doc.addSource(std::move(source));
}

void observe(Event& event, int channel, double arrival, double attackEnd)
{
    auto& observation = event.channels[static_cast<size_t>(channel)];
    observation.present = true;
    observation.arrivalSamples = arrival;
    observation.attackEndSamples = attackEnd;
    observation.usefulEndSamples = attackEnd + 4800.0;
}

Event hit(double time, double closeAttackEnd, double otherArrival, double otherAttackEnd)
{
    Event event;
    event.timeSamples = time;
    event.referenceChannel = 0;
    observe(event, 0, time, closeAttackEnd);
    observe(event, 1, otherArrival, otherAttackEnd);
    return event;
}
} // namespace

TEST_CASE("edit region plan uses the shared protected zone, not reference-only attacks")
{
    Document doc;
    addSource(doc);

    const auto first = doc.addEvent(hit(1000.0, 1200.0, 1120.0, 1500.0));
    const auto second = doc.addEvent(hit(5000.0, 5200.0, 4900.0, 5300.0));

    const auto regions = buildEditRegionPlan(doc, { 0.0f, 5.0f });

    REQUIRE(regions.size() == 1);
    const auto& region = regions.front();
    CHECK(region.previousEvent == first);
    CHECK(region.nextEvent == second);
    CHECK(region.status == EditRegionStatus::ready);

    CHECK(region.previousProtected.start == Approx(1000.0));
    CHECK(region.previousProtected.end == Approx(1500.0));
    CHECK(region.nextProtected.start == Approx(4900.0));
    CHECK(region.nextProtected.end == Approx(5300.0));

    const double crossfade = 0.005 * kRate;
    CHECK(region.warpBudgetSamples == Approx(3400.0));
    CHECK(region.body.start == Approx(1500.0));
    CHECK(region.body.end == Approx(4900.0 - crossfade));
    CHECK(region.join.start == Approx(4900.0 - crossfade));
    CHECK(region.join.end == Approx(4900.0));
    CHECK(region.canWarpDecay());
    CHECK(region.canCrossfade());
}

TEST_CASE("edit region plan clamps crossfade when protected zones leave only a small gap")
{
    Document doc;
    addSource(doc);

    doc.addEvent(hit(1000.0, 2000.0, 1100.0, 1900.0));
    doc.addEvent(hit(2100.0, 2300.0, 2120.0, 2400.0));

    const auto regions = buildEditRegionPlan(doc, { 0.0f, 5.0f });

    REQUIRE(regions.size() == 1);
    const auto& region = regions.front();
    CHECK(region.status == EditRegionStatus::crossfadeClamped);
    CHECK(region.warpBudgetSamples == Approx(100.0));
    CHECK(region.requestedCrossfadeSamples == Approx(240.0));
    CHECK(region.body.empty());
    CHECK(region.join.start == Approx(2000.0));
    CHECK(region.join.end == Approx(2100.0));
    CHECK_FALSE(region.canWarpDecay());
    CHECK(region.canCrossfade());
}

TEST_CASE("edit region plan reports overlapped protected zones instead of inventing warp room")
{
    Document doc;
    addSource(doc);

    doc.addEvent(hit(1000.0, 2200.0, 1100.0, 2100.0));
    doc.addEvent(hit(2100.0, 2300.0, 2120.0, 2400.0));

    const auto regions = buildEditRegionPlan(doc, { 0.0f, 3.0f });

    REQUIRE(regions.size() == 1);
    const auto& region = regions.front();
    CHECK(region.status == EditRegionStatus::protectedZonesOverlap);
    CHECK(region.warpBudgetSamples == Approx(-100.0));
    CHECK(region.body.empty());
    CHECK(region.join.empty());
    CHECK_FALSE(region.canWarpDecay());
    CHECK_FALSE(region.canCrossfade());
}

TEST_CASE("edit region plan keeps crossfade options in milliseconds across sample rates")
{
    auto makePlan = [](double sampleRate)
    {
        Document doc;
        addSource(doc, sampleRate);

        const double samplesPerMs = 0.001 * sampleRate;
        doc.addEvent(hit(10.0 * samplesPerMs, 20.0 * samplesPerMs, 11.0 * samplesPerMs,
                         22.0 * samplesPerMs));
        doc.addEvent(hit(120.0 * samplesPerMs, 130.0 * samplesPerMs, 119.0 * samplesPerMs,
                         132.0 * samplesPerMs));

        return buildEditRegionPlan(doc, { 0.0f, 4.0f }).front();
    };

    const auto at48 = makePlan(48000.0);
    const auto at96 = makePlan(96000.0);

    CHECK(at48.join.length() / 48000.0 * 1000.0 == Approx(4.0));
    CHECK(at96.join.length() / 96000.0 * 1000.0 == Approx(4.0));
    CHECK(at96.join.length() == Approx(2.0 * at48.join.length()));
}

TEST_CASE("edit region plan marks adjacent events with missing observations")
{
    Document doc;
    addSource(doc);

    doc.addEvent(hit(1000.0, 1200.0, 1100.0, 1500.0));

    Event unobserved;
    unobserved.timeSamples = 5000.0;
    doc.addEvent(unobserved);

    const auto regions = buildEditRegionPlan(doc);

    REQUIRE(regions.size() == 1);
    CHECK(regions.front().status == EditRegionStatus::missingProtectedZone);
    CHECK(regions.front().body.empty());
    CHECK(regions.front().join.empty());
}
