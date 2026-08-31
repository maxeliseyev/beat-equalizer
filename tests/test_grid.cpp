#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/Grid.h"

#include <array>

using Catch::Matchers::WithinAbs;
using namespace beat::grid;

TEST_CASE("grid steps are counted in quarters")
{
    REQUIRE_THAT(stepQuarters(Division::quarter), WithinAbs(1.0, 1.0e-9));
    REQUIRE_THAT(stepQuarters(Division::eighth), WithinAbs(0.5, 1.0e-9));
    REQUIRE_THAT(stepQuarters(Division::eighthTriplet), WithinAbs(1.0 / 3.0, 1.0e-9));
    REQUIRE_THAT(stepQuarters(Division::sixteenth), WithinAbs(0.25, 1.0e-9));
    REQUIRE_THAT(stepQuarters(Division::sixteenthTriplet), WithinAbs(1.0 / 6.0, 1.0e-9));
    REQUIRE_THAT(stepQuarters(Division::thirtySecond), WithinAbs(0.125, 1.0e-9));

    // Off — не «шаг ноль где-то в рисовании», а явный ноль здесь.
    REQUIRE(stepQuarters(Division::off) == 0.0);
}

TEST_CASE("a bar is four quarters in 4/4 and three in 6/8")
{
    REQUIRE_THAT(barQuarters(4, 4), WithinAbs(4.0, 1.0e-9));
    REQUIRE_THAT(barQuarters(6, 8), WithinAbs(3.0, 1.0e-9));
    REQUIRE_THAT(barQuarters(7, 8), WithinAbs(3.5, 1.0e-9));
    REQUIRE_THAT(barQuarters(0, 0), WithinAbs(4.0, 1.0e-9));
}

TEST_CASE("lines land where the window actually starts")
{
    std::array<Line, kMaxLines> lines {};

    // Окно ровно в одну четверть от нуля, шаг восьмая: 0 и 0.5.
    int count = linesInWindow(0.0, 1.0, 0.5, 4.0, lines.data(), kMaxLines);
    REQUIRE(count == 2);
    REQUIRE_THAT(lines[0].position, WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(lines[1].position, WithinAbs(0.5f, 1.0e-5f));
    REQUIRE(lines[0].beat);
    REQUIRE(lines[0].bar);
    REQUIRE_FALSE(lines[1].beat);

    // Окно начинается не на линии: первая линия — ближайшая справа.
    count = linesInWindow(0.3, 1.0, 0.25, 4.0, lines.data(), kMaxLines);
    REQUIRE(count == 4);
    REQUIRE_THAT(lines[0].position, WithinAbs(0.2f, 1.0e-5f));
    REQUIRE_THAT(lines[3].position, WithinAbs(0.95f, 1.0e-5f));
    REQUIRE(lines[2].beat); // четверть на 1.0
    REQUIRE_FALSE(lines[2].bar);
}

TEST_CASE("bar lines repeat with the time signature")
{
    std::array<Line, kMaxLines> lines {};
    const int count = linesInWindow(0.0, 8.0, 1.0, 4.0, lines.data(), kMaxLines);

    REQUIRE(count == 8);
    for (int i = 0; i < count; ++i)
    {
        REQUIRE(lines[static_cast<size_t>(i)].beat);
        REQUIRE(lines[static_cast<size_t>(i)].bar == (i % 4 == 0));
    }
}

TEST_CASE("the grid stays inside its buffer and its window")
{
    std::array<Line, kMaxLines> lines {};

    REQUIRE(linesInWindow(0.0, 4.0, 0.0, 4.0, lines.data(), kMaxLines) == 0);
    REQUIRE(linesInWindow(0.0, 0.0, 0.25, 4.0, lines.data(), kMaxLines) == 0);
    REQUIRE(linesInWindow(0.0, 4.0, 0.25, 4.0, nullptr, kMaxLines) == 0);

    // Просят больше линий, чем влезает: отдаём ровно столько, сколько попросили.
    REQUIRE(linesInWindow(0.0, 100.0, 0.25, 4.0, lines.data(), 8) == 8);

    // Отрицательная позиция (хост до нуля таймлайна) не ломает индексацию.
    const int count = linesInWindow(-0.6, 1.0, 0.5, 4.0, lines.data(), kMaxLines);
    REQUIRE(count == 2);
    REQUIRE_THAT(lines[0].position, WithinAbs(0.1f, 1.0e-5f));
    REQUIRE_THAT(lines[1].position, WithinAbs(0.6f, 1.0e-5f));
}
