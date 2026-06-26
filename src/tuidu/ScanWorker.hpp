// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file ScanWorker.hpp
/// @brief Background thread that drives a Scanner and feeds progress to the UI.

#include <mutex>
#include <string>
#include <thread>

#include <coro/Cancellation.hpp>
#include <platform/FileInfoProvider.hpp>
#include <platform/MessageQueue.hpp>
#include <tuidu/ScanProgress.hpp>
#include <tuidu/Tree.hpp>

namespace tuidu
{

/// Runs a @ref Scanner on a dedicated @c std::jthread so the synchronous, blocking
/// directory enumeration never stalls the UI thread.
///
/// Progress is delivered through an injected @c MessageQueue<ScanProgress> (whose
/// wakeup the UI's event source selects on), so a push wakes the UI to drain and
/// redraw. The worker writes the shared @ref Tree while it runs; the UI reads it
/// between drains. Cancellation is cooperative via a @c StopSource: @ref requestStop
/// (or destruction) trips the scan's @c StopToken, unwinding it promptly.
class ScanWorker
{
  public:
    /// @param provider The directory-listing seam (not owned; outlives the worker).
    /// @param tree The tree to populate (not owned; its root must already exist).
    /// @param progress The channel progress is pushed to (not owned).
    /// @param options Scan policy.
    /// @param treeMutex Mutex guarding @p tree; the worker locks it around each mutation so
    ///        a UI thread holding the same mutex reads a consistent tree (not owned).
    ScanWorker(endo::platform::FileInfoProvider const& provider,
               Tree& tree,
               endo::platform::MessageQueue<ScanProgress>& progress,
               ScanOptions options,
               std::mutex& treeMutex) noexcept;

    ScanWorker(ScanWorker const&) = delete;
    ScanWorker& operator=(ScanWorker const&) = delete;
    ScanWorker(ScanWorker&&) = delete;
    ScanWorker& operator=(ScanWorker&&) = delete;

    /// Requests cancellation and joins the worker thread.
    ~ScanWorker();

    /// Starts the background scan of @p rootId. Call at most once per worker.
    /// @param rootId The tree node to scan (typically @c tree.root()).
    void start(NodeId rootId);

    /// Requests the running scan stop as soon as it next checks its token.
    void requestStop() noexcept;

    /// @return true while the worker thread is running.
    [[nodiscard]] bool running() const noexcept { return _thread.joinable(); }

  private:
    endo::platform::FileInfoProvider const& _provider;     ///< Directory-listing seam.
    Tree& _tree;                                           ///< Tree being populated.
    endo::platform::MessageQueue<ScanProgress>& _progress; ///< Worker → UI channel.
    ScanOptions _options;                                  ///< Scan policy.
    std::mutex& _treeMutex;                                ///< Guards @c _tree against UI reads.
    endo::coro::StopSource _stop;                          ///< Cancels the scan.
    std::thread _thread;                                   ///< The worker thread.
};

} // namespace tuidu
