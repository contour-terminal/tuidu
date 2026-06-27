// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file ChordSequence.hpp
/// @brief Data-driven table of two-key chord sequences (vim operators like `dd`).

#include <array>
#include <string_view>

#include <tuidu/Action.hpp>

namespace tuidu
{

/// One two-key chord sequence (a vim-style "operator", e.g. `dd`). Adding a sequence is one
/// row here — the same row drives both recognition (@ref ChordRecognizer) and the help overlay.
struct ChordSequenceDef
{
    std::string_view first;   ///< Lead key chord spec, parsed by tui::KeyChord::parse (e.g. "d").
    std::string_view second;  ///< Completing key chord spec (e.g. "d").
    Action action;            ///< Action emitted when the sequence completes within the timeout.
    std::string_view display; ///< How the chord is shown in help (e.g. "dd").
    std::string_view help;    ///< Help text; empty hides the row from the help overlay.
};

/// The default chord sequences. The size is deduced (std::to_array) so adding a row needs no
/// count update. `dd` deletes the selected item: pressing `d` twice in quick succession makes
/// the destructive intent unambiguous.
inline constexpr auto ChordSequences = std::to_array<ChordSequenceDef>({
    { .first = "d", .second = "d", .action = Action::Delete, .display = "dd", .help = "Delete selected" },
    { .first = "y",
      .second = "y",
      .action = Action::YankPath,
      .display = "yy",
      .help = "Copy path to clipboard" },
});

} // namespace tuidu
