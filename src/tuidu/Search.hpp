// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file Search.hpp
/// @brief Fuzzy, ranked name search over a node set (substring-first, then fuzzy score).

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <tuidu/Tree.hpp>

namespace tuidu
{

/// A node that matched a search query, with the data needed to rank it.
struct ScoredNode
{
    NodeId node;          ///< The matching node.
    bool substring;       ///< True if the query is a contiguous substring of the name.
    std::size_t position; ///< Byte offset of the (substring) match; npos for scattered matches.
    int score;            ///< Fuzzy score (higher is better); ties broken by name length/order.
};

/// Ranks @p candidates by how well their names match @p query.
///
/// Ordering policy (best first):
///  1. A contiguous-substring match outranks a scattered fuzzy match.
///  2. Among substring matches, an earlier match position wins, then a shorter name.
///  3. Among fuzzy matches, a higher fuzzy score wins, then a shorter name.
///  4. Final tie-break is name byte-order (stable, deterministic).
///
/// Matching is smart-case (a lowercase query matches case-insensitively; any uppercase
/// makes it case-sensitive). An empty @p query matches everything in @p candidates'
/// original order (callers use this to restore the unfiltered list).
///
/// @param tree The tree the nodes belong to (for name lookups).
/// @param candidates The node ids to search (typically a directory's children).
/// @param query The search text.
/// @return The matching nodes, ranked best-first.
[[nodiscard]] std::vector<ScoredNode> rankMatches(Tree const& tree,
                                                  std::span<NodeId const> candidates,
                                                  std::string_view query);

} // namespace tuidu
