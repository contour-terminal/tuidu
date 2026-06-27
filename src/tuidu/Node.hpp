// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file Node.hpp
/// @brief Pure-data node type for the disk-usage tree.
///
/// Nodes live in a flat arena (see @ref tuidu::Tree). A @ref NodeId is a 32-bit
/// index into that arena rather than a pointer, so re-sorting and growth never
/// invalidate the tree topology. This file has no OS dependency and is fully
/// unit-testable in isolation.

#include <cstdint>
#include <type_traits>

namespace tuidu
{

/// Index handle into the node arena. Cheaper and more stable than a pointer.
using NodeId = std::uint32_t;

/// Sentinel meaning "no node" (e.g. the root's parent, or an unscanned child range).
inline constexpr NodeId InvalidNode = ~NodeId { 0 };

/// Per-node state flags. A bitset rather than a bag of bools so flag handling is
/// data-driven (one mask/test path) instead of one branch per concept.
enum class NodeFlag : std::uint8_t
{
    None = 0,
    IsDir = 1 << 0,       ///< Entry is a directory.
    IsSymlink = 1 << 1,   ///< Entry is a symbolic link (not followed during scan).
    ReadError = 1 << 2,   ///< listDirectory failed here; the subtree aggregate is partial.
    OtherDevice = 1 << 3, ///< Child crossed a filesystem boundary; excluded from the aggregate.
    HardlinkDup = 1 << 4, ///< (dev,ino) already counted; size deduplicated to 0 in the aggregate.
    Excluded = 1 << 5,    ///< Matched an exclude pattern.
    Deleted = 1 << 6,     ///< Removed from disk; excluded from its parent's child index and aggregates.
};

/// Bitwise OR of two flag sets.
[[nodiscard]] constexpr NodeFlag operator|(NodeFlag lhs, NodeFlag rhs) noexcept
{
    using U = std::underlying_type_t<NodeFlag>;
    return static_cast<NodeFlag>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

/// Bitwise AND of two flag sets.
[[nodiscard]] constexpr NodeFlag operator&(NodeFlag lhs, NodeFlag rhs) noexcept
{
    using U = std::underlying_type_t<NodeFlag>;
    return static_cast<NodeFlag>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

/// Bitwise NOT of a flag set.
[[nodiscard]] constexpr NodeFlag operator~(NodeFlag value) noexcept
{
    using U = std::underlying_type_t<NodeFlag>;
    return static_cast<NodeFlag>(~static_cast<U>(value));
}

/// In-place OR-assign.
constexpr NodeFlag& operator|=(NodeFlag& lhs, NodeFlag rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

/// In-place AND-assign.
constexpr NodeFlag& operator&=(NodeFlag& lhs, NodeFlag rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

/// @return true if @p value has every bit of @p test set.
[[nodiscard]] constexpr bool hasFlag(NodeFlag value, NodeFlag test) noexcept
{
    return (value & test) == test;
}

/// One filesystem entry in the arena. Trivially relocatable, ~64 bytes.
///
/// `ownSize`/`ownBlocks` describe this entry alone; `aggSize`/`aggBlocks`/`itemCount`
/// describe its whole subtree and are filled in as the scan completes. Names are not
/// stored here — they live in the @ref Tree string arena, referenced by
/// (`nameOffset`, `nameLen`) — to avoid a per-node heap allocation at ncdu scale.
struct Node
{
    NodeId parent = InvalidNode;     ///< Parent index, or InvalidNode for the root.
    NodeId firstChild = InvalidNode; ///< Index of first child, or InvalidNode if leaf/unscanned.
    std::uint32_t childCount = 0;    ///< Children occupy [firstChild, firstChild + childCount).

    std::int64_t ownSize = 0;    ///< Apparent size of this entry alone (st_size).
    std::int64_t ownBlocks = 0;  ///< Disk usage of this entry alone (st_blocks * 512).
    std::int64_t aggSize = 0;    ///< Subtree apparent size (own + descendants); filled by aggregation.
    std::int64_t aggBlocks = 0;  ///< Subtree disk usage (own + descendants); filled by aggregation.
    std::uint64_t itemCount = 0; ///< Files + dirs in the subtree, counting self; filled by aggregation.

    std::int64_t mtime = 0; ///< Last modification time, epoch seconds.
    std::int64_t mode = 0;  ///< Permission bits.
    std::uint64_t dev = 0;  ///< Device id (st_dev) for cross-filesystem detection.

    NodeFlag flags = NodeFlag::None; ///< State flags (see @ref NodeFlag).
    std::uint32_t nameOffset = 0;    ///< Offset of the name within the Tree string arena.
    std::uint16_t nameLen = 0;       ///< Length of the name in bytes.

    /// @return true if this node is a directory.
    [[nodiscard]] constexpr bool isDir() const noexcept { return hasFlag(flags, NodeFlag::IsDir); }

    /// @return true if this node is a symbolic link.
    [[nodiscard]] constexpr bool isSymlink() const noexcept { return hasFlag(flags, NodeFlag::IsSymlink); }

    /// @return true if this node's subtree could not be fully read.
    [[nodiscard]] constexpr bool hasReadError() const noexcept { return hasFlag(flags, NodeFlag::ReadError); }

    /// @return true if this node has been removed from disk (and from its parent's view).
    [[nodiscard]] constexpr bool isDeleted() const noexcept { return hasFlag(flags, NodeFlag::Deleted); }
};

} // namespace tuidu
