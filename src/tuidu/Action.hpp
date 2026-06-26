// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file Action.hpp
/// @brief The single dispatch vocabulary shared by the keymap, sort table, and UI.

#include <cstdint>

namespace tuidu
{

/// A user-triggerable action. The keymap maps key chords to these; the browser
/// interprets them. Keeping one closed enum lets dispatch be a table lookup rather
/// than scattered key-specific branches.
enum class Action : std::uint8_t
{
    None, ///< No-op / unbound.

    // Navigation
    MoveUp,     ///< Move the cursor up one row.
    MoveDown,   ///< Move the cursor down one row.
    MoveTop,    ///< Jump to the first row.
    MoveBottom, ///< Jump to the last row.
    PageUp,       ///< Scroll up one page.
    PageDown,     ///< Scroll down one page.
    HalfPageUp,   ///< Scroll up half a page (Ctrl+U).
    HalfPageDown, ///< Scroll down half a page (Ctrl+D).
    Descend,      ///< Enter the selected directory.
    Ascend,     ///< Go to the parent directory.

    // Sorting
    SortBySize,  ///< Sort by size (toggles direction on repeat).
    SortByName,  ///< Sort by name.
    SortByItems, ///< Sort by item count.
    SortByMtime, ///< Sort by modification time.

    // View toggles
    ToggleApparentDisk, ///< Switch between apparent size and disk usage.
    ToggleHidden,       ///< Show/hide dotfiles.

    // Commands
    SearchOpen, ///< Open the search/filter prompt.
    Rescan,     ///< Re-run the scan for the current root.
    ShowInfo,   ///< Show details for the selected item.
    Help,       ///< Toggle the help overlay.
    Quit,       ///< Exit the application.
};

} // namespace tuidu
