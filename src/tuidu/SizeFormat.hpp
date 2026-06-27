// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file SizeFormat.hpp
/// @brief Data-driven human-readable size formatting (binary / SI unit tables).

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tuidu
{

/// Which unit family to format sizes in.
enum class UnitSystem : std::uint8_t
{
    Binary, ///< Powers of 1024: B, KiB, MiB, GiB, TiB, PiB, EiB.
    SI,     ///< Powers of 1000: B, kB, MB, GB, TB, PB, EB.
};

/// Parses a @ref UnitSystem from its lowercase config/CLI name (data-driven lookup).
/// @param name "binary" or "si" (case-sensitive).
/// @return The matching unit system, or @c std::nullopt if @p name is unrecognized.
[[nodiscard]] std::optional<UnitSystem> unitSystemFromString(std::string_view name) noexcept;

/// One row of a unit table: the threshold at/above which it applies, the divisor to
/// scale bytes into the unit, and the suffix to print. Rows are ordered largest-first.
struct UnitRow
{
    std::int64_t threshold;  ///< Smallest byte count that uses this unit.
    double divisor;          ///< Bytes per unit (e.g. 1024 for KiB).
    std::string_view suffix; ///< Printed unit suffix (e.g. "MiB").
};

/// Binary (1024-based) unit table, largest unit first.
inline constexpr std::array<UnitRow, 7> BinaryUnits { {
    { std::int64_t { 1 } << 60, 1152921504606846976.0, "EiB" },
    { std::int64_t { 1 } << 50, 1125899906842624.0, "PiB" },
    { std::int64_t { 1 } << 40, 1099511627776.0, "TiB" },
    { std::int64_t { 1 } << 30, 1073741824.0, "GiB" },
    { std::int64_t { 1 } << 20, 1048576.0, "MiB" },
    { std::int64_t { 1 } << 10, 1024.0, "KiB" },
    { 0, 1.0, "B" },
} };

/// SI (1000-based) unit table, largest unit first.
inline constexpr std::array<UnitRow, 7> SiUnits { {
    { 1'000'000'000'000'000'000LL, 1e18, "EB" },
    { 1'000'000'000'000'000LL, 1e15, "PB" },
    { 1'000'000'000'000LL, 1e12, "TB" },
    { 1'000'000'000LL, 1e9, "GB" },
    { 1'000'000LL, 1e6, "MB" },
    { 1'000LL, 1e3, "kB" },
    { 0, 1.0, "B" },
} };

/// Formats @p bytes as a human-readable string using @p system.
///
/// Picks the first table row whose threshold is <= |bytes| (rows are largest-first),
/// then prints with one decimal place — except whole bytes, which print as an integer.
/// Negative inputs are formatted with a leading '-'.
/// @param bytes The byte count to format.
/// @param system Which unit family to use (default binary).
/// @return The formatted size, e.g. "1.0 KiB" or "512 B".
[[nodiscard]] std::string formatSize(std::int64_t bytes, UnitSystem system = UnitSystem::Binary);

} // namespace tuidu
