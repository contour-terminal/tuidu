// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file Keymap.hpp
/// @brief Data-driven vim-style key bindings: one table drives dispatch and the help overlay.

#include <tui/InputEvent.hpp>
#include <tui/KeyBindings.hpp>

#include <array>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <tuidu/Action.hpp>

namespace tuidu
{

/// One row of the static key-binding table. A row with empty @c help is an alias
/// (e.g. arrow keys mirroring j/k) and is hidden from the help overlay.
struct KeyBindingDef
{
    std::string_view chord; ///< Chord spec parsed by tui::KeyChord::parse (e.g. "shift+g").
    Action action;          ///< Action this chord triggers.
    std::string_view help;  ///< Help text; empty marks an alias (excluded from help).
};

/// The default vim-style bindings. Adding a binding is one row here; the same row
/// feeds both dispatch and the `?` help overlay — no parallel lists to keep in sync.
/// The size is deduced (std::to_array) so adding a row needs no count update.
inline constexpr auto kDefaultKeymap = std::to_array<KeyBindingDef>({
    { "j", Action::MoveDown, "Move down" },
    { "down", Action::MoveDown, "" },
    { "k", Action::MoveUp, "Move up" },
    { "up", Action::MoveUp, "" },
    { "g", Action::MoveTop, "Jump to top" },
    { "shift+g", Action::MoveBottom, "Jump to bottom" },
    { "pageup", Action::PageUp, "Page up" },
    { "pagedown", Action::PageDown, "Page down" },
    { "ctrl+u", Action::HalfPageUp, "Half page up" },
    { "ctrl+d", Action::HalfPageDown, "Half page down" },
    { "l", Action::Descend, "Open directory" },
    { "enter", Action::Descend, "" },
    { "h", Action::Ascend, "Go to parent" },
    { "backspace", Action::Ascend, "" },
    { "s", Action::SortBySize, "Sort by size" },
    { "n", Action::SortByName, "Sort by name" },
    { "shift+c", Action::SortByItems, "Sort by item count" },
    { "shift+m", Action::SortByMtime, "Sort by date" },
    { "a", Action::ToggleApparentDisk, "Apparent / disk usage" },
    { ".", Action::ToggleHidden, "Toggle hidden" },
    { "/", Action::SearchOpen, "Search" },
    { "r", Action::Rescan, "Rescan" },
    { "i", Action::ShowInfo, "Item info" },
    { "?", Action::Help, "Help" },
    { "q", Action::Quit, "Quit" },
    { "escape", Action::Quit, "" },
});

/// A help-overlay row: the displayed key and its description.
struct HelpEntry
{
    std::string_view key;  ///< The chord spec (e.g. "shift+g").
    std::string_view help; ///< The description.
};

/// Resolves key events to actions using a parsed binding table, and exposes the
/// help rows derived from that same table.
///
/// Built from @ref kDefaultKeymap by default; bindings can be overridden at runtime
/// (e.g. from user config) via @ref bind. Dispatch is a single linear match over the
/// parsed chords — no per-key branching.
class Keymap
{
  public:
    /// Constructs a keymap from the default binding table.
    Keymap();

    /// Constructs a keymap from a custom binding table (e.g. for tests or config).
    /// @param defs The binding rows to install.
    explicit Keymap(std::span<KeyBindingDef const> defs);

    /// Binds @p chordSpec to @p action, replacing any existing binding for that chord.
    /// @param chordSpec A chord spec parseable by tui::KeyChord::parse.
    /// @param action The action to trigger.
    /// @return true if the chord parsed and was bound.
    bool bind(std::string_view chordSpec, Action action);

    /// @return The action bound to @p event, or Action::None if unbound.
    [[nodiscard]] Action lookup(tui::KeyEvent const& event) const noexcept;

    /// @return The help-overlay rows (bindings with non-empty help), in table order.
    [[nodiscard]] std::vector<HelpEntry> helpEntries() const;

  private:
    void install(std::span<KeyBindingDef const> defs);

    std::vector<std::pair<tui::KeyChord, Action>> _bindings; ///< Parsed chord → action table.
    std::vector<HelpEntry> _help;                            ///< Help rows in declaration order.
};

} // namespace tuidu
