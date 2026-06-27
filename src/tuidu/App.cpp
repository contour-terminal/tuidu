// SPDX-License-Identifier: Apache-2.0
#include <tui/InputEvent.hpp>
#include <tui/KeyCode.hpp>

#include <algorithm>
#include <format>
#include <string>
#include <variant>
#include <vector>

#include <tuidu/App.hpp>
#include <tuidu/ScanWorker.hpp>
#include <tuidu/SizeFormat.hpp>
#include <tuidu/SortMode.hpp>

namespace tuidu
{

namespace
{
    /// Builds the fullscreen screen configuration: a full-screen app on the alternate
    /// screen buffer (so the user's scrollback is preserved and restored on exit).
    [[nodiscard]] tui::ScreenConfig makeScreenConfig()
    {
        tui::ScreenConfig config {};
        config.viewport = tui::Viewport::Fullscreen;
        config.alternateScreen = true;
        return config;
    }
} // namespace

App::App(tui::Terminal& terminal,
         tui::runtime::EventSource& eventSource,
         endo::platform::FileInfoProvider const& provider,
         endo::platform::FileSystem const& fileSystem,
         endo::platform::MessageQueue<ScanProgress>& progress,
         endo::platform::MessageQueue<DeleteProgress>& deleteProgress,
         AppConfig config):
    _terminal(terminal),
    _eventSource(eventSource),
    _provider(provider),
    _fileSystem(fileSystem),
    _progress(progress),
    _deleteProgress(deleteProgress),
    _config(std::move(config)),
    _model(_tree),
    _theme(_config.themeMode),
    _screen(terminal, makeScreenConfig()),
    _browser(_model, tui::TreeTableConfig { .showHeader = true, .barColumn = -1 }),
    _help(_keymap),
    _runtime(eventSource)
{
    _model.setSizeMode(_config.sizeMode);
    _model.setUnits(_config.units);

    // Lay out the browser above a one-line status bar.
    auto const rows = _screen.rows();
    auto const cols = _screen.cols();
    _screen.root().addChild(
        _browser,
        tui::LayoutParams { .area = tui::Rect { .x = 0, .y = 0, .width = cols, .height = rows - 1 } });
    _screen.root().addChild(
        _statusBar,
        tui::LayoutParams { .area = tui::Rect { .x = 0, .y = rows - 1, .width = cols, .height = 1 } });
    _screen.setFocus(&_browser);
}

void App::refreshStatus()
{
    auto const root = _tree.root();
    auto const total = (_model.sizeMode() == SizeMode::Disk) ? _tree[root].aggBlocks : _tree[root].aggSize;
    auto const sortLabel = sortModes().empty() ? std::string_view {} : sortModes().front().label;

    _statusBar.setLeftText(_model.currentTitle());
    if (_scanInFlight)
        _statusBar.setRightText(
            std::format("scanning… {} items  {}", _scannedItems, formatSize(_scannedBytes, _config.units)));
    else
        _statusBar.setRightText(
            std::format("{}  {} items", formatSize(total, _config.units), _tree[root].itemCount));
    (void) sortLabel;
}

void App::setHelpVisible(bool visible)
{
    if (visible == _helpVisible)
        return;
    _helpVisible = visible;

    if (visible)
    {
        // Center the overlay on screen and give it focus so it receives the next key.
        auto const size = _help.preferredSize();
        auto const x = std::max(0, (_screen.cols() - size.width) / 2);
        auto const y = std::max(0, (_screen.rows() - size.height) / 2);
        _help.setArea(tui::Rect { .x = x, .y = y, .width = size.width, .height = size.height });
        _screen.showOverlay(_help, tui::Point { .x = x, .y = y });
    }
    else
    {
        _screen.hideOverlay(_help);
    }
    _screen.invalidate();
    _dirty = true;
}

void App::setDeleteDialogVisible(bool visible)
{
    if (visible)
    {
        auto const size = _deleteDialog.preferredSize();
        auto const x = std::max(0, (_screen.cols() - size.width) / 2);
        auto const y = std::max(0, (_screen.rows() - size.height) / 2);
        _deleteDialog.setArea(tui::Rect { .x = x, .y = y, .width = size.width, .height = size.height });
        _screen.showOverlay(_deleteDialog, tui::Point { .x = x, .y = y });
    }
    else
    {
        _screen.hideOverlay(_deleteDialog);
    }
    _screen.invalidate();
    _dirty = true;
}

void App::beginDelete()
{
    // Precondition: called from dispatch(), which already holds _treeMutex — so we read the tree
    // directly here and must not re-lock (the mutex is non-recursive).
    if (_deleteInFlight)
        return;

    auto valid = false;
    auto const rowId = _browser.cursorRow(valid);
    if (!valid)
        return;

    auto const id = static_cast<NodeId>(rowId);
    if (id == _tree.root() || id == InvalidNode)
        return; // never delete the scan root itself

    auto const& node = _tree[id];
    auto const path = _tree.fullPath(id);
    auto const total = node.itemCount;
    // Recurse only into a real directory; a symlink (even to a directory) is unlinked, not followed.
    auto const mode = (node.isDir() && !node.isSymlink()) ? DeleteMode::Recursive : DeleteMode::SingleEntry;

    _deletingNode = id;
    _deleteInFlight = true;
    _deleteDialog.setTarget(path);
    _deleteDialog.setProgress(0.0F);
    _deleteDialog.setStatus(std::format("Deleting 0 / {}", total));

    _deleteWorker.emplace(_fileSystem, _deleteProgress);
    _deleteWorker->start(path, total, mode);
    setDeleteDialogVisible(true);
}

void App::yankSelectedPath()
{
    // Precondition: called from dispatch(), which already holds _treeMutex.
    auto valid = false;
    auto const rowId = _browser.cursorRow(valid);
    if (!valid)
        return;

    auto const path = _tree.fullPath(static_cast<NodeId>(rowId));
    // OSC 52 copy through the terminal — works over SSH and across platforms.
    _terminal.output().copyToClipboard(path);
}

void App::drainDeleteProgress()
{
    if (!_deleteInFlight)
        return;

    std::vector<DeleteProgress> batch;
    _deleteProgress.drainTo(batch);
    if (batch.empty())
        return;

    auto finished = false;
    auto failed = false;
    std::string failure;
    for (auto const& p: batch)
    {
        auto const fraction =
            (p.total != 0) ? static_cast<float>(p.deleted) / static_cast<float>(p.total) : 1.0F;
        _deleteDialog.setProgress(fraction);
        _deleteDialog.setStatus(std::format("Deleting {} / {}", p.deleted, p.total));
        if (p.done)
        {
            finished = true;
            if (p.error.has_value())
            {
                failed = true;
                failure = *p.error;
            }
            else if (!p.cancelled)
            {
                // Remove the now-deleted subtree and roll its size back out of the ancestors.
                std::scoped_lock const lock { _treeMutex };
                _tree.removeSubtree(_deletingNode);
                _model.resort();
                _browser.moveCursor(0); // re-clamp the cursor onto the shrunken row set
            }
        }
    }

    if (finished)
    {
        _deleteInFlight = false;
        _deletingNode = InvalidNode;
        setDeleteDialogVisible(false);
    }

    if (failed)
        _statusBar.setLeftText(std::format("delete failed: {}", failure));
    else
        refreshStatus();
    _screen.invalidate();
    _dirty = true;
}

void App::applyProgress()
{
    std::vector<ScanProgress> batch;
    _progress.drainTo(batch);
    if (batch.empty())
        return;
    for (auto const& p: batch)
    {
        _scannedItems = p.scannedItems;
        _scannedBytes = p.scannedBytes;
        if (p.done)
            _scanInFlight = false;
    }
    // The tree grew (or finished); re-apply the sort so new children appear in order.
    // Hold the tree mutex so the worker is not mid-mutation while we read/sort.
    {
        std::scoped_lock const lock { _treeMutex };
        _model.resort();
        refreshStatus();
    }
    _screen.invalidate();
    _dirty = true;
}

bool App::dispatch(Action action)
{
    // Navigation/sort touch the tree (sorting, descend/ascend); hold the mutex so a
    // concurrent scan worker is not mid-mutation.
    std::scoped_lock const lock { _treeMutex };
    switch (action)
    {
        case Action::Quit: return false;
        case Action::MoveDown: _browser.moveCursor(1); break;
        case Action::MoveUp: _browser.moveCursor(-1); break;
        case Action::MoveTop: _browser.moveToTop(); break;
        case Action::MoveBottom: _browser.moveToBottom(); break;
        case Action::PageDown: _browser.pageBy(1); break;
        case Action::PageUp: _browser.pageBy(-1); break;
        case Action::HalfPageDown: _browser.halfPageBy(1); break;
        case Action::HalfPageUp: _browser.halfPageBy(-1); break;
        case Action::Descend: _browser.descendCursor(); break;
        case Action::Ascend: _browser.ascendCursor(); break;
        case Action::SortBySize:
        case Action::SortByName:
        case Action::SortByItems:
        case Action::SortByMtime: _browser.sortBy(static_cast<int>(action)); break;
        case Action::ToggleApparentDisk:
            _model.setSizeMode(_model.sizeMode() == SizeMode::Apparent ? SizeMode::Disk : SizeMode::Apparent);
            break;
        case Action::Help: setHelpVisible(!_helpVisible); break;
        case Action::Delete: beginDelete(); break;
        case Action::YankPath: yankSelectedPath(); break;
        case Action::None:
        case Action::ToggleHidden:
        case Action::SearchOpen:
        case Action::Rescan:
        case Action::ShowInfo: break; // not yet wired in this MVP
    }
    refreshStatus();
    _screen.invalidate();
    _dirty = true;
    return true;
}

endo::coro::Task<void> App::mainFlow()
{
    refreshStatus();
    bool running = true;
    while (running)
    {
        applyProgress();
        drainDeleteProgress();

        // Render only when something changed. The Screen diff-renders, but skipping a
        // no-op draw entirely avoids burning CPU on idle wakeups.
        if (_dirty)
        {
            // Rendering reads the tree (the model walks it); hold the mutex so the scan
            // worker is not mutating it concurrently.
            std::scoped_lock const lock { _treeMutex };
            _screen.draw();
            _dirty = false;
        }

        // nextActivity() takes a *positive* timeout (it is a relative deadline; a negative
        // value would resolve to a past deadline and busy-spin on wait(0)). Poll briefly
        // while a scan streams progress, and idle on a long interval otherwise — input and
        // scan wakeups interrupt the wait immediately regardless of the timeout.
        auto const pollMs = std::chrono::milliseconds { _scanInFlight ? kScanPollMs : kIdlePollMs };
        auto activity = co_await _runtime.nextActivity(pollMs);

        switch (activity.kind)
        {
            case tui::runtime::ActivityKind::AgentReady: continue; // scan progress pending; drain at the top
            case tui::runtime::ActivityKind::Timeout: continue;    // idle tick; nothing to do
            case tui::runtime::ActivityKind::Event: break;
        }

        if (!activity.event)
            continue;
        auto const& event = *activity.event;

        if (auto const* key = std::get_if<tui::KeyEvent>(&event))
        {
            // While a delete runs, Esc cancels it; every other key is swallowed so the browser
            // beneath stays inert until the delete finishes.
            if (_deleteInFlight)
            {
                if (key->key == tui::KeyCode::Escape && _deleteWorker.has_value())
                    _deleteWorker->requestStop();
                continue;
            }

            // While the help overlay is up, any key dismisses it and is otherwise swallowed.
            if (_helpVisible)
            {
                setHelpVisible(false);
                continue;
            }

            // Feed the key to the chord recognizer first (e.g. `dd`). A pending lead key is
            // swallowed until its completing press; a completed chord dispatches its action;
            // anything else passes through to the single-key keymap.
            auto const chord = _chords.feed(*key);
            if (chord.outcome == ChordOutcome::Pending)
                continue;
            auto const action =
                (chord.outcome == ChordOutcome::Completed) ? chord.action : _keymap.lookup(*key);
            if (action != Action::None)
            {
                if (!dispatch(action))
                {
                    running = false;
                    continue;
                }
            }
        }
        else if (auto const* scheme = std::get_if<tui::ColorSchemeReport>(&event))
        {
            auto const cs = (scheme->mode == 2) ? tui::ColorScheme::Light : tui::ColorScheme::Dark;
            if (_theme.onColorScheme(cs))
            {
                _screen.setTheme(tui::currentTheme());
                _screen.invalidate();
                _dirty = true;
            }
        }
        else
        {
            // Mouse events drive the browser (click to select, double-click to descend, wheel to
            // scroll). While a modal overlay is up, swallow them so a click cannot reach the
            // browser beneath; resize/focus still apply.
            if (std::holds_alternative<tui::MouseEvent>(event) && (_deleteInFlight || _helpVisible))
                continue;

            // Resize / mouse / focus: let the component tree handle it and redraw. The browser
            // reads the tree while navigating, so hold the tree mutex against the scan worker.
            {
                std::scoped_lock const lock { _treeMutex };
                (void) _screen.dispatchEvent(event);
            }
            _dirty = true;
        }
    }
    co_return;
}

int App::run()
{
    // Pick the initial theme from the terminal's reported scheme before the first draw.
    (void) _theme.applyForScheme(_terminal.colorScheme());
    _screen.setTheme(tui::currentTheme());

    // Build the root node and point the model at it (the model was constructed over an
    // empty tree, so its current directory is only valid now), then kick off the scan.
    _tree.createRoot(_config.rootPath);
    _model.resetToRoot();

    ScanWorker worker(_provider, _tree, _progress, _config.scanOptions, _treeMutex);
    _scanInFlight = true;
    worker.start(_tree.root());

    _runtime.setInterruptHandler([this] { _runtime.rootStopSource().request_stop(); });
    _runtime.blockOn(mainFlow());

    worker.requestStop();
    return 0;
}

} // namespace tuidu
