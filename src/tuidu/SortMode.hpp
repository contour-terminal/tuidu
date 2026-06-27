// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file SortMode.hpp
/// @brief Data-driven sort modes: a table of label + trigger action + comparator.

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include <tuidu/Action.hpp>
#include <tuidu/Tree.hpp>

namespace tuidu
{

/// A comparator over two sibling nodes: true if @p a sorts before @p b.
using SortComparator = bool (*)(Tree const&, NodeId a, NodeId b);

/// One selectable sort order. @c triggerAction links it to the keymap; pressing the
/// same key again is what cycles to the next row sharing that action (e.g. size ↓/↑).
struct SortDef
{
    std::string_view label; ///< Human-readable label (for the status bar).
    std::string_view key;   ///< Stable config/CLI identifier (e.g. "size-desc").
    Action triggerAction;   ///< Keymap action that selects/cycles to this mode.
    SortComparator less;    ///< Ordering comparator.
};

/// The available sort modes, in cycle order. Adding a mode is one row here.
[[nodiscard]] std::span<SortDef const> sortModes() noexcept;

/// Finds the next sort mode to apply when @p action fires, given the @p current index.
///
/// If @p action selects a different mode, returns that mode's index. If it re-selects
/// the current mode's action, advances to the next row sharing that action (wrapping),
/// which is how a repeated key toggles ascending/descending. Returns @p current if no
/// row matches @p action.
/// @param current Index of the currently-active sort mode.
/// @param action The triggering action.
/// @return Index into @ref sortModes of the mode to apply next.
[[nodiscard]] std::size_t nextSortMode(std::size_t current, Action action) noexcept;

/// Looks up a sort mode by its stable @c SortDef::key (data-driven lookup).
/// @param key One of the @ref sortModes keys, e.g. "size-desc", "name", "date".
/// @return The index into @ref sortModes, or @c std::nullopt if @p key is unrecognized.
[[nodiscard]] std::optional<std::size_t> sortModeIndexFromKey(std::string_view key) noexcept;

} // namespace tuidu
