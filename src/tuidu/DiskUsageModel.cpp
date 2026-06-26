// SPDX-License-Identifier: Apache-2.0
#include <tui/Theme.hpp>

#include <tuidu/ColorRules.hpp>
#include <tuidu/DiskUsageModel.hpp>
#include <tuidu/SortMode.hpp>

namespace tuidu
{

namespace
{
    /// Maps a generic TableColumn from a tuidu ColumnDef (layout-only translation).
    [[nodiscard]] tui::TableColumn toTableColumn(ColumnDef const& col)
    {
        auto const width =
            (col.widthPolicy == WidthPolicy::Flex) ? tui::ColumnWidth::Flex : tui::ColumnWidth::Fixed;
        auto const align = (col.align == Align::Right) ? tui::ColumnAlign::Right : tui::ColumnAlign::Left;
        return tui::TableColumn { .header = col.header, .width = width, .size = col.width, .align = align };
    }

    // Thresholds for color rules (data, not hardcoded in predicates).
    constexpr double kLargeThreshold = 0.20;
    constexpr double kHugeThreshold = 0.50;
} // namespace

DiskUsageModel::DiskUsageModel(Tree& tree) noexcept: _tree(tree), _currentDir(tree.root())
{
    resort();
}

RenderCtx DiskUsageModel::renderCtx(NodeId node) const
{
    return RenderCtx { .tree = _tree,
                       .node = node,
                       .parent = _tree[node].parent,
                       .sizeMode = _sizeMode,
                       .units = _units,
                       .barWidth = 0 };
}

std::vector<tui::TableColumn> DiskUsageModel::columns() const
{
    std::vector<tui::TableColumn> out;
    for (auto const& col: tuidu::columns())
        out.push_back(toTableColumn(col));
    return out;
}

std::vector<tui::RowId> DiskUsageModel::rows() const
{
    std::vector<tui::RowId> out;
    for (auto const id: _tree.childrenOf(_currentDir))
        out.push_back(static_cast<tui::RowId>(id));
    return out;
}

std::string DiskUsageModel::cellText(tui::RowId row, std::size_t column) const
{
    auto const cols = tuidu::columns();
    if (column >= cols.size())
        return {};
    return cols[column].format(renderCtx(static_cast<NodeId>(row)));
}

tui::RowStyle DiskUsageModel::rowStyle(tui::RowId row, bool selected) const
{
    auto const node = static_cast<NodeId>(row);
    auto const parent = _tree[node].parent;
    auto const parentSize = (parent != InvalidNode) ? _tree[parent].aggSize : _tree[node].aggSize;
    auto const fraction =
        (parentSize > 0) ? static_cast<double>(_tree[node].aggSize) / static_cast<double>(parentSize) : 0.0;

    auto const ctx = ColorContext { .tree = _tree,
                                    .node = node,
                                    .percentOfParent = fraction,
                                    .largeThreshold = kLargeThreshold,
                                    .hugeThreshold = kHugeThreshold };
    auto const resolved = resolveColor(ctx, tui::currentTheme().colors);

    auto style = tui::Style {};
    style.fg = resolved.color;
    style.bold = resolved.bold;
    return tui::RowStyle { .style = style, .selected = selected };
}

bool DiskUsageModel::canDescend(tui::RowId row) const
{
    auto const node = static_cast<NodeId>(row);
    return _tree[node].isDir() && _tree[node].childCount > 0;
}

bool DiskUsageModel::descend(tui::RowId row)
{
    auto const node = static_cast<NodeId>(row);
    if (!canDescend(row))
        return false;
    _currentDir = node;
    resort();
    return true;
}

bool DiskUsageModel::ascend()
{
    auto const parent = _tree[_currentDir].parent;
    if (parent == InvalidNode)
        return false;
    _currentDir = parent;
    resort();
    return true;
}

std::string DiskUsageModel::currentTitle() const
{
    return _tree.fullPath(_currentDir);
}

void DiskUsageModel::sortBy(int sortKey)
{
    _sortMode = nextSortMode(_sortMode, static_cast<Action>(sortKey));
    resort();
}

void DiskUsageModel::setSizeMode(SizeMode mode)
{
    _sizeMode = mode;
}

void DiskUsageModel::resetToRoot()
{
    _currentDir = _tree.root();
    resort();
}

void DiskUsageModel::resort()
{
    if (_tree.empty() || _currentDir == InvalidNode)
        return;
    _tree.rebuildChildIndex(_currentDir, [this](Tree const& t, NodeId a, NodeId b) {
        return sortModes()[_sortMode].less(t, a, b);
    });
}

} // namespace tuidu
