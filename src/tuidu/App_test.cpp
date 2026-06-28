// SPDX-License-Identifier: Apache-2.0
#include <tui/InputEvent.hpp>
#include <tui/KeyCode.hpp>
#include <tui/MockTerminalOutput.hpp>
#include <tui/Modifier.hpp>
#include <tui/Terminal.hpp>
#include <tui/runtime/EventSource.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <platform/MessageQueue.hpp>
#include <platform/testing/InMemoryFileSystem.hpp>
#include <platform/testing/MockFileInfoProvider.hpp>
#include <tuidu/App.hpp>
#include <tuidu/DeleteProgress.hpp>

using namespace tuidu;
using endo::platform::FileEntry;
using endo::platform::MessageQueue;
using endo::platform::testing::InMemoryFileSystem;
using endo::platform::testing::MockFileInfoProvider;

namespace
{
[[nodiscard]] FileEntry file(std::string name, std::int64_t size)
{
    return FileEntry { .name = std::move(name),
                       .size = size,
                       .blocks = (size + 511) / 512,
                       .mode = 0644,
                       .mtime = 100,
                       .dev = 1,
                       .ino = 0,
                       .isDir = false,
                       .isSymlink = false };
}

[[nodiscard]] FileEntry dir(std::string name)
{
    return FileEntry { .name = std::move(name),
                       .size = 0,
                       .blocks = 0,
                       .mode = 0755,
                       .mtime = 100,
                       .dev = 1,
                       .ino = 0,
                       .isDir = true,
                       .isSymlink = false };
}

/// A key-press input event for a printable character.
[[nodiscard]] tui::InputEvent keyChar(char32_t c)
{
    return tui::InputEvent { tui::KeyEvent { .key = {}, .modifiers = tui::Modifier::None, .codepoint = c } };
}

/// A deterministic EventSource for App tests: it reports the scan-progress wakeup
/// (AgentReady) while the scan is still in flight, then delivers pre-scripted key events.
///
/// "Scan in flight" is read from a predicate (wired to @c App::scanInFlight) rather than
/// guessed from the poll timeout — the runtime, not the App, owns the actual wait timeout,
/// so the predicate is the only reliable signal. This removes the worker/UI race without
/// any fixed sleeps.
class ScanThenKeysSource: public tui::runtime::EventSource
{
  public:
    explicit ScanThenKeysSource(std::vector<tui::InputEvent> keys): _keys(std::move(keys)) {}

    /// Wires the "scan in flight" predicate (set after the App is constructed).
    void setScanInFlight(std::function<bool()> pred) { _scanInFlight = std::move(pred); }

    tui::runtime::WaitOutcome wait(int /*timeoutMs*/) override
    {
        if (_scanInFlight && _scanInFlight())
        {
            // Scan running: nudge the worker forward, then ask the App to drain progress.
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return tui::runtime::WaitOutcome { .agentReady = true };
        }

        // Scan finished: deliver the scripted keys, then a backstop interrupt.
        if (_keyIndex < _keys.size())
            return tui::runtime::WaitOutcome { .events = { _keys[_keyIndex++] } };
        return tui::runtime::WaitOutcome { .interrupted = true };
    }

  private:
    std::vector<tui::InputEvent> _keys;
    std::function<bool()> _scanInFlight;
    std::size_t _keyIndex = 0;
};
} // namespace

TEST_CASE("App: scans and quits via 'q', tree populated", "[app]")
{
    MockFileInfoProvider provider;
    provider.setEntries(".", { file("a.txt", 100), dir("sub") });
    provider.setEntries("./sub", { file("x.txt", 50) });

    // Wait for the scan to finish (poll timeout flips to -1), then press 'q'.
    ScanThenKeysSource source { { keyChar(U'q') } };

    auto mockOut = std::make_unique<tui::MockTerminalOutput>(80, 24);
    tui::Terminal terminal { std::move(mockOut) };

    MessageQueue<ScanProgress> progress;
    MessageQueue<DeleteProgress> deleteProgress;
    InMemoryFileSystem fs;

    AppConfig config;
    config.rootPath = ".";
    config.themeMode = ThemeMode::Dark; // deterministic, no terminal query

    App app { terminal, source, provider, fs, progress, deleteProgress, config };
    source.setScanInFlight([&] { return app.scanInFlight(); });
    auto const rc = app.run();

    CHECK(rc == 0);
    // The scan completed before quit: root aggregate reflects a.txt + sub/x.txt.
    CHECK(app.tree().root() != InvalidNode);
    CHECK(app.tree()[app.tree().root()].aggSize == 150);
}

TEST_CASE("App: a failed scan surfaces a diagnostic instead of aborting", "[app]")
{
    // A provider that throws stands in for the real failure (a Windows filename the ANSI code
    // page can't map). The app must keep running, expose the error via scanError(), and not
    // terminate the process.
    struct ThrowingProvider final: endo::platform::FileInfoProvider
    {
        [[nodiscard]] std::vector<FileEntry> listDirectory(std::string const& /*path*/) const override
        {
            throw std::runtime_error("boom: unrepresentable filename");
        }
    };

    ThrowingProvider provider;
    ScanThenKeysSource source { { keyChar(U'q') } };

    auto mockOut = std::make_unique<tui::MockTerminalOutput>(80, 24);
    tui::Terminal terminal { std::move(mockOut) };
    MessageQueue<ScanProgress> progress;
    MessageQueue<DeleteProgress> deleteProgress;
    InMemoryFileSystem fs;

    AppConfig config;
    config.themeMode = ThemeMode::Dark;
    App app { terminal, source, provider, fs, progress, deleteProgress, config };
    source.setScanInFlight([&] { return app.scanInFlight(); });

    CHECK(app.run() == 0);
    REQUIRE(app.scanError().has_value());
    CHECK(app.scanError()->find("boom") != std::string::npos);
}

TEST_CASE("App: 'j' then 'q' navigates without crashing", "[app]")
{
    MockFileInfoProvider provider;
    provider.setEntries(".", { file("a.txt", 100), file("b.txt", 200) });

    ScanThenKeysSource source { { keyChar(U'j'), keyChar(U'q') } };

    auto mockOut = std::make_unique<tui::MockTerminalOutput>(80, 24);
    tui::Terminal terminal { std::move(mockOut) };
    MessageQueue<ScanProgress> progress;
    MessageQueue<DeleteProgress> deleteProgress;
    InMemoryFileSystem fs;

    AppConfig config;
    config.themeMode = ThemeMode::Dark;
    App app { terminal, source, provider, fs, progress, deleteProgress, config };
    source.setScanInFlight([&] { return app.scanInFlight(); });
    CHECK(app.run() == 0);
    CHECK(app.tree()[app.tree().root()].aggSize == 300);
}

TEST_CASE("App: '?' opens the help overlay; any key closes it", "[app]")
{
    MockFileInfoProvider provider;
    provider.setEntries(".", { file("a.txt", 100) });

    // After scanning: press '?', then a key to dismiss, then 'q'. The source observes
    // app.helpVisible() between keys to assert the overlay toggled.
    bool sawHelpOpen = false;
    bool sawHelpClosedAgain = false;
    auto* appPtr = static_cast<App*>(nullptr);

    // A source wired (after construction) to the app, checking help state between keys.
    struct HelpProbeSource: tui::runtime::EventSource
    {
        std::function<bool()> scanInFlight;
        std::function<bool()> helpVisible;
        bool* sawOpen = nullptr;
        bool* sawClosed = nullptr;
        int step = 0;

        tui::runtime::WaitOutcome wait(int) override
        {
            if (scanInFlight && scanInFlight())
            {
                std::this_thread::yield();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return tui::runtime::WaitOutcome { .agentReady = true };
            }
            switch (step++)
            {
                case 0: // open help
                    return tui::runtime::WaitOutcome { .events = { keyChar(U'?') } };
                case 1: // help should be open now; send a key to dismiss
                    if (helpVisible && helpVisible())
                        *sawOpen = true;
                    return tui::runtime::WaitOutcome { .events = { keyChar(U'j') } };
                case 2: // help should be closed now; quit
                    if (helpVisible && !helpVisible())
                        *sawClosed = true;
                    return tui::runtime::WaitOutcome { .events = { keyChar(U'q') } };
                default: return tui::runtime::WaitOutcome { .interrupted = true };
            }
        }
    } source;

    source.sawOpen = &sawHelpOpen;
    source.sawClosed = &sawHelpClosedAgain;

    auto mockOut = std::make_unique<tui::MockTerminalOutput>(80, 24);
    tui::Terminal terminal { std::move(mockOut) };
    MessageQueue<ScanProgress> progress;
    MessageQueue<DeleteProgress> deleteProgress;
    InMemoryFileSystem fs;

    AppConfig config;
    config.themeMode = ThemeMode::Dark;
    App app { terminal, source, provider, fs, progress, deleteProgress, config };
    appPtr = &app;
    source.scanInFlight = [&] {
        return app.scanInFlight();
    };
    source.helpVisible = [&] {
        return app.helpVisible();
    };

    CHECK(app.run() == 0);
    CHECK(sawHelpOpen);        // '?' opened the overlay
    CHECK(sawHelpClosedAgain); // a subsequent key closed it
    CHECK_FALSE(app.helpVisible());
    (void) appPtr;
}

namespace
{
/// Drives an App through a scan, then a scripted `dd` delete, then `q`. It reports AgentReady
/// while either the scan or the delete is in flight (so the App drains progress), and otherwise
/// delivers the next scripted key. Deterministic — no fixed sleeps gate the assertions.
class DeleteFlowSource: public tui::runtime::EventSource
{
  public:
    std::function<bool()> scanInFlight;   ///< Wired to App::scanInFlight.
    std::function<bool()> deleteInFlight; ///< Wired to App::deleteInFlight.

    tui::runtime::WaitOutcome wait(int /*timeoutMs*/) override
    {
        if ((scanInFlight && scanInFlight()) || (deleteInFlight && deleteInFlight()))
        {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return tui::runtime::WaitOutcome { .agentReady = true };
        }
        switch (_step++)
        {
            case 0: return tui::runtime::WaitOutcome { .events = { keyChar(U'd') } };
            case 1: return tui::runtime::WaitOutcome { .events = { keyChar(U'd') } };
            case 2: return tui::runtime::WaitOutcome { .events = { keyChar(U'q') } };
            default: return tui::runtime::WaitOutcome { .interrupted = true };
        }
    }

  private:
    int _step = 0;
};
} // namespace

TEST_CASE("App: 'dd' deletes the selected item and updates the tree", "[app][delete]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 100), file("b.txt", 200) });

    // The filesystem the delete acts on must hold the same paths the tree exposes.
    InMemoryFileSystem fs;
    fs.addFile("/r/a.txt", "a");
    fs.addFile("/r/b.txt", "bb");

    DeleteFlowSource source;

    auto mockOut = std::make_unique<tui::MockTerminalOutput>(80, 24);
    tui::Terminal terminal { std::move(mockOut) };
    MessageQueue<ScanProgress> progress;
    MessageQueue<DeleteProgress> deleteProgress;

    AppConfig config;
    config.rootPath = "/r";
    config.themeMode = ThemeMode::Dark;
    App app { terminal, source, provider, fs, progress, deleteProgress, config };
    source.scanInFlight = [&] {
        return app.scanInFlight();
    };
    source.deleteInFlight = [&] {
        return app.deleteInFlight();
    };

    CHECK(app.run() == 0);

    // Size-desc default puts b.txt (200) at row 0, so `dd` removes it: it is gone from disk,
    // gone from the model rows, and the root aggregate dropped by 200.
    CHECK_FALSE(app.deleteInFlight());
    CHECK_FALSE(fs.exists("/r/b.txt"));
    CHECK(fs.exists("/r/a.txt"));
    CHECK(app.model().rows().size() == 1);
    CHECK(app.tree()[app.tree().root()].aggSize == 100);
}

TEST_CASE("App: a failed 'dd' delete leaves the tree unchanged", "[app][delete]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 100), file("b.txt", 200) });

    // Empty filesystem: the targeted path does not exist, so the delete reports an error and the
    // tree must be left intact.
    InMemoryFileSystem fs;

    DeleteFlowSource source;

    auto mockOut = std::make_unique<tui::MockTerminalOutput>(80, 24);
    tui::Terminal terminal { std::move(mockOut) };
    MessageQueue<ScanProgress> progress;
    MessageQueue<DeleteProgress> deleteProgress;

    AppConfig config;
    config.rootPath = "/r";
    config.themeMode = ThemeMode::Dark;
    App app { terminal, source, provider, fs, progress, deleteProgress, config };
    source.scanInFlight = [&] {
        return app.scanInFlight();
    };
    source.deleteInFlight = [&] {
        return app.deleteInFlight();
    };

    CHECK(app.run() == 0);

    CHECK_FALSE(app.deleteInFlight());
    CHECK(app.model().rows().size() == 2);               // both items still present
    CHECK(app.tree()[app.tree().root()].aggSize == 300); // aggregate unchanged
}

TEST_CASE("App: 'yy' copies the selected item's path to the clipboard", "[app][yank]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 100), file("b.txt", 200) });

    // 'y' arms the chord, the second 'y' completes it (copy), then 'q' quits.
    ScanThenKeysSource source { { keyChar(U'y'), keyChar(U'y'), keyChar(U'q') } };

    auto mockOut = std::make_unique<tui::MockTerminalOutput>(80, 24);
    auto* mockOutRaw = mockOut.get();
    tui::Terminal terminal { std::move(mockOut) };
    MessageQueue<ScanProgress> progress;
    MessageQueue<DeleteProgress> deleteProgress;
    InMemoryFileSystem fs;

    AppConfig config;
    config.rootPath = "/r";
    config.themeMode = ThemeMode::Dark;
    App app { terminal, source, provider, fs, progress, deleteProgress, config };
    source.setScanInFlight([&] { return app.scanInFlight(); });
    CHECK(app.run() == 0);

    // Size-desc default puts b.txt (200) at row 0, so its full path is what gets yanked.
    CHECK(mockOutRaw->clipboardText() == "/r/b.txt");
}

TEST_CASE("App: descend into a directory then quit", "[app]")
{
    MockFileInfoProvider provider;
    provider.setEntries(".", { dir("sub"), file("top.txt", 10) });
    provider.setEntries("./sub", { file("deep.txt", 999) });

    // Sort is size-desc by default; "sub" (999) outranks top.txt (10), so cursor row 0 is sub.
    ScanThenKeysSource source { { keyChar(U'l'), keyChar(U'q') } }; // descend, quit

    auto mockOut = std::make_unique<tui::MockTerminalOutput>(80, 24);
    tui::Terminal terminal { std::move(mockOut) };
    MessageQueue<ScanProgress> progress;
    MessageQueue<DeleteProgress> deleteProgress;
    InMemoryFileSystem fs;

    AppConfig config;
    config.themeMode = ThemeMode::Dark;
    App app { terminal, source, provider, fs, progress, deleteProgress, config };
    source.setScanInFlight([&] { return app.scanInFlight(); });
    CHECK(app.run() == 0);
    // After descending into sub, the model's current node is sub.
    CHECK(app.model().currentTitle() == "./sub");
}
