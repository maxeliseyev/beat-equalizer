#include "doc/DelayField.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace beat;
using namespace beat::doc;

namespace
{
// Кит одного события: опора, близкий пораньше, оверхед и комната позже.
DelayField kitEvent(EventId id)
{
    DelayField field;
    field.setRaw(id, 0, 0.0);      // опора
    field.setRaw(id, 1, -4.0);     // пришёл раньше опоры
    field.setRaw(id, 2, 240.0);    // оверхед, 5 мс на 48 кГц
    field.setRaw(id, 3, 960.0);    // комната, 20 мс
    return field;
}
} // namespace

TEST_CASE("aligning to zero delays every channel by the same arrival")
{
    constexpr EventId id = 7;
    auto field = kitEvent(id);

    REQUIRE(field.maxRaw(id) == 960.0);
    CHECK(field.applied(id, 0) == Approx(960.0));
    CHECK(field.applied(id, 1) == Approx(964.0));
    CHECK(field.applied(id, 2) == Approx(720.0));
    CHECK(field.applied(id, 3) == Approx(0.0));
}

TEST_CASE("returning the room delay keeps the distance between microphones")
{
    constexpr EventId id = 7;
    auto field = kitEvent(id);

    field.setReturn(2, 1.0f);
    field.setReturn(3, 1.0f);

    // r = 1: канал стоит там, где его услышал микрофон, а весь кит просто
    // подождал самый дальний. Разница оверхед — комната сохранилась.
    CHECK(field.applied(id, 2) == Approx(960.0));
    CHECK(field.applied(id, 3) == Approx(960.0));
    CHECK(field.applied(id, 2) - field.applied(id, 0) == Approx(0.0));

    field.setReturn(3, 0.5f);
    CHECK(field.applied(id, 3) == Approx(480.0));
}

TEST_CASE("raw tdoa survives the return factor: alignment stays reversible")
{
    constexpr EventId id = 1;
    auto field = kitEvent(id);

    field.setReturn(3, 1.0f);
    (void) field.applied(id, 3);
    field.setReturn(3, 0.0f);

    CHECK(field.raw(id, 3) == 960.0);
    CHECK(field.applied(id, 3) == Approx(0.0));
}

TEST_CASE("applied delay is never negative")
{
    constexpr EventId id = 2;
    DelayField field;
    field.setRaw(id, 0, 0.0);
    field.setRaw(id, 1, -50.0);
    field.setRaw(id, 2, -10.0);

    for (int ch = 0; ch < 3; ++ch)
        CHECK(field.applied(id, ch) >= 0.0);

    // Все каналы пришли раньше опоры: ждёт опора, а не они.
    CHECK(field.maxRaw(id) == 0.0);
    CHECK(field.applied(id, 1) == Approx(50.0));
}

TEST_CASE("return factor is clamped and unmeasured channels stay silent")
{
    constexpr EventId id = 3;
    DelayField field;
    field.setRaw(id, 0, 0.0);
    field.setReturn(0, 3.0f);
    field.setReturn(1, -1.0f);

    CHECK(field.returnFactor(0) == 1.0f);
    CHECK(field.returnFactor(1) == 0.0f);
    CHECK_FALSE(field.has(id, 5));
    CHECK(field.applied(id, 5) == 0.0);
    CHECK(field.raw(id, 5) == 0.0);
}

TEST_CASE("delay rows are per event and go away with the event")
{
    DelayField field;
    field.setRaw(1, 0, 0.0);
    field.setRaw(1, 1, 100.0);
    field.setRaw(2, 0, 0.0);
    field.setRaw(2, 1, 130.0);

    CHECK(field.eventCount() == 2);
    CHECK(field.raw(1, 1) == 100.0);
    CHECK(field.raw(2, 1) == 130.0);

    field.eraseEvent(1);
    CHECK(field.eventCount() == 1);
    CHECK_FALSE(field.has(1, 1));
    CHECK(field.has(2, 1));
}

TEST_CASE("channel numbers outside the plugin range are ignored, not wrapped")
{
    DelayField field;
    field.setRaw(1, kMaxChannels, 100.0);
    field.setRaw(1, -1, 100.0);

    CHECK(field.eventCount() == 0);
    CHECK_FALSE(field.has(1, 0));
}
