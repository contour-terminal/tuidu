// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file ColorRules.hpp
/// @brief Data-driven row coloring: first matching rule selects a theme palette slot.

#include <tui/Theme.hpp>

#include <array>
#include <cstdint>
#include <span>

#include <tuidu/Node.hpp>
#include <tuidu/Tree.hpp>

namespace tuidu
{

/// Inputs a color rule evaluates against. Thresholds are data (from config), never
/// hardcoded inside the predicates.
struct ColorContext
{
    Tree const& tree;       ///< The tree being displayed.
    NodeId node;            ///< The node for this row.
    double percentOfParent; ///< This node's size as a fraction (0..1) of its parent.
    double largeThreshold;  ///< Fraction at/above which a row counts as "large".
    double hugeThreshold;   ///< Fraction at/above which a row counts as "huge".
};

/// A coloring rule: a predicate plus the palette slot (and bold hint) to apply when it
/// is the first matching rule. The slot is a pointer-to-member into @c tui::ColorPalette,
/// so switching themes recolors every row without touching the rules.
struct ColorRule
{
    bool (*match)(ColorContext const&);      ///< Predicate over the row.
    tui::RgbColor tui::ColorPalette::* slot; ///< Palette slot to use (pointer-to-member).
    bool bold;                               ///< Whether to render bold.
};

/// Result of resolving the color rules for a row.
struct ResolvedColor
{
    tui::RgbColor color; ///< The concrete color, looked up from the active palette.
    bool bold;           ///< Whether to render bold.
};

/// The coloring rules, evaluated top to bottom; first match wins. Adding a rule is one
/// row here.
[[nodiscard]] std::span<ColorRule const> colorRules() noexcept;

/// Resolves the row color for @p ctx against @p palette.
/// @param ctx The row inputs (node + thresholds).
/// @param palette The active theme palette to resolve the slot against.
/// @return The concrete color and bold flag from the first matching rule.
[[nodiscard]] ResolvedColor resolveColor(ColorContext const& ctx, tui::ColorPalette const& palette);

} // namespace tuidu
