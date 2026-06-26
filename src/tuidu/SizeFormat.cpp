// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <cstdlib>
#include <format>
#include <span>

#include <tuidu/SizeFormat.hpp>

namespace tuidu
{

namespace
{
    /// @return The unit table for @p system.
    [[nodiscard]] std::span<UnitRow const> unitsFor(UnitSystem system) noexcept
    {
        switch (system)
        {
            case UnitSystem::SI: return kSiUnits;
            case UnitSystem::Binary: break;
        }
        return kBinaryUnits;
    }
} // namespace

std::string formatSize(std::int64_t bytes, UnitSystem system)
{
    auto const negative = bytes < 0;
    auto const magnitude = negative ? -bytes : bytes;
    auto const units = unitsFor(system);

    // Rows are largest-first; pick the first whose threshold the magnitude reaches.
    auto const row = std::ranges::find_if(units, [&](UnitRow const& r) { return magnitude >= r.threshold; });
    auto const& unit = (row != units.end()) ? *row : units.back();

    auto const* const sign = negative ? "-" : "";

    // Whole bytes print as an integer (e.g. "512 B"); scaled units get one decimal.
    if (unit.divisor <= 1.0)
        return std::format("{}{} {}", sign, magnitude, unit.suffix);

    auto const scaled = static_cast<double>(magnitude) / unit.divisor;
    return std::format("{}{:.1f} {}", sign, scaled, unit.suffix);
}

} // namespace tuidu
