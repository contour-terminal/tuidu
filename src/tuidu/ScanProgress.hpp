// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file ScanProgress.hpp
/// @brief The worker → UI progress message and the options that steer a scan.

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <tuidu/Node.hpp>

namespace tuidu
{

/// Options steering a scan. All policy lives here as data, not as branches in the walk.
struct ScanOptions
{
    bool crossDevices = false;         ///< false = stop at filesystem boundaries (du -x default).
    bool followSymlinks = false;       ///< false = count a symlink's own size, do not traverse it.
    bool dedupeHardlinks = true;       ///< true = count a (dev,ino) only once.
    std::uint32_t progressEvery = 256; ///< Emit a progress message every N entries scanned.
};

/// One progress update pushed from the scan worker to the UI thread.
struct ScanProgress
{
    NodeId node = InvalidNode;              ///< The directory whose subtree just completed (if any).
    std::uint64_t scannedItems = 0;         ///< Running total of entries scanned so far.
    std::int64_t scannedBytes = 0;          ///< Running total of apparent bytes scanned so far.
    std::optional<std::string> currentPath; ///< Path currently being scanned (for the status line).
    bool done = false;                      ///< True on the final message when the scan finishes.
};

} // namespace tuidu
