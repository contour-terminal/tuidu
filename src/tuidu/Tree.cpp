// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <ranges>
#include <utility>

#include <tuidu/Tree.hpp>

namespace tuidu
{

std::pair<std::uint32_t, std::uint16_t> Tree::internName(std::string_view name)
{
    auto const offset = static_cast<std::uint32_t>(_names.size());
    _names.append(name);
    return { offset, static_cast<std::uint16_t>(name.size()) };
}

NodeId Tree::createRoot(std::string_view path)
{
    _nodes.clear();
    _names.clear();
    _childIndex.clear();
    _childIndexOffset.clear();
    _childIndexCount.clear();

    auto node = Node {};
    std::tie(node.nameOffset, node.nameLen) = internName(path);
    node.itemCount = 1; // the root counts itself; descendants add to it via propagateUp
    _nodes.push_back(node);
    _childIndexOffset.push_back(0);
    _childIndexCount.push_back(0);
    _root = 0;
    return _root;
}

NodeId Tree::addChild(NodeId parent, std::string_view name)
{
    auto node = Node {};
    node.parent = parent;
    std::tie(node.nameOffset, node.nameLen) = internName(name);

    auto const id = static_cast<NodeId>(_nodes.size());
    _nodes.push_back(node);
    _childIndexOffset.push_back(0);
    _childIndexCount.push_back(0);

    // Maintain the contiguous [firstChild, firstChild + childCount) range. The scanner
    // appends all of a directory's children consecutively, so this stays contiguous.
    auto& parentNode = _nodes[parent];
    if (parentNode.firstChild == InvalidNode)
        parentNode.firstChild = id;
    ++parentNode.childCount;
    return id;
}

std::string_view Tree::name(NodeId id) const noexcept
{
    auto const& node = _nodes[id];
    return std::string_view { _names }.substr(node.nameOffset, node.nameLen);
}

std::string Tree::fullPath(NodeId id) const
{
    // Walk to the root collecting names, then join. The root's name is the full base
    // path; descendants contribute their filename component.
    std::vector<NodeId> chain;
    for (auto cur = id; cur != InvalidNode; cur = _nodes[cur].parent)
        chain.push_back(cur);

    std::string path;
    for (auto const nodeId: std::ranges::reverse_view(chain))
    {
        auto const part = name(nodeId);
        if (path.empty())
        {
            path = std::string { part };
        }
        else
        {
            if (!path.empty() && path.back() != '/')
                path.push_back('/');
            path.append(part);
        }
    }
    return path;
}

std::span<NodeId const> Tree::childrenOf(NodeId parent) const noexcept
{
    auto const offset = _childIndexOffset[parent];
    auto const count = _childIndexCount[parent];
    if (count == 0)
        return {};
    return std::span<NodeId const> { _childIndex }.subspan(offset, count);
}

void Tree::rebuildChildIndex(NodeId parent, NodeLess const& less)
{
    auto const& parentNode = _nodes[parent];
    auto const count = parentNode.childCount;

    if (count == 0)
    {
        _childIndexCount[parent] = 0;
        return;
    }

    // Children occupy the contiguous arena range [firstChild, firstChild + childCount).
    // Deleted children are skipped so they vanish from the parent's view without disturbing
    // the contiguous arena range or invalidating any NodeId.
    auto ids = std::vector<NodeId> {};
    ids.reserve(count);
    for (auto i = std::uint32_t { 0 }; i < count; ++i)
    {
        auto const child = static_cast<NodeId>(parentNode.firstChild + i);
        if (!_nodes[child].isDeleted())
            ids.push_back(child);
    }

    std::ranges::sort(ids, [&](NodeId a, NodeId b) { return less(*this, a, b); });

    // Append the freshly-sorted slice and point the parent at it. We append rather than
    // overwrite in place so a parent re-sorted after others keeps a valid contiguous slice.
    auto const offset = static_cast<std::uint32_t>(_childIndex.size());
    _childIndex.insert(_childIndex.end(), ids.begin(), ids.end());
    _childIndexOffset[parent] = offset;
    _childIndexCount[parent] = static_cast<std::uint32_t>(ids.size());
}

void Tree::propagateUp(NodeId id, std::int64_t size, std::int64_t blocks, std::uint64_t items)
{
    for (auto cur = id; cur != InvalidNode; cur = _nodes[cur].parent)
    {
        auto& node = _nodes[cur];
        node.aggSize += size;
        node.aggBlocks += blocks;
        node.itemCount += items;
    }
}

void Tree::removeSubtree(NodeId id)
{
    if (id == InvalidNode || id == _root)
        return;

    auto& node = _nodes[id];
    if (node.isDeleted())
        return; // already removed; do not subtract its totals twice

    // Capture the subtree totals before flagging, then subtract them from every ancestor so the
    // parent chain's aggregates stay correct. The deleted node keeps its own (now irrelevant)
    // aggregates; only ancestors are adjusted. Each subtraction is exact because every ancestor's
    // aggregate includes this subtree's contribution.
    auto const size = node.aggSize;
    auto const blocks = node.aggBlocks;
    auto const items = node.itemCount;
    node.flags |= NodeFlag::Deleted;
    for (auto cur = node.parent; cur != InvalidNode; cur = _nodes[cur].parent)
    {
        auto& ancestor = _nodes[cur];
        ancestor.aggSize -= size;
        ancestor.aggBlocks -= blocks;
        ancestor.itemCount -= items;
    }
}

} // namespace tuidu
