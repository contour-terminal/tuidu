// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <ranges>
#include <utility>

#include <tuidu/ChordRecognizer.hpp>

namespace tuidu
{

ChordRecognizer::ChordRecognizer(std::span<ChordSequenceDef const> sequences,
                                 std::chrono::milliseconds timeout,
                                 Clock clock):
    _timeout(timeout),
    _clock(clock ? std::move(clock) : Clock { [] { return std::chrono::steady_clock::now(); } })
{
    _sequences.reserve(sequences.size());
    for (auto const& def: sequences)
    {
        auto const first = tui::KeyChord::parse(def.first);
        auto const second = tui::KeyChord::parse(def.second);
        if (first.has_value() && second.has_value())
            _sequences.push_back(Parsed { .first = *first, .second = *second, .action = def.action });
    }
}

ChordResult ChordRecognizer::feed(tui::KeyEvent const& event)
{
    auto const now = _clock();

    // A lead key is armed: try to complete a sequence with this key, if still within the timeout.
    if (_hasPending)
    {
        if (now - _pendingAt <= _timeout)
        {
            for (auto const idx: _candidates)
            {
                if (_sequences[idx].second.matches(event))
                {
                    _hasPending = false;
                    _candidates.clear();
                    return ChordResult { .outcome = ChordOutcome::Completed,
                                         .action = _sequences[idx].action };
                }
            }
        }
        // Timed out, or this key does not complete the pending lead: drop it and re-interpret the
        // key below (so it may itself start a fresh sequence or pass through normally).
        _hasPending = false;
        _candidates.clear();
    }

    // No pending lead: arm one if this key is the lead of any sequence.
    _candidates.clear();
    for (auto const idx: std::views::iota(std::size_t { 0 }, _sequences.size()))
    {
        if (_sequences[idx].first.matches(event))
            _candidates.push_back(idx);
    }
    if (!_candidates.empty())
    {
        _hasPending = true;
        _pendingAt = now;
        return ChordResult { .outcome = ChordOutcome::Pending };
    }

    return ChordResult { .outcome = ChordOutcome::Passthrough };
}

void ChordRecognizer::reset() noexcept
{
    _hasPending = false;
    _candidates.clear();
}

} // namespace tuidu
