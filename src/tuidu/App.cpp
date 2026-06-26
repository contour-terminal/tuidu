// SPDX-License-Identifier: Apache-2.0
#include <tui/InputEvent.hpp>

#include <format>
#include <vector>

#include <tuidu/App.hpp>
#include <tuidu/ScanWorker.hpp>
#include <tuidu/SizeFormat.hpp>
#include <tuidu/SortMode.hpp>

namespace tuidu
{

namespace
{
    /// Builds the fullscreen screen configuration (set the one field we care about).
    [[nodiscard]] tui::ScreenConfig makeScreenConfig()
    {
        tui::ScreenConfig config {};
        config.viewport = tui::Viewport::Fullscreen;
        return config;
    }
} // namespace

App::App(tui::Terminal& terminal,
         tui::runtime::EventSource& eventSource,
         endo::platform::FileInfoProvider const& provider,
         endo::platform::MessageQueue<ScanProgress>& progress,
         AppConfig config):
    _terminal(terminal),
    _eventSource(eventSource),
    _provider(provider),
    _progress(progress),
    _config(std::move(config)),
    _model(_tree),
    _theme(_config.themeMode),
    _screen(terminal, makeScreenConfig()),
    _browser(_model, tui::TreeTableConfig { .showHeader = true, .barColumn = -1 }),
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
        case Action::Descend: _browser.descendCursor(); break;
        case Action::Ascend: _browser.ascendCursor(); break;
        case Action::SortBySize:
        case Action::SortByName:
        case Action::SortByItems:
        case Action::SortByMtime: _browser.sortBy(static_cast<int>(action)); break;
        case Action::ToggleApparentDisk:
            _model.setSizeMode(_model.sizeMode() == SizeMode::Apparent ? SizeMode::Disk : SizeMode::Apparent);
            break;
        case Action::None:
        case Action::ToggleHidden:
        case Action::SearchOpen:
        case Action::Rescan:
        case Action::ShowInfo:
        case Action::Help: break; // not yet wired in this MVP
    }
    refreshStatus();
    _screen.invalidate();
    return true;
}

endo::coro::Task<void> App::mainFlow()
{
    refreshStatus();
    bool running = true;
    while (running)
    {
        applyProgress();
        // Rendering reads the tree (the model walks it); hold the mutex so the scan worker
        // is not mutating it concurrently.
        {
            std::scoped_lock const lock { _treeMutex };
            _screen.draw();
        }

        auto const pollMs = std::chrono::milliseconds { _scanInFlight ? 60 : -1 };
        auto activity = co_await _runtime.nextActivity(pollMs);

        switch (activity.kind)
        {
            case tui::runtime::ActivityKind::AgentReady: continue; // scan progress pending; drain at the top
            case tui::runtime::ActivityKind::Timeout: continue;    // spinner tick / idle; redraw at the top
            case tui::runtime::ActivityKind::Event: break;
        }

        if (!activity.event)
            continue;
        auto const& event = *activity.event;

        if (auto const* key = std::get_if<tui::KeyEvent>(&event))
        {
            if (auto const action = _keymap.lookup(*key); action != Action::None)
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
            }
        }
        else
        {
            (void) _screen.dispatchEvent(event);
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
