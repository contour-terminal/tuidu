// SPDX-License-Identifier: Apache-2.0
#include <core/async/Cancellation.hpp>
#include <core/async/Task.hpp>
#include <core/net/EventLoop.hpp>
#include <core/net/IoBackend.hpp>
#include <core/platform/MessageQueue.hpp>
#include <core/platform/SystemPipe.hpp>
#include <core/platform/Wakeup.hpp>
#include <core/platform/testing/InMemoryFileSystem.hpp>
#include <core/platform/testing/MockFileInfoProvider.hpp>
#include <core/tui/InputEvent.hpp>
#include <core/tui/KeyCode.hpp>
#include <core/tui/MockTerminalOutput.hpp>
#include <core/tui/Modifier.hpp>
#include <core/tui/Terminal.hpp>
#include <core/tui/runtime/testing/ScriptedInputSource.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <tuidu/App.hpp>
#include <tuidu/DeleteProgress.hpp>

using namespace tuidu;
using core::platform::FileEntry;
using core::platform::MessageQueue;
using core::platform::testing::InMemoryFileSystem;
using core::platform::testing::MockFileInfoProvider;
using core::tui::runtime::testing::ScriptedInputSource;

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
[[nodiscard]] core::tui::InputEvent keyChar(char32_t c)
{
    return core::tui::InputEvent { core::tui::KeyEvent {
        .key = {}, .modifiers = core::tui::Modifier::None, .codepoint = c } };
}

/// How long a scripted step waits for the App to reach the state it types into. Generous: a step
/// that reaches it does so in milliseconds, and one that does not is a failure either way.
constexpr auto StepBudget = std::chrono::seconds { 10 };

/// How often a step re-reads the App's state while it waits.
constexpr auto StepPoll = std::chrono::milliseconds { 1 };

/// Everything an App test injects, declared in the order that makes teardown safe: the pipe, the
/// wakeup and the queues outlive the loop, and the loop outlives the App a case builds over it.
///
/// Readiness is real. Input is a @c ScriptedInputSource over a @c SystemPipe, so the runtime's
/// input flow parks on the pipe as it parks on a terminal; and the workers' pushes signal a real
/// @c Wakeup that the App relays into the runtime, which is the wiring `main.cpp` builds.
struct Harness
{
    Harness()
    {
        progress.setWakeup(&workerWakeup);
        deleteProgress.setWakeup(&workerWakeup);
    }

    Harness(Harness const&) = delete;
    Harness(Harness&&) = delete;
    Harness& operator=(Harness const&) = delete;
    Harness& operator=(Harness&&) = delete;
    ~Harness() = default;

    /// The @c Step::waitsFor of a step that timed out. Declared before the loop, because the
    /// script flow that writes it is the loop's to destroy.
    std::string timedOutOn;
    std::unique_ptr<core::platform::SystemPipe> inputPipe = core::platform::createSystemPipe().value();
    ScriptedInputSource input { inputPipe.get() };
    core::platform::Wakeup workerWakeup;
    MessageQueue<ScanProgress> progress;
    MessageQueue<DeleteProgress> deleteProgress;
    std::unique_ptr<core::net::IoBackend> backend = core::net::makeDefaultBackend();
    core::net::EventLoop loop { *backend };
    std::unique_ptr<core::tui::MockTerminalOutput> ownedOutput =
        std::make_unique<core::tui::MockTerminalOutput>(80, 24);
    core::tui::MockTerminalOutput* output = ownedOutput.get(); ///< Observes what the App wrote.
    core::tui::Terminal terminal { std::move(ownedOutput) };
};

/// One scripted step: wait for the App to reach a state, then type into it.
struct Step
{
    std::string_view waitsFor;                     ///< What @c ready means, reported if it never holds.
    std::function<bool()> ready;                   ///< The state the step waits for.
    std::vector<core::tui::InputEvent> keys;       ///< What the step types once @c ready holds.
    std::function<void()> then;                    ///< Run after @c keys are typed, if set.
    std::chrono::milliseconds budget = StepBudget; ///< How long @c ready may take to hold.
};

/// Waits, @c StepPoll at a time on @p loop, until @p ready holds or @p budget runs out.
/// @param loop The loop to wait on (a pointer: a coroutine's reference parameter dangles).
/// @param ready The condition to wait for, read on the loop's thread like the App's own state.
/// @param budget How long @p ready may take to hold.
/// @return Whether @p ready held within the budget.
core::async::Task<bool> eventually(core::net::EventLoop* loop,
                                   std::function<bool()> ready,
                                   std::chrono::milliseconds budget)
{
    auto const deadline = std::chrono::steady_clock::now() + budget;
    while (!ready())
    {
        if (std::chrono::steady_clock::now() >= deadline)
            co_return false;
        co_await loop->delay(StepPoll);
    }
    co_return true;
}

/// Plays @p steps into @p input, one after another. A step whose state never arrives stops the
/// loop, which cancels the App's main flow so that the case fails instead of hanging, and names
/// itself in @p timedOutOn.
///
/// After the last step it keeps waiting, for nothing: the App is expected to quit on the last key
/// it was typed, and the backstop is what turns an App that did not into a named failure. When the
/// App does quit, this flow is still parked, and the loop's teardown cancels it there.
/// @param loop The loop the App runs on.
/// @param input Where the keys are typed.
/// @param steps The script.
/// @param timedOutOn Receives the @c Step::waitsFor of a step that timed out.
core::async::Task<void> play(core::net::EventLoop* loop,
                             ScriptedInputSource* input,
                             std::vector<Step> steps,
                             std::string* timedOutOn)
{
    steps.push_back(
        Step { .waitsFor = "the App to quit on the last key", .ready = [] { return false; }, .keys = {} });
    for (auto const& step: steps)
    {
        if (!co_await eventually(loop, step.ready, step.budget))
        {
            *timedOutOn = step.waitsFor;
            loop->requestStop();
            co_return;
        }
        for (auto const& key: step.keys)
            input->pushEvents({ key });
        if (step.then)
            step.then();
    }
}

/// Runs @p app to completion while @p steps are played into it.
/// @param harness The environment @p app was built over.
/// @param app The App under test.
/// @param steps The script.
/// @return What @c App::run returned. A step that timed out fails the case.
int runScripted(Harness& harness, App& app, std::vector<Step> steps)
{
    harness.loop.spawn(play(&harness.loop, &harness.input, std::move(steps), &harness.timedOutOn));
    auto rc = -1;
    auto cancelled = false;
    try
    {
        rc = app.run();
    }
    catch (core::async::OperationCancelled const&)
    {
        cancelled = true; // The backstop stopped the loop; timedOutOn says why.
    }
    INFO("timed out waiting for " << harness.timedOutOn);
    REQUIRE(harness.timedOutOn.empty());
    REQUIRE_FALSE(cancelled);
    return rc;
}

/// The step every case starts with: once the scan has finished, type @p keys.
/// @param app The App whose scan to wait for.
/// @param keys What to type then.
[[nodiscard]] Step afterScan(App const& app, std::vector<core::tui::InputEvent> keys)
{
    return Step { .waitsFor = "the scan to finish",
                  .ready = [&app] { return !app.scanInFlight(); },
                  .keys = std::move(keys) };
}

/// Builds an App over @p harness.
[[nodiscard]] App makeApp(Harness& harness,
                          core::platform::FileInfoProvider const& provider,
                          core::platform::FileSystem const& fileSystem,
                          AppConfig config)
{
    return App { harness.loop, harness.terminal, harness.input,          harness.workerWakeup, provider,
                 fileSystem,   harness.progress, harness.deleteProgress, std::move(config) };
}
} // namespace

TEST_CASE("App: scans and quits via 'q', tree populated", "[app]")
{
    MockFileInfoProvider provider;
    provider.setEntries(".", { file("a.txt", 100), dir("sub") });
    provider.setEntries("./sub", { file("x.txt", 50) });
    InMemoryFileSystem fs;

    AppConfig config;
    config.rootPath = ".";
    config.themeMode = ThemeMode::Dark; // deterministic, no terminal query

    Harness harness;
    auto app = makeApp(harness, provider, fs, config);
    auto const rc = runScripted(harness, app, { afterScan(app, { keyChar(U'q') }) });

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
    struct ThrowingProvider final: core::platform::FileInfoProvider
    {
        [[nodiscard]] std::vector<FileEntry> listDirectory(std::string const& /*path*/) const override
        {
            throw std::runtime_error("boom: unrepresentable filename");
        }
    };

    ThrowingProvider provider;
    InMemoryFileSystem fs;

    AppConfig config;
    config.themeMode = ThemeMode::Dark;

    Harness harness;
    auto app = makeApp(harness, provider, fs, config);

    CHECK(runScripted(harness, app, { afterScan(app, { keyChar(U'q') }) }) == 0);
    REQUIRE(app.scanError().has_value());
    CHECK(app.scanError()->find("boom") != std::string::npos);
}

TEST_CASE("App: 'j' then 'q' navigates without crashing", "[app]")
{
    MockFileInfoProvider provider;
    provider.setEntries(".", { file("a.txt", 100), file("b.txt", 200) });
    InMemoryFileSystem fs;

    AppConfig config;
    config.themeMode = ThemeMode::Dark;

    Harness harness;
    auto app = makeApp(harness, provider, fs, config);
    CHECK(runScripted(harness, app, { afterScan(app, { keyChar(U'j'), keyChar(U'q') }) }) == 0);
    CHECK(app.tree()[app.tree().root()].aggSize == 300);
}

TEST_CASE("App: '?' opens the help overlay; any key closes it", "[app]")
{
    MockFileInfoProvider provider;
    provider.setEntries(".", { file("a.txt", 100) });
    InMemoryFileSystem fs;

    AppConfig config;
    config.themeMode = ThemeMode::Dark;

    Harness harness;
    auto app = makeApp(harness, provider, fs, config);

    // After scanning: press '?', then a key to dismiss once the overlay is up, then 'q' once it
    // is gone again. Each step waits for the state the previous key should have produced, so a
    // key that did not open or close the overlay fails the case by name.
    CHECK(runScripted(harness,
                      app,
                      { afterScan(app, { keyChar(U'?') }),
                        Step { .waitsFor = "'?' to open the help overlay",
                               .ready = [&app] { return app.helpVisible(); },
                               .keys = { keyChar(U'j') } },
                        Step { .waitsFor = "a key to close the help overlay",
                               .ready = [&app] { return !app.helpVisible(); },
                               .keys = { keyChar(U'q') } } })
          == 0);
    CHECK_FALSE(app.helpVisible());
}

namespace
{
/// The script of a `dd` delete: once the scan has finished, `dd`; then `q` once @p deleteOver holds.
///
/// `q` must wait for the delete to end: while it runs, the App swallows every key but Esc, so a
/// `q` typed early would be lost and the case would time out rather than test anything.
/// @param app The App under test.
/// @param deleteOver What the case expects the finished delete to have left behind.
[[nodiscard]] std::vector<Step> deleteThenQuit(App const& app, std::function<bool()> deleteOver)
{
    return { afterScan(app, { keyChar(U'd'), keyChar(U'd') }),
             Step { .waitsFor = "the delete to finish",
                    .ready = [&app,
                              deleteOver =
                                  std::move(deleteOver)] { return !app.deleteInFlight() && deleteOver(); },
                    .keys = { keyChar(U'q') } } };
}
} // namespace

TEST_CASE("App: 'dd' deletes the selected item and updates the tree", "[app][delete]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 100), file("b.txt", 200) });

    // The filesystem the delete acts on must hold the same paths the tree exposes.
    InMemoryFileSystem fs;
    fs.addFile("/r/a.txt", "a");
    fs.addFile("/r/b.txt", "bb");

    AppConfig config;
    config.rootPath = "/r";
    config.themeMode = ThemeMode::Dark;

    Harness harness;
    auto app = makeApp(harness, provider, fs, config);

    CHECK(runScripted(harness, app, deleteThenQuit(app, [&app] { return app.model().rows().size() == 1; }))
          == 0);

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

    AppConfig config;
    config.rootPath = "/r";
    config.themeMode = ThemeMode::Dark;

    Harness harness;
    auto app = makeApp(harness, provider, fs, config);

    // The failure is what ends the delete here, so it is what the script waits for: an App that
    // reported no error would time out on this step instead of passing.
    CHECK(runScripted(harness, app, deleteThenQuit(app, [&app] { return app.deleteError().has_value(); }))
          == 0);

    CHECK_FALSE(app.deleteInFlight());
    REQUIRE(app.deleteError().has_value());
    CHECK_FALSE(app.deleteError()->empty());
    CHECK(app.model().rows().size() == 2);               // both items still present
    CHECK(app.tree()[app.tree().root()].aggSize == 300); // aggregate unchanged
}

TEST_CASE("App: 'yy' copies the selected item's path to the clipboard", "[app][yank]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 100), file("b.txt", 200) });
    InMemoryFileSystem fs;

    AppConfig config;
    config.rootPath = "/r";
    config.themeMode = ThemeMode::Dark;

    Harness harness;
    auto app = makeApp(harness, provider, fs, config);

    // 'y' arms the chord, the second 'y' completes it (copy), then 'q' quits.
    CHECK(runScripted(harness, app, { afterScan(app, { keyChar(U'y'), keyChar(U'y'), keyChar(U'q') }) })
          == 0);

    // Size-desc default puts b.txt (200) at row 0, so its full path is what gets yanked.
    CHECK(harness.output->clipboardText() == "/r/b.txt");
}

TEST_CASE("App: descend into a directory then quit", "[app]")
{
    MockFileInfoProvider provider;
    provider.setEntries(".", { dir("sub"), file("top.txt", 10) });
    provider.setEntries("./sub", { file("deep.txt", 999) });
    InMemoryFileSystem fs;

    AppConfig config;
    config.themeMode = ThemeMode::Dark;

    Harness harness;
    auto app = makeApp(harness, provider, fs, config);

    // Sort is size-desc by default; "sub" (999) outranks top.txt (10), so cursor row 0 is sub.
    CHECK(runScripted(harness, app, { afterScan(app, { keyChar(U'l'), keyChar(U'q') }) })
          == 0); // descend, quit
    // After descending into sub, the model's current node is sub.
    CHECK(app.model().currentTitle() == "./sub");
}

TEST_CASE("App: a worker's push wakes the idle App without input", "[app][relay]")
{
    MockFileInfoProvider provider;
    provider.setEntries(".", { file("a.txt", 100) });
    InMemoryFileSystem fs;

    AppConfig config;
    config.themeMode = ThemeMode::Dark;

    Harness harness;
    auto app = makeApp(harness, provider, fs, config);

    // Once the scan is over the App idles on a one-second poll, and nothing is typed. A progress
    // message pushed now signals the workers' wakeup, and only the App's relay of that wakeup into
    // the runtime can have it applied in well under that second: the poll would take up to a full
    // one, and there is no input to wake the App instead. So the step's budget is a fraction of it.
    constexpr auto RelayBudget = std::chrono::milliseconds { 400 };
    auto const pushLate = [&harness] {
        (void) harness.progress.push(ScanProgress { .done = true, .error = "pushed after the scan" });
    };
    CHECK(runScripted(harness,
                      app,
                      { Step { .waitsFor = "the scan to finish",
                               .ready = [&app] { return !app.scanInFlight(); },
                               .keys = {},
                               .then = pushLate },
                        Step { .waitsFor = "the idle App to apply a pushed progress message",
                               .ready = [&app] { return app.scanError().has_value(); },
                               .keys = { keyChar(U'q') },
                               .budget = RelayBudget } })
          == 0);
    REQUIRE(app.scanError().has_value());
    CHECK(*app.scanError() == "pushed after the scan");
}

TEST_CASE("App: the end of terminal input quits the App", "[app][hangup]")
{
    MockFileInfoProvider provider;
    provider.setEntries(".", { file("a.txt", 100) });
    InMemoryFileSystem fs;

    AppConfig config;
    config.themeMode = ThemeMode::Dark;

    Harness harness;
    auto app = makeApp(harness, provider, fs, config);

    // A terminal that hangs up ends its input, and no `q` will ever arrive. The App must return
    // from run() on its own: waiting again would be cancelled at once, for ever, which is the
    // 100% CPU spin core-cpp#49 moved out of the runtime and into whoever waits on it.
    CHECK(runScripted(harness,
                      app,
                      { Step { .waitsFor = "the scan to finish",
                               .ready = [&app] { return !app.scanInFlight(); },
                               .keys = {},
                               .then = [&harness] { harness.input.closeInput(); } } })
          == 0);
}
