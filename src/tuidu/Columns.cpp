// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <format>

#include <tuidu/Columns.hpp>

namespace tuidu
{

std::int64_t RenderCtx::metric(NodeId id) const noexcept
{
    auto const& entry = tree[id];
    return sizeMode == SizeMode::Disk ? entry.aggBlocks : entry.aggSize;
}

std::string fmtSize(RenderCtx const& ctx)
{
    return formatSize(ctx.metric(ctx.node), ctx.units);
}

std::string fmtBar(RenderCtx const& ctx)
{
    auto const width = std::max(ctx.barWidth, 0);
    if (width == 0)
        return {};

    auto const parentSize = (ctx.parent != InvalidNode) ? ctx.metric(ctx.parent) : ctx.metric(ctx.node);
    auto const nodeSize = ctx.metric(ctx.node);

    auto filled = 0;
    if (parentSize > 0)
    {
        auto const ratio = static_cast<double>(nodeSize) / static_cast<double>(parentSize);
        filled = static_cast<int>(std::lround(ratio * static_cast<double>(width)));
        filled = std::clamp(filled, 0, width);
    }

    std::string bar;
    bar.reserve(static_cast<std::size_t>(width) * 3); // '#'/' ' are 1 byte; reserve generously
    for (auto i = 0; i < width; ++i)
        bar += (i < filled) ? '#' : ' ';
    return bar;
}

std::string fmtPercent(RenderCtx const& ctx)
{
    auto const parentSize = (ctx.parent != InvalidNode) ? ctx.metric(ctx.parent) : ctx.metric(ctx.node);
    if (parentSize <= 0)
        return "  0%";

    auto const pct = 100.0 * static_cast<double>(ctx.metric(ctx.node)) / static_cast<double>(parentSize);
    return std::format("{:3.0f}%", pct);
}

std::string fmtItems(RenderCtx const& ctx)
{
    return std::format("{}", ctx.tree[ctx.node].itemCount);
}

std::string fmtMtime(RenderCtx const& ctx)
{
    auto const epoch = ctx.tree[ctx.node].mtime;
    auto const time = static_cast<std::time_t>(epoch);
    std::tm tm {};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    return std::format(
        "{:04}-{:02}-{:02} {:02}:{:02}", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
}

std::string fmtName(RenderCtx const& ctx)
{
    auto const name = std::string { ctx.tree.name(ctx.node) };
    return ctx.tree[ctx.node].isDir() ? name + "/" : name;
}

namespace
{
    constexpr std::array<ColumnDef, 6> kColumns { {
        { .header = "Size",
          .widthPolicy = WidthPolicy::Fixed,
          .width = 10,
          .format = &fmtSize,
          .align = Align::Right },
        { .header = "",
          .widthPolicy = WidthPolicy::Fixed,
          .width = 12,
          .format = &fmtBar,
          .align = Align::Left },
        { .header = "%",
          .widthPolicy = WidthPolicy::Fixed,
          .width = 5,
          .format = &fmtPercent,
          .align = Align::Right },
        { .header = "Items",
          .widthPolicy = WidthPolicy::Fixed,
          .width = 8,
          .format = &fmtItems,
          .align = Align::Right },
        { .header = "Date",
          .widthPolicy = WidthPolicy::Fixed,
          .width = 17,
          .format = &fmtMtime,
          .align = Align::Left },
        { .header = "Name",
          .widthPolicy = WidthPolicy::Flex,
          .width = 10,
          .format = &fmtName,
          .align = Align::Left },
    } };
} // namespace

std::span<ColumnDef const> columns() noexcept
{
    return kColumns;
}

} // namespace tuidu
