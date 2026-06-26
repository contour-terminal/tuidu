// SPDX-License-Identifier: Apache-2.0
#include <string>
#include <utility>
#include <vector>

#include <coro/Cancellation.hpp>
#include <tuidu/Scanner.hpp>

namespace tuidu
{

namespace
{
    /// Combines a device id and inode into a single dedup key.
    [[nodiscard]] std::uint64_t identityKey(std::uint64_t dev, std::uint64_t ino) noexcept
    {
        // Mix the device into the high bits so distinct (dev,ino) pairs rarely collide.
        return (dev << 32) ^ (dev >> 32) ^ ino;
    }
} // namespace

Scanner::Scanner(endo::platform::FileInfoProvider const& provider,
                 Tree& tree,
                 ScanOptions options,
                 std::mutex* treeMutex) noexcept:
    _provider(provider), _tree(tree), _options(options), _treeMutex(treeMutex)
{
}

void Scanner::maybeEmit(ProgressSink const& sink, NodeId node, bool force, bool done)
{
    if (!sink)
        return;
    if (!force && _sinceEmit < _options.progressEvery)
        return;

    _sinceEmit = 0;
    std::string currentPath;
    {
        auto guard = lockTree();
        currentPath = _tree.fullPath(node);
    }
    auto progress = ScanProgress { .node = node,
                                   .scannedItems = _scannedItems,
                                   .scannedBytes = _scannedBytes,
                                   .currentPath = std::move(currentPath),
                                   .done = done };
    sink(progress);
}

endo::coro::Task<void> Scanner::scan(NodeId dirId, ProgressSink sink)
{
    {
        auto guard = lockTree();
        _rootDev = _tree[dirId].dev;
    }
    co_await scanDir(dirId, sink);
    maybeEmit(sink, dirId, /*force=*/true, /*done=*/true);
    co_return;
}

endo::coro::Task<void> Scanner::scanDir(NodeId dirId, ProgressSink sink)
{
    // Reading the directory is I/O — done without the tree lock so the UI thread is never
    // blocked on a slow readdir/lstat.
    std::string path;
    {
        auto guard = lockTree();
        path = _tree.fullPath(dirId);
    }
    auto const entries = _provider.listDirectory(path);

    // Cooperative cancellation: bail before mutating if a stop was requested.
    if (auto const token = co_await endo::coro::thisCoroStopToken(); token.stop_requested())
        throw endo::coro::OperationCancelled {};

    // Phase 1: add ALL of this directory's children first, contiguously, so the Tree's
    // [firstChild, firstChild + childCount) invariant holds. (Recursing per-entry would
    // interleave grandchildren into this range and corrupt it.) Collect the subdirectories
    // to descend into during phase 2.
    std::vector<NodeId> toTraverse;
    {
        auto guard = lockTree();
        for (auto const& entry: entries)
        {
            auto const childId = _tree.addChild(dirId, entry.name);
            auto& child = _tree.at(childId);
            child.mtime = entry.mtime;
            child.mode = entry.mode;
            child.dev = entry.dev;
            child.ownSize = entry.size;
            child.ownBlocks = (entry.blocks >= 0) ? entry.blocks * 512 : entry.size;
            if (entry.isDir)
                child.flags |= NodeFlag::IsDir;
            if (entry.isSymlink)
                child.flags |= NodeFlag::IsSymlink;

            ++_scannedItems;
            _scannedBytes += entry.size;
            ++_sinceEmit;

            // Cross-device boundary: do not recurse, exclude from the aggregate.
            if (!_options.crossDevices && entry.dev != 0 && _rootDev != 0 && entry.dev != _rootDev)
            {
                child.flags |= NodeFlag::OtherDevice;
                continue;
            }

            // Hardlink dedup: a second sighting of (dev,ino) contributes nothing to size.
            if (_options.dedupeHardlinks && entry.ino != 0)
            {
                auto const key = identityKey(entry.dev, entry.ino);
                if (!_seenInodes.insert(key).second)
                {
                    child.flags |= NodeFlag::HardlinkDup;
                    _tree.propagateUp(childId, 0, 0, 1);
                    continue;
                }
            }

            // Account for this entry itself (its own size and one item) across all ancestors.
            // Descendants of a traversed directory account for themselves with their own
            // propagateUp calls, so directories add only their own (typically zero) size.
            _tree.propagateUp(childId, child.ownSize, child.ownBlocks, 1);

            // Symlinks are not traversed by default: their own (link) size is all they add.
            if (entry.isDir && (!entry.isSymlink || _options.followSymlinks))
                toTraverse.push_back(childId);
        }
    }

    maybeEmit(sink, dirId, /*force=*/false, /*done=*/false);

    // Phase 2: recurse into the subdirectories (each appends its own contiguous range).
    for (auto const childId: toTraverse)
        co_await scanDir(childId, sink);

    co_return;
}

} // namespace tuidu
