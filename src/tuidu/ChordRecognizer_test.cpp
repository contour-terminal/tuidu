// SPDX-License-Identifier: Apache-2.0
#include <tui/InputEvent.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>

#include <tuidu/ChordRecognizer.hpp>
#include <tuidu/ChordSequence.hpp>

using namespace tuidu;
using namespace std::chrono_literals;

namespace
{

/// Builds a KeyEvent for a printable character (no modifiers).
tui::KeyEvent keyChar(char32_t c)
{
    return tui::KeyEvent { .key = tui::keyCodeFromCodepoint(c),
                           .modifiers = tui::Modifier::None,
                           .codepoint = c };
}

/// A manually-advanced clock so the timeout is exercised deterministically (no real sleeps).
struct FakeClock
{
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::time_point::min();

    [[nodiscard]] auto operator()() const { return now; }
};

} // namespace

TEST_CASE("ChordRecognizer: dd within the timeout completes to Delete", "[chord]")
{
    auto clock = FakeClock {};
    auto recognizer = ChordRecognizer { kChordSequences, 500ms, [&clock] {
                                           return clock.now;
                                       } };

    auto const lead = recognizer.feed(keyChar(U'd'));
    CHECK(lead.outcome == ChordOutcome::Pending);
    CHECK(recognizer.pending());

    clock.now += 100ms; // second press comfortably within the timeout
    auto const completed = recognizer.feed(keyChar(U'd'));
    CHECK(completed.outcome == ChordOutcome::Completed);
    CHECK(completed.action == Action::Delete);
    CHECK_FALSE(recognizer.pending());
}

TEST_CASE("ChordRecognizer: a non-completing key passes through and is not swallowed", "[chord]")
{
    auto clock = FakeClock {};
    auto recognizer = ChordRecognizer { kChordSequences, 500ms, [&clock] {
                                           return clock.now;
                                       } };

    CHECK(recognizer.feed(keyChar(U'd')).outcome == ChordOutcome::Pending);

    clock.now += 50ms;
    // 'j' does not complete `dd`; it must pass through so the caller can dispatch it normally.
    auto const result = recognizer.feed(keyChar(U'j'));
    CHECK(result.outcome == ChordOutcome::Passthrough);
    CHECK_FALSE(recognizer.pending());
}

TEST_CASE("ChordRecognizer: a second 'd' after the timeout does not delete, it re-arms", "[chord]")
{
    auto clock = FakeClock {};
    auto recognizer = ChordRecognizer { kChordSequences, 500ms, [&clock] {
                                           return clock.now;
                                       } };

    CHECK(recognizer.feed(keyChar(U'd')).outcome == ChordOutcome::Pending);

    clock.now += 800ms; // too slow — the lead has lapsed
    auto const result = recognizer.feed(keyChar(U'd'));
    CHECK(result.outcome == ChordOutcome::Pending); // re-armed as a fresh lead, NOT completed
    CHECK(result.action == Action::None);
    CHECK(recognizer.pending());
}

TEST_CASE("ChordRecognizer: an unrelated key passes straight through", "[chord]")
{
    auto recognizer = ChordRecognizer {};
    auto const result = recognizer.feed(keyChar(U'j'));
    CHECK(result.outcome == ChordOutcome::Passthrough);
    CHECK_FALSE(recognizer.pending());
}

TEST_CASE("ChordRecognizer: reset drops a pending lead", "[chord]")
{
    auto recognizer = ChordRecognizer {};
    CHECK(recognizer.feed(keyChar(U'd')).outcome == ChordOutcome::Pending);
    recognizer.reset();
    CHECK_FALSE(recognizer.pending());
    // After reset, the next 'd' is a fresh lead again, not a completion.
    CHECK(recognizer.feed(keyChar(U'd')).outcome == ChordOutcome::Pending);
}
