// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include <tuidu/ColorRules.hpp>

namespace tuidu
{

namespace
{
    /// A subtree that could not be fully read (permission/IO error).
    [[nodiscard]] bool isReadError(ColorContext const& ctx)
    {
        return ctx.tree[ctx.node].hasReadError();
    }

    /// Occupies a "huge" share of its parent (data-driven threshold).
    [[nodiscard]] bool isHugeShare(ColorContext const& ctx)
    {
        return ctx.percentOfParent >= ctx.hugeThreshold;
    }

    /// Occupies a "large" share of its parent (data-driven threshold).
    [[nodiscard]] bool isLargeShare(ColorContext const& ctx)
    {
        return ctx.percentOfParent >= ctx.largeThreshold;
    }

    /// A directory.
    [[nodiscard]] bool isDirectory(ColorContext const& ctx)
    {
        return ctx.tree[ctx.node].isDir();
    }

    /// Always matches (the default fallthrough rule).
    [[nodiscard]] bool always(ColorContext const&)
    {
        return true;
    }

    constexpr std::array<ColorRule, 5> ColorRules { {
        { .match = &isReadError, .slot = &tui::ColorPalette::error, .bold = false },
        { .match = &isHugeShare, .slot = &tui::ColorPalette::warning, .bold = true },
        { .match = &isLargeShare, .slot = &tui::ColorPalette::accent, .bold = false },
        { .match = &isDirectory, .slot = &tui::ColorPalette::primary, .bold = true },
        { .match = &always, .slot = &tui::ColorPalette::text, .bold = false },
    } };
} // namespace

std::span<ColorRule const> colorRules() noexcept
{
    return ColorRules;
}

ResolvedColor resolveColor(ColorContext const& ctx, tui::ColorPalette const& palette)
{
    for (auto const& rule: ColorRules)
        if (rule.match(ctx))
            return ResolvedColor { .color = palette.*(rule.slot), .bold = rule.bold };

    // ColorRules always ends with a catch-all, so this is unreachable; default to text.
    return ResolvedColor { .color = palette.text, .bold = false };
}

} // namespace tuidu
