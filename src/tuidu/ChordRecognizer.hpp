// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file ChordRecognizer.hpp
/// @brief Recognizes two-key chord sequences (vim operators like `dd`) with a press timeout.

#include <tui/InputEvent.hpp>
#include <tui/KeyBindings.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include <tuidu/Action.hpp>
#include <tuidu/ChordSequence.hpp>

namespace tuidu
{

/// How a key event was interpreted by @ref ChordRecognizer::feed.
enum class ChordOutcome : std::uint8_t
{
    Passthrough, ///< Not part of any sequence; the caller should handle the key normally.
    Pending,     ///< Consumed as a lead key; the completing key is awaited (swallow this key).
    Completed,   ///< A sequence completed; see @ref ChordResult::action.
};

/// The result of feeding one key event.
struct ChordResult
{
    ChordOutcome outcome = ChordOutcome::Passthrough; ///< Interpretation of the fed key.
    Action action = Action::None;                     ///< Valid only when outcome == Completed.
};

/// Recognizes two-key chord sequences (e.g. vim's `dd`) from a data table, with a timeout
/// between the two presses so two slow, unrelated presses never trigger a sequence.
///
/// Time is read through an injected clock (dependency injection), so the timeout is unit-testable
/// without real sleeps. Elapsed time is evaluated lazily — when the next key arrives — so no timer
/// is needed: a completing key that comes too late simply re-arms as a fresh lead.
class ChordRecognizer
{
  public:
    /// Monotonic time source; injected so tests can drive time deterministically.
    using Clock = std::function<std::chrono::steady_clock::time_point()>;

    /// @param sequences The chord-sequence table to recognize (defaults to @ref kChordSequences).
    /// @param timeout Maximum gap allowed between the lead and completing key press.
    /// @param clock Monotonic time source; defaults to std::chrono::steady_clock::now.
    explicit ChordRecognizer(std::span<ChordSequenceDef const> sequences = kChordSequences,
                             std::chrono::milliseconds timeout = std::chrono::milliseconds { 500 },
                             Clock clock = {});

    /// Feeds a key event and reports how it was interpreted.
    /// @param event The key event to interpret.
    /// @return Passthrough (handle normally), Pending (swallow; awaiting completion), or
    ///         Completed (with the resolved @ref Action).
    [[nodiscard]] ChordResult feed(tui::KeyEvent const& event);

    /// Drops any pending lead key (e.g. when focus changes or a modal opens).
    void reset() noexcept;

    /// @return true while a lead key is pending its completing press.
    [[nodiscard]] bool pending() const noexcept { return _hasPending; }

  private:
    /// A parsed sequence row: the two chords and the action they emit.
    struct Parsed
    {
        tui::KeyChord first;  ///< Parsed lead chord.
        tui::KeyChord second; ///< Parsed completing chord.
        Action action;        ///< Action emitted on completion.
    };

    std::vector<Parsed> _sequences;     ///< Parsed sequence table.
    std::chrono::milliseconds _timeout; ///< Max gap between the two presses.
    Clock _clock;                       ///< Injected monotonic clock.

    bool _hasPending = false;                         ///< Whether a lead key is armed.
    std::vector<std::size_t> _candidates;             ///< Sequence indices the pending lead matched.
    std::chrono::steady_clock::time_point _pendingAt; ///< When the lead key was pressed.
};

} // namespace tuidu
