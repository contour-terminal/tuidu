// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include <tuidu/SortMode.hpp>

namespace tuidu
{

namespace
{
    /// Size descending, then name ascending as a stable tie-breaker.
    [[nodiscard]] bool lessBySizeDesc(Tree const& t, NodeId a, NodeId b)
    {
        if (t[a].aggSize != t[b].aggSize)
            return t[a].aggSize > t[b].aggSize;
        return t.name(a) < t.name(b);
    }

    /// Size ascending, then name ascending.
    [[nodiscard]] bool lessBySizeAsc(Tree const& t, NodeId a, NodeId b)
    {
        if (t[a].aggSize != t[b].aggSize)
            return t[a].aggSize < t[b].aggSize;
        return t.name(a) < t.name(b);
    }

    /// Name ascending (case-sensitive byte order).
    [[nodiscard]] bool lessByName(Tree const& t, NodeId a, NodeId b)
    {
        return t.name(a) < t.name(b);
    }

    /// Item count descending, then name ascending.
    [[nodiscard]] bool lessByItemsDesc(Tree const& t, NodeId a, NodeId b)
    {
        if (t[a].itemCount != t[b].itemCount)
            return t[a].itemCount > t[b].itemCount;
        return t.name(a) < t.name(b);
    }

    /// Modification time descending (newest first), then name ascending.
    [[nodiscard]] bool lessByMtimeDesc(Tree const& t, NodeId a, NodeId b)
    {
        if (t[a].mtime != t[b].mtime)
            return t[a].mtime > t[b].mtime;
        return t.name(a) < t.name(b);
    }

    constexpr std::array<SortDef, 5> SortModes { {
        { .label = "Size ↓",
          .key = "size-desc",
          .triggerAction = Action::SortBySize,
          .less = &lessBySizeDesc },
        { .label = "Size ↑", .key = "size-asc", .triggerAction = Action::SortBySize, .less = &lessBySizeAsc },
        { .label = "Name", .key = "name", .triggerAction = Action::SortByName, .less = &lessByName },
        { .label = "Items ↓",
          .key = "items",
          .triggerAction = Action::SortByItems,
          .less = &lessByItemsDesc },
        { .label = "Date ↓", .key = "date", .triggerAction = Action::SortByMtime, .less = &lessByMtimeDesc },
    } };
} // namespace

std::span<SortDef const> sortModes() noexcept
{
    return SortModes;
}

std::size_t nextSortMode(std::size_t current, Action action) noexcept
{
    auto const count = SortModes.size();
    if (current >= count)
        current = 0;

    // Re-selecting the current mode's action cycles to the next row with that action.
    if (SortModes[current].triggerAction == action)
    {
        for (auto offset = std::size_t { 1 }; offset < count; ++offset)
        {
            auto const idx = (current + offset) % count;
            if (SortModes[idx].triggerAction == action)
                return idx;
        }
        return current; // only one row uses this action
    }

    // Otherwise jump to the first row matching the action.
    for (auto idx = std::size_t { 0 }; idx < count; ++idx)
        if (SortModes[idx].triggerAction == action)
            return idx;

    return current; // action not a sort trigger
}

std::optional<std::size_t> sortModeIndexFromKey(std::string_view key) noexcept
{
    auto const row = std::ranges::find(SortModes, key, &SortDef::key);
    if (row == SortModes.end())
        return std::nullopt;
    return static_cast<std::size_t>(std::ranges::distance(SortModes.begin(), row));
}

} // namespace tuidu
