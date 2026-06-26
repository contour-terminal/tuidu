// SPDX-License-Identifier: Apache-2.0
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

    constexpr std::array<SortDef, 5> kSortModes { {
        { .label = "Size ↓", .triggerAction = Action::SortBySize, .less = &lessBySizeDesc },
        { .label = "Size ↑", .triggerAction = Action::SortBySize, .less = &lessBySizeAsc },
        { .label = "Name", .triggerAction = Action::SortByName, .less = &lessByName },
        { .label = "Items ↓", .triggerAction = Action::SortByItems, .less = &lessByItemsDesc },
        { .label = "Date ↓", .triggerAction = Action::SortByMtime, .less = &lessByMtimeDesc },
    } };
} // namespace

std::span<SortDef const> sortModes() noexcept
{
    return kSortModes;
}

std::size_t nextSortMode(std::size_t current, Action action) noexcept
{
    auto const count = kSortModes.size();
    if (current >= count)
        current = 0;

    // Re-selecting the current mode's action cycles to the next row with that action.
    if (kSortModes[current].triggerAction == action)
    {
        for (auto offset = std::size_t { 1 }; offset < count; ++offset)
        {
            auto const idx = (current + offset) % count;
            if (kSortModes[idx].triggerAction == action)
                return idx;
        }
        return current; // only one row uses this action
    }

    // Otherwise jump to the first row matching the action.
    for (auto idx = std::size_t { 0 }; idx < count; ++idx)
        if (kSortModes[idx].triggerAction == action)
            return idx;

    return current; // action not a sort trigger
}

} // namespace tuidu
