// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file Scanner.hpp
/// @brief Coroutine-first recursive directory walker that aggregates disk usage.

#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_set>

#include <coro/Task.hpp>
#include <platform/FileInfoProvider.hpp>
#include <tuidu/ScanProgress.hpp>
#include <tuidu/Tree.hpp>

namespace tuidu
{

/// Recursively walks a directory tree over an injected @c FileInfoProvider, building
/// a @ref Tree and aggregating subtree sizes/blocks/item-counts.
///
/// Dependency-injected: it touches the filesystem only through the abstract
/// @c endo::platform::FileInfoProvider, so production injects a real provider and tests
/// inject a mock. The walk is a coroutine (@c endo::coro::Task) — `scan` `co_await`s a
/// recursive `scanDir` per subdirectory and bubbles aggregates up on return.
/// Cancellation is cooperative: the inherited @c StopToken is checked between entries,
/// and a requested stop throws @c endo::coro::OperationCancelled to unwind cleanly.
class Scanner
{
  public:
    /// Sink invoked with periodic progress and the final (done) message.
    using ProgressSink = std::function<void(ScanProgress const&)>;

    /// @param provider The directory-listing seam (not owned; outlives the scanner).
    /// @param tree The tree to populate (not owned; the root must already exist).
    /// @param options Scan policy (cross-device, symlinks, dedup, progress cadence).
    /// @param treeMutex Optional mutex guarding @p tree against concurrent UI reads. When
    ///        set, the scanner locks it around each tree mutation so a UI thread holding
    ///        the same mutex sees a consistent tree. Null for single-threaded use.
    Scanner(endo::platform::FileInfoProvider const& provider,
            Tree& tree,
            ScanOptions options,
            std::mutex* treeMutex = nullptr) noexcept;

    /// Scans the subtree rooted at @p dirId, populating the tree and emitting progress.
    /// @param dirId The directory node to scan (typically @c tree.root()).
    /// @param sink Receives periodic progress and a final done message.
    /// @return A task that completes when the subtree is fully scanned (or cancelled).
    [[nodiscard]] endo::coro::Task<void> scan(NodeId dirId, ProgressSink sink);

  private:
    /// Recursively scans @p dirId, populating children and folding their aggregates up.
    /// @param dirId The directory node to scan.
    /// @param sink Progress sink (taken by value — coroutine parameters must not be refs).
    [[nodiscard]] endo::coro::Task<void> scanDir(NodeId dirId, ProgressSink sink);

    /// Emits a progress message if the cadence threshold has been reached, or if forced.
    void maybeEmit(ProgressSink const& sink, NodeId node, bool force, bool done);

    /// @return A lock on the tree mutex (held while in scope), or a disengaged lock when
    ///         no mutex was injected (single-threaded use).
    [[nodiscard]] std::unique_lock<std::mutex> lockTree()
    {
        return _treeMutex ? std::unique_lock<std::mutex> { *_treeMutex } : std::unique_lock<std::mutex> {};
    }

    endo::platform::FileInfoProvider const& _provider; ///< Directory-listing seam.
    Tree& _tree;                                       ///< Tree being populated.
    ScanOptions _options;                              ///< Scan policy.
    std::mutex* _treeMutex;                            ///< Optional guard for @c _tree (may be null).
    std::unordered_set<std::uint64_t> _seenInodes;     ///< (dev,ino) keys for hardlink dedup.
    std::uint64_t _rootDev = 0;                        ///< Device of the scan root (cross-device check).
    std::uint64_t _scannedItems = 0;                   ///< Running scanned-item total.
    std::int64_t _scannedBytes = 0;                    ///< Running scanned-byte total.
    std::uint32_t _sinceEmit = 0;                      ///< Entries scanned since the last emit.
};

} // namespace tuidu
