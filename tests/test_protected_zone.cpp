#include "doc/ProtectedZone.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace beat;
using namespace beat::doc;

namespace
{
constexpr double kRate = 48000.0;

void observe(Event& event, int channel, double arrival, double attackEnd)
{
    auto& observation = event.channels[static_cast<size_t>(channel)];
    observation.present = true;
    observation.arrivalSamples = arrival;
    observation.attackEndSamples = attackEnd;
    observation.usefulEndSamples = attackEnd + 4800.0;
}
} // namespace

TEST_CASE("protected zone is the union across microphones, not the reference attack")
{
    Event event;
    event.referenceChannel = 0;
    event.timeSamples = 10000.0;

    // Близкий отзвучал первым, оверхед пришёл позже и ещё в атаке.
    observe(event, 0, 10000.0, 10240.0);
    observe(event, 1, 10120.0, 10480.0);
    observe(event, 2, 10576.0, 10900.0);

    const auto zone = protectedZone(event, kRate, 0.0f);

    CHECK(zone.start == Approx(10000.0));
    CHECK(zone.end == Approx(10900.0));

    // Зона опорного канала одна была бы короче почти на 14 мс — ровно то, что
    // одноканальный инструмент не видит и чем портит оверхед.
    const double referenceOnly = 10240.0 - 10000.0;
    CHECK(zone.length() > referenceOnly + 600.0);
}

TEST_CASE("margin widens the zone on both sides")
{
    Event event;
    observe(event, 0, 1000.0, 1200.0);

    const auto bare = protectedZone(event, kRate, 0.0f);
    const auto padded = protectedZone(event, kRate, kProtectedMarginMs);
    const double margin = 0.001 * static_cast<double>(kProtectedMarginMs) * kRate;

    CHECK(padded.start == Approx(bare.start - margin));
    CHECK(padded.end == Approx(bare.end + margin));
    CHECK(padded.contains(1000.0));
    CHECK_FALSE(padded.contains(padded.end));
}

TEST_CASE("channels that did not hear the hit do not stretch the zone")
{
    Event event;
    observe(event, 0, 1000.0, 1200.0);

    auto& silent = event.channels[5];
    silent.present = false;
    silent.arrivalSamples = 0.0;
    silent.attackEndSamples = 100000.0;

    const auto zone = protectedZone(event, kRate, 0.0f);
    CHECK(zone.start == Approx(1000.0));
    CHECK(zone.end == Approx(1200.0));
}

TEST_CASE("an event nobody observed has no zone")
{
    Event event;
    CHECK(protectedZone(event, kRate).empty());
    CHECK(protectedZone(event, kRate).length() == 0.0);

    Event heard;
    observe(heard, 0, 1000.0, 1200.0);
    CHECK(protectedZone(heard, 0.0).empty());
}

TEST_CASE("warp budget goes negative when the zones collide")
{
    // 120 BPM, шестнадцатая — 125 мс = 6000 сэмплов.
    Event first;
    observe(first, 0, 0.0, 1440.0);   // 30 мс атаки
    observe(first, 1, 480.0, 2400.0); // оверхед на 10 мс дальше

    Event second;
    observe(second, 0, 6000.0, 7440.0);
    observe(second, 1, 6480.0, 8400.0);

    const auto a = protectedZone(first, kRate, kProtectedMarginMs);
    const auto b = protectedZone(second, kRate, kProtectedMarginMs);

    const double budget = warpBudget(a, b);
    CHECK(budget > 0.0);
    CHECK(budget < 6000.0);

    // Тот же кит вдвое быстрее — бюджет кончился, и это должно быть видно
    // числом, а не услышано в размазанной атаке.
    Event tight;
    observe(tight, 0, 2400.0, 3840.0);
    observe(tight, 1, 2880.0, 4800.0);
    CHECK(warpBudget(a, protectedZone(tight, kRate, kProtectedMarginMs)) < 0.0);
}
