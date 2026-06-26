// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file DeleteProgress.hpp
/// @brief The delete-worker → UI progress message.

#include <cstdint>
#include <optional>
#include <string>

namespace tuidu
{

/// One progress update pushed from the @ref DeleteWorker to the UI thread.
///
/// The worker pushes these periodically while removing a subtree from disk, plus a final message
/// with @ref done set. @ref cancelled distinguishes a user-aborted delete (subtree left partially
/// removed) from a completed one; @ref error carries the first filesystem failure, if any.
struct DeleteProgress
{
    std::uint64_t deleted = 0;        ///< Entries removed so far.
    std::uint64_t total = 0;          ///< Total entries to remove (the subtree's item count).
    std::string currentPath;          ///< The path most recently acted upon (for the dialog).
    bool done = false;                ///< True on the final message, however the delete ended.
    bool cancelled = false;           ///< True if the delete was aborted before completing.
    std::optional<std::string> error; ///< The first filesystem error, if the delete failed.
};

} // namespace tuidu
