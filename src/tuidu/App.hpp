// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file App.hpp
/// @brief The tuidu application: wires the scanner, model, view, and coroutine loop.

#include <tui/Screen.hpp>
#include <tui/StatusBar.hpp>
#include <tui/Terminal.hpp>
#include <tui/TreeTableView.hpp>
#include <tui/runtime/EventSource.hpp>
#include <tui/runtime/TuiRuntime.hpp>

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>

#include <coro/Task.hpp>
#include <platform/FileInfoProvider.hpp>
#include <platform/FileSystem.hpp>
#include <platform/MessageQueue.hpp>
#include <platform/Wakeup.hpp>
#include <tuidu/ChordRecognizer.hpp>
#include <tuidu/DeleteProgress.hpp>
#include <tuidu/DeleteProgressDialog.hpp>
#include <tuidu/DeleteWorker.hpp>
#include <tuidu/DiskUsageModel.hpp>
#include <tuidu/HelpOverlay.hpp>
#include <tuidu/Keymap.hpp>
#include <tuidu/ScanProgress.hpp>
#include <tuidu/ThemeController.hpp>
#include <tuidu/Tree.hpp>

namespace tuidu
{

/// Configuration for an @ref App run (data, injected at construction).
struct AppConfig
{
    std::string rootPath = ".";             ///< Directory to scan.
    ThemeMode themeMode = ThemeMode::Auto;  ///< Initial theme mode.
    ScanOptions scanOptions {};             ///< Scan policy.
    SizeMode sizeMode = SizeMode::Apparent; ///< Initial size metric.
    UnitSystem units = UnitSystem::Binary;  ///< Initial unit formatting.
    std::size_t sortMode = 0;               ///< Initial sort mode (index into @ref sortModes; 0 = size ↓).
    double largeThreshold = 0.20;           ///< Parent-share at/above which a row colors as "large".
    double hugeThreshold = 0.50;            ///< Parent-share at/above which a row colors as "huge".
};

/// The tuidu application. Owns the model/view/scanner and runs the main coroutine loop
/// over an injected @c Terminal and @c EventSource — so production wires a real terminal
/// and a @c TerminalEventSource while tests inject a @c MockTerminalOutput and a scripted
/// @c MockEventSource. Every OS touch is behind an injected interface.
class App
{
  public:
    /// @param terminal The terminal (real or mock-backed; not owned, must outlive the App).
    /// @param eventSource The runtime's wait source (real or scripted; not owned).
    /// @param provider The directory-listing seam (not owned).
    /// @param fileSystem The filesystem seam used to delete items (not owned).
    /// @param progress The scan worker → UI channel (not owned; its wakeup must back @p eventSource).
    /// @param deleteProgress The delete worker → UI channel (not owned; its wakeup must back
    ///        @p eventSource, so a delete-progress push wakes the UI loop).
    /// @param config Run configuration.
    App(tui::Terminal& terminal,
        tui::runtime::EventSource& eventSource,
        endo::platform::FileInfoProvider const& provider,
        endo::platform::FileSystem const& fileSystem,
        endo::platform::MessageQueue<ScanProgress>& progress,
        endo::platform::MessageQueue<DeleteProgress>& deleteProgress,
        AppConfig config);

    /// Runs the application to completion (until the user quits).
    /// @return The process exit code.
    int run();

    /// @return The disk-usage tree (for inspection in tests).
    [[nodiscard]] Tree const& tree() const noexcept { return _tree; }

    /// @return The browser model (for inspection in tests).
    [[nodiscard]] DiskUsageModel const& model() const noexcept { return _model; }

    /// @return true while a background scan is still running (for test synchronization).
    [[nodiscard]] bool scanInFlight() const noexcept { return _scanInFlight; }

    /// @return true while the help overlay is shown (for inspection in tests).
    [[nodiscard]] bool helpVisible() const noexcept { return _helpVisible; }

    /// @return true while a delete is running (for test synchronization).
    [[nodiscard]] bool deleteInFlight() const noexcept { return _deleteInFlight; }

    /// @return The error that aborted the scan, if any (empty when the scan succeeded or is
    ///         still running). Surfaced in the status bar and printed on exit.
    [[nodiscard]] std::optional<std::string> const& scanError() const noexcept { return _scanError; }

  private:
    /// The root coroutine flow: drain progress, draw, await activity, dispatch keys.
    [[nodiscard]] endo::coro::Task<void> mainFlow();

    /// Applies a resolved @ref Action to the model/view/app state.
    /// @return false if the app should quit.
    bool dispatch(Action action);

    /// Folds queued scan-progress messages into the status bar and view.
    void applyProgress();

    /// Updates the status bar text from the current tree/scan state.
    void refreshStatus();

    /// Shows or hides the centered help overlay.
    /// @param visible Whether the help overlay should be shown.
    void setHelpVisible(bool visible);

    /// Shows or hides the centered delete-progress dialog.
    /// @param visible Whether the dialog should be shown.
    void setDeleteDialogVisible(bool visible);

    /// Starts deleting the currently-selected item (the `dd` chord target), spawning a
    /// @ref DeleteWorker and showing the progress dialog. No-op if nothing is selected, the
    /// selection is the root, or a delete is already running.
    void beginDelete();

    /// Folds queued delete-progress messages into the dialog; on completion, removes the node
    /// from the tree (unless the delete was cancelled or failed) and restores the browser.
    void drainDeleteProgress();

    /// Copies the currently-selected item's full path to the system clipboard (the `yy` chord),
    /// via the terminal's OSC 52 clipboard. No-op if nothing is selected.
    void yankSelectedPath();

    tui::Terminal& _terminal;                              ///< Terminal (injected).
    tui::runtime::EventSource& _eventSource;               ///< Wait source (injected).
    endo::platform::FileInfoProvider const& _provider;     ///< Directory-listing seam (injected).
    endo::platform::FileSystem const& _fileSystem;         ///< Filesystem seam for deletion (injected).
    endo::platform::MessageQueue<ScanProgress>& _progress; ///< Scan worker → UI channel (injected).
    endo::platform::MessageQueue<DeleteProgress>& _deleteProgress; ///< Delete worker → UI channel (injected).
    AppConfig _config;                                             ///< Run configuration.

    Tree _tree;                         ///< The disk-usage tree.
    DiskUsageModel _model;              ///< Adapts the tree to the generic view.
    ThemeController _theme;             ///< Dark/light theme controller.
    Keymap _keymap;                     ///< Key → Action bindings.
    ChordRecognizer _chords;            ///< Multi-key chord (e.g. `dd`) recognizer.
    tui::Screen _screen;                ///< The render surface.
    tui::TreeTableView _browser;        ///< The generic browser component.
    tui::StatusBar _statusBar;          ///< The status line.
    HelpOverlay _help;                  ///< The help panel (shown on '?').
    DeleteProgressDialog _deleteDialog; ///< The delete-progress overlay (shown during `dd`).
    tui::runtime::TuiRuntime _runtime;  ///< The coroutine scheduler.

    std::optional<DeleteWorker> _deleteWorker; ///< The active delete worker (empty when idle).

    std::mutex _treeMutex;                 ///< Guards _tree between the scan worker and the UI.
    bool _scanInFlight = false;            ///< True while a scan is running.
    bool _helpVisible = false;             ///< Whether the help overlay is shown.
    bool _deleteInFlight = false;          ///< True while a delete is running.
    NodeId _deletingNode = InvalidNode;    ///< The node being deleted (removed from the tree on success).
    bool _dirty = true;                    ///< Whether the screen needs a redraw (starts true).
    std::uint64_t _scannedItems = 0;       ///< Latest scanned-item total (status).
    std::int64_t _scannedBytes = 0;        ///< Latest scanned-byte total (status).
    std::optional<std::string> _scanError; ///< Error that aborted the scan, if any (shown stickily).

    static constexpr int ScanPollMs = 60;   ///< Poll interval while a scan streams progress.
    static constexpr int IdlePollMs = 1000; ///< Idle poll interval (input/scan wakeups preempt it).
};

} // namespace tuidu
