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

#include <mutex>
#include <string>

#include <coro/Task.hpp>
#include <platform/FileInfoProvider.hpp>
#include <platform/MessageQueue.hpp>
#include <platform/Wakeup.hpp>
#include <tuidu/DiskUsageModel.hpp>
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
    /// @param progress The worker → UI channel (not owned; its wakeup must back @p eventSource).
    /// @param config Run configuration.
    App(tui::Terminal& terminal,
        tui::runtime::EventSource& eventSource,
        endo::platform::FileInfoProvider const& provider,
        endo::platform::MessageQueue<ScanProgress>& progress,
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

    tui::Terminal& _terminal;                              ///< Terminal (injected).
    tui::runtime::EventSource& _eventSource;               ///< Wait source (injected).
    endo::platform::FileInfoProvider const& _provider;     ///< Directory-listing seam (injected).
    endo::platform::MessageQueue<ScanProgress>& _progress; ///< Worker → UI channel (injected).
    AppConfig _config;                                     ///< Run configuration.

    Tree _tree;                        ///< The disk-usage tree.
    DiskUsageModel _model;             ///< Adapts the tree to the generic view.
    ThemeController _theme;            ///< Dark/light theme controller.
    Keymap _keymap;                    ///< Key → Action bindings.
    tui::Screen _screen;               ///< The render surface.
    tui::TreeTableView _browser;       ///< The generic browser component.
    tui::StatusBar _statusBar;         ///< The status line.
    tui::runtime::TuiRuntime _runtime; ///< The coroutine scheduler.

    std::mutex _treeMutex;           ///< Guards _tree between the scan worker and the UI.
    bool _scanInFlight = false;      ///< True while a scan is running.
    bool _dirty = true;              ///< Whether the screen needs a redraw (starts true).
    std::uint64_t _scannedItems = 0; ///< Latest scanned-item total (status).
    std::int64_t _scannedBytes = 0;  ///< Latest scanned-byte total (status).

    static constexpr int kScanPollMs = 60;   ///< Poll interval while a scan streams progress.
    static constexpr int kIdlePollMs = 1000;  ///< Idle poll interval (input/scan wakeups preempt it).
};

} // namespace tuidu
