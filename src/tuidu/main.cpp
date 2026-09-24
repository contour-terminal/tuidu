// SPDX-License-Identifier: Apache-2.0
/// @file main.cpp
/// @brief Composition root for tuidu — the TUI-first `du`.
///
/// This is the only translation unit that names concrete platform implementations;
/// everything else takes injected abstractions.

#include <core/net/EventLoop.hpp>
#include <core/net/IoBackend.hpp>
#include <core/platform/FileInfoProvider.hpp>
#include <core/platform/MessageQueue.hpp>
#include <core/platform/NativeFileSystem.hpp>
#include <core/platform/Wakeup.hpp>
#include <core/tui/Terminal.hpp>
#include <core/tui/runtime/TerminalInputSource.hpp>

#include <filesystem>
#include <optional>
#include <print>

#include <tuidu/App.hpp>
#include <tuidu/Cli.hpp>
#include <tuidu/Config.hpp>
#include <tuidu/DeleteProgress.hpp>
#include <tuidu/ScanProgress.hpp>

/// @brief Program entry point.
/// @param argc Argument count.
/// @param argv Arguments: optional flags (see `--help`) and a directory to scan.
/// @return Process exit code.
int main(int argc, char const* argv[])
{
    // 1. Parse the command line. `--help`/`--version`/usage errors exit here.
    auto cli = tuidu::parseCommandLine(argc, argv);
    if (!cli.options)
        return cli.exitCode;

    core::platform::NativeFileSystem fileSystem;

    // 2. Start from defaults, then overlay the YAML config file (explicit --config wins over the
    //    auto-discovered per-OS path). 3. CLI flags overlay last so they win over the file.
    auto config = tuidu::AppConfig {};
    auto const configPath = cli.options->configPath
                                ? std::optional<std::filesystem::path> { *cli.options->configPath }
                                : tuidu::defaultConfigPath();
    if (configPath)
    {
        auto const applied = tuidu::applyConfigFile(fileSystem, *configPath, config);
        if (!applied)
        {
            std::println(stderr, "tuidu: {}", applied.error());
            return 1;
        }
    }
    cli.options->applyTo(config);

    auto const provider = core::platform::nativeFileInfoProvider();

    // Worker → UI channels; the App waits on their shared wakeup so a scan- or delete-progress push
    // wakes the UI loop (surfacing as ActivityKind::AgentReady).
    core::platform::Wakeup scanWakeup;
    core::platform::MessageQueue<tuidu::ScanProgress> progress;
    progress.setWakeup(&scanWakeup);
    core::platform::MessageQueue<tuidu::DeleteProgress> deleteProgress;
    deleteProgress.setWakeup(&scanWakeup);

    core::tui::Terminal terminal;
    if (auto const init = terminal.initialize(); !init)
    {
        std::println(stderr, "tuidu: failed to initialize terminal");
        return 1;
    }

    // The input source reads the terminal's handles, so it is built after initialize(). The loop is
    // declared before the App so it outlives it: the runtime inside the App is torn down before the
    // loop it is parked on.
    auto input = core::tui::runtime::TerminalInputSource { terminal };
    auto const backend = core::net::makeDefaultBackend();
    auto loop = core::net::EventLoop { *backend };

    auto app = tuidu::App { loop,       terminal, input,          scanWakeup, *provider,
                            fileSystem, progress, deleteProgress, config };
    auto const rc = app.run();

    terminal.shutdown();

    // If the background scan aborted, the TUI is gone now — echo the diagnostic to stderr so the
    // user always sees why, rather than the process appearing to exit normally.
    if (auto const& scanError = app.scanError())
    {
        std::println(stderr, "tuidu: scan did not complete: {}", *scanError);
        return 1;
    }
    return rc;
}
