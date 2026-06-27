// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file Columns.hpp
/// @brief Data-driven browser columns: a table of header + width + formatter.

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <tuidu/SizeFormat.hpp>
#include <tuidu/Tree.hpp>

namespace tuidu
{

/// Which size metric the size/bar/percent columns report.
enum class SizeMode : std::uint8_t
{
    Apparent, ///< Apparent size (st_size).
    Disk,     ///< Allocated disk usage (st_blocks * 512).
};

/// Parses a @ref SizeMode from its lowercase config/CLI name (data-driven lookup).
/// @param name "apparent" or "disk" (case-sensitive).
/// @return The matching size mode, or @c std::nullopt if @p name is unrecognized.
[[nodiscard]] std::optional<SizeMode> sizeModeFromString(std::string_view name) noexcept;

/// Everything a column formatter needs to render one row. Pure inputs — no I/O.
struct RenderCtx
{
    Tree const& tree;  ///< The tree being displayed.
    NodeId node;       ///< The node for this row.
    NodeId parent;     ///< The node's parent (for percent-of-parent); InvalidNode if none.
    SizeMode sizeMode; ///< Apparent vs disk usage.
    UnitSystem units;  ///< Binary vs SI unit formatting.
    int barWidth;      ///< Width (cells) of the bar-graph column.

    /// @return The size metric for @p id under the active @ref SizeMode.
    [[nodiscard]] std::int64_t metric(NodeId id) const noexcept;
};

/// How a column claims horizontal space.
enum class WidthPolicy : std::uint8_t
{
    Fixed, ///< Exactly @c width cells.
    Flex,  ///< At least @c width cells; absorbs leftover space (one Flex column max).
};

/// Text alignment within a column.
enum class Align : std::uint8_t
{
    Left,
    Right,
};

/// A column descriptor: its header, sizing, formatter, and alignment.
struct ColumnDef
{
    std::string_view header;                 ///< Column header label.
    WidthPolicy widthPolicy;                 ///< Fixed or flex sizing.
    int width;                               ///< Fixed width, or minimum width for Flex.
    std::string (*format)(RenderCtx const&); ///< Value extractor + formatter.
    Align align;                             ///< Text alignment.
};

/// The browser's column layout, left to right. Adding a column is one row here.
[[nodiscard]] std::span<ColumnDef const> columns() noexcept;

// --- Formatters (exposed for unit testing) -------------------------------------

/// @return Human-readable size for the row's node under the active size mode.
[[nodiscard]] std::string fmtSize(RenderCtx const& ctx);

/// @return A proportional bar graph (e.g. "###  ") of node size vs parent size.
[[nodiscard]] std::string fmtBar(RenderCtx const& ctx);

/// @return The node's percent of its parent's size, e.g. " 42%".
[[nodiscard]] std::string fmtPercent(RenderCtx const& ctx);

/// @return The subtree item count.
[[nodiscard]] std::string fmtItems(RenderCtx const& ctx);

/// @return The node's mtime formatted as YYYY-MM-DD HH:MM.
[[nodiscard]] std::string fmtMtime(RenderCtx const& ctx);

/// @return The node's name, prefixed with "/" when it is a directory.
[[nodiscard]] std::string fmtName(RenderCtx const& ctx);

} // namespace tuidu
