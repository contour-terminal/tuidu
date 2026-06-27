// SPDX-License-Identifier: Apache-2.0
/// @file main.cpp
/// @brief Composition root for tuidu — the TUI-first `du`.
///
/// This is the only translation unit that names concrete platform implementations;
/// everything else takes injected abstractions.

#include <platform/MessageQueue.hpp>
#include <platform/NativeFileSystem.hpp>
#include <platform/Wakeup.hpp>
#include <tuidu/App.hpp>
#include <tuidu/Cli.hpp>
#include <tuidu/Config.hpp>
#include <tuidu/DeleteProgress.hpp>
#include <tuidu/ScanProgress.hpp>

#if !defined(_WIN32)
    #include <platform/linux/LinuxFileInfoProvider.hpp>
#else
    #include <platform/windows/WindowsFileInfoProvider.hpp>
#endif

#include <tui/Terminal.hpp>
#include <tui/runtime/TerminalEventSource.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <print>

namespace
{
/// Constructs the platform-appropriate FileInfoProvider.
[[nodiscard]] std::unique_ptr<endo::platform::FileInfoProvider> makeFileInfoProvider()
{
#if !defined(_WIN32)
    return std::make_unique<endo::platform::LinuxFileInfoProvider>();
#else
    return std::make_unique<endo::platform::WindowsFileInfoProvider>();
#endif
}
} // namespace

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

    endo::platform::NativeFileSystem fileSystem;

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

    auto provider = makeFileInfoProvider();

    // Worker → UI channels; their shared wakeup is what the event source selects on so a scan- or
    // delete-progress push wakes the UI loop (surfacing as ActivityKind::AgentReady).
    endo::platform::Wakeup scanWakeup;
    endo::platform::MessageQueue<tuidu::ScanProgress> progress;
    progress.setWakeup(&scanWakeup);
    endo::platform::MessageQueue<tuidu::DeleteProgress> deleteProgress;
    deleteProgress.setWakeup(&scanWakeup);

    tui::Terminal terminal;
    if (auto const init = terminal.initialize(); !init)
    {
        std::println(stderr, "tuidu: failed to initialize terminal");
        return 1;
    }

    tui::runtime::TerminalEventSource eventSource { terminal, &scanWakeup };

    auto app = tuidu::App { terminal, eventSource, *provider, fileSystem, progress, deleteProgress, config };
    auto const rc = app.run();

    terminal.shutdown();
    return rc;
}
