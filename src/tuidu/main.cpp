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
#include <tuidu/DeleteProgress.hpp>
#include <tuidu/ScanProgress.hpp>

#if !defined(_WIN32)
    #include <platform/linux/LinuxFileInfoProvider.hpp>
#else
    #include <platform/windows/WindowsFileInfoProvider.hpp>
#endif

#include <tui/Terminal.hpp>
#include <tui/runtime/TerminalEventSource.hpp>

#include <print>
#include <span>
#include <string>
#include <vector>

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
/// @param argv Arguments; argv[1], if present, is the directory to scan.
/// @return Process exit code.
int main(int argc, char const* argv[])
{
    auto const args = std::span<char const* const> { argv, static_cast<std::size_t>(argc) };
    auto config = tuidu::AppConfig {};
    config.rootPath = (args.size() > 1) ? std::string { args[1] } : std::string { "." };

    auto provider = makeFileInfoProvider();
    endo::platform::NativeFileSystem fileSystem;

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
