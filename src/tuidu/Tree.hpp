// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file Tree.hpp
/// @brief Flat-arena disk-usage tree: owns nodes, names, and per-directory sort order.

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <tuidu/Node.hpp>

namespace tuidu
{

class Tree;

/// Comparator over two nodes of the same parent, used to order children.
/// @return true if @p a should sort before @p b.
using NodeLess = std::function<bool(Tree const&, NodeId a, NodeId b)>;

/// Owns the flat node arena, a shared name arena, and per-directory sorted child
/// index lists. Pure data — no OS dependency — so it is fully unit-testable.
///
/// Re-sorting a directory rewrites only its slice of the child-index vector; the
/// node arena itself is never reordered, so @ref NodeId handles stay valid across
/// sorts. Appending nodes may relocate the arena, so callers must hold @ref NodeId
/// (indices), never @c Node& references, across mutations.
class Tree
{
  public:
    Tree() = default;

    /// Creates the root node for @p path and returns its id.
    /// @param path Absolute path the scan starts from (stored as the root's name).
    /// @return The root node id (also retrievable via @ref root).
    NodeId createRoot(std::string_view path);

    /// Appends a child node under @p parent.
    /// @param parent The parent node id (must already exist).
    /// @param name The entry's display name (filename component).
    /// @return The new node's id.
    NodeId addChild(NodeId parent, std::string_view name);

    /// @return The root node id, or @ref InvalidNode if the tree is empty.
    [[nodiscard]] NodeId root() const noexcept { return _root; }

    /// @return The number of nodes currently in the arena.
    [[nodiscard]] std::size_t size() const noexcept { return _nodes.size(); }

    /// @return true if the tree has no nodes.
    [[nodiscard]] bool empty() const noexcept { return _nodes.empty(); }

    /// @return Read-only access to the node with id @p id.
    [[nodiscard]] Node const& operator[](NodeId id) const noexcept { return _nodes[id]; }

    /// @return Mutable access to the node with id @p id.
    [[nodiscard]] Node& at(NodeId id) noexcept { return _nodes[id]; }

    /// @return The display name of node @p id.
    [[nodiscard]] std::string_view name(NodeId id) const noexcept;

    /// @return The full path of node @p id, built by walking to the root.
    [[nodiscard]] std::string fullPath(NodeId id) const;

    /// @return The children of @p parent in the currently-applied sort order.
    ///         Empty if @p parent has no children or has not been sorted yet.
    [[nodiscard]] std::span<NodeId const> childrenOf(NodeId parent) const noexcept;

    /// (Re)builds the sorted child-index slice for @p parent using @p less.
    /// Cheap: touches only the index vector, never the node arena.
    /// @param parent The directory whose children to order.
    /// @param less Comparator establishing the order.
    void rebuildChildIndex(NodeId parent, NodeLess const& less);

    /// Adds @p size / @p blocks / @p items to node @p id **and every ancestor** (inclusive
    /// of @p id). The scanner calls this once per scanned entry with that entry's own size
    /// and an item count of 1, so each entry contributes itself to its own aggregate and to
    /// every ancestor's — directories never re-add their subtree totals (which would
    /// double-count).
    /// @param id The entry whose contribution to roll upward (this node included).
    /// @param size Apparent bytes to add to the aggregate.
    /// @param blocks Disk-usage bytes to add to the aggregate.
    /// @param items Item count to add to the aggregate.
    void propagateUp(NodeId id, std::int64_t size, std::int64_t blocks, std::uint64_t items);

  private:
    /// Interns @p name into the string arena and returns its (offset, length).
    [[nodiscard]] std::pair<std::uint32_t, std::uint16_t> internName(std::string_view name);

    std::vector<Node> _nodes;                     ///< The flat node arena.
    std::string _names;                           ///< Flat name arena; nodes slice into it.
    std::vector<NodeId> _childIndex;              ///< Concatenated per-directory sorted child id lists.
    std::vector<std::uint32_t> _childIndexOffset; ///< Per-node start offset into _childIndex.
    std::vector<std::uint32_t> _childIndexCount;  ///< Per-node child count in _childIndex.
    NodeId _root = InvalidNode;                   ///< The root node id.
};

} // namespace tuidu
