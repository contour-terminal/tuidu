// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file DeleteWorker.hpp
/// @brief Background thread that recursively deletes a path and feeds progress to the UI.

#include <cstdint>
#include <filesystem>
#include <thread>

#include <platform/FileSystem.hpp>
#include <platform/MessageQueue.hpp>
#include <tuidu/DeleteProgress.hpp>

namespace tuidu
{

/// How a target should be removed by @ref DeleteWorker.
enum class DeleteMode : std::uint8_t
{
    SingleEntry, ///< Remove the target as one entry (a file, or a symlink — unlinked, not followed).
    Recursive,   ///< Recurse into the target directory, deleting its contents deepest-first.
};

/// Recursively deletes a filesystem path on a dedicated @c std::jthread so the blocking removal
/// never stalls the UI thread.
///
/// Deletion goes through an injected @ref endo::platform::FileSystem (so it is unit-testable with
/// @c InMemoryFileSystem) and reports progress through an injected
/// @c MessageQueue<DeleteProgress> whose wakeup the UI's event source selects on — a push wakes
/// the UI to update the progress dialog. Cancellation is cooperative via the jthread's stop token:
/// @ref requestStop (or destruction) aborts the delete between entries, leaving the remainder on
/// disk and emitting a final @ref DeleteProgress with @c cancelled set.
class DeleteWorker
{
  public:
    /// @param fs The filesystem seam used for removal (not owned; outlives the worker).
    /// @param progress The channel progress is pushed to (not owned).
    /// @param progressEvery Emit a progress message every N removed entries.
    DeleteWorker(endo::platform::FileSystem const& fs,
                 endo::platform::MessageQueue<DeleteProgress>& progress,
                 std::uint32_t progressEvery = 64) noexcept;

    DeleteWorker(DeleteWorker const&) = delete;
    DeleteWorker& operator=(DeleteWorker const&) = delete;
    DeleteWorker(DeleteWorker&&) = delete;
    DeleteWorker& operator=(DeleteWorker&&) = delete;

    /// Requests cancellation and joins the worker thread.
    ~DeleteWorker();

    /// Starts the background deletion of @p target. Call at most once per worker.
    /// @param target The absolute or relative path to remove (file or directory tree).
    /// @param total The expected number of entries to remove (the subtree item count), used as
    ///        the progress-bar denominator.
    /// @param mode Whether to recurse into @p target as a directory or remove it as a single
    ///        entry. The caller derives this from the node so a symlink-to-directory is never
    ///        traversed (see @ref DeleteMode).
    void start(std::filesystem::path target, std::uint64_t total, DeleteMode mode);

    /// Requests the running deletion stop as soon as it next checks its token.
    void requestStop() noexcept;

    /// @return true while the worker thread is running.
    [[nodiscard]] bool running() const noexcept { return _thread.joinable(); }

  private:
    /// Body of the worker thread: enumerate the subtree, remove it deepest-first, report progress.
    void run(std::stop_token const& stop,
             std::filesystem::path const& target,
             std::uint64_t total,
             DeleteMode mode);

    endo::platform::FileSystem const& _fs;                   ///< Filesystem seam (removal).
    endo::platform::MessageQueue<DeleteProgress>& _progress; ///< Worker → UI channel.
    std::uint32_t _progressEvery;                            ///< Progress emission cadence.
    std::jthread _thread;                                    ///< The worker thread (auto-joins).
};

} // namespace tuidu
