// SPDX-License-Identifier: Apache-2.0
#include <crispy/CLI.h>

#include <exception>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>

#include <tuidu/Cli.hpp>
#include <tuidu/SortMode.hpp>

#ifndef TUIDU_VERSION
    #define TUIDU_VERSION "unknown"
#endif

namespace tuidu
{

namespace cli = crispy::cli;

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace
{
    /// The command name; also the prefix crispy uses for every parsed flag key ("tuidu.theme").
    constexpr std::string_view CommandName = "tuidu";

    /// Right margin (columns) used when rendering help/usage text.
    constexpr unsigned HelpMargin = 100;

    /// @return The fully-qualified flag-store key for option @p name (e.g. "tuidu.theme").
    [[nodiscard]] std::string key(std::string_view name)
    {
        return std::string { CommandName } + "." + std::string { name };
    }

    /// Builds the data-driven command description: one row per option. Adding a flag is one entry.
    [[nodiscard]] cli::command makeCommand()
    {
        return cli::command {
            .name = CommandName,
            .helpText = "tuidu — a TUI-first disk-usage analyzer (an ncdu with vim motions).",
            .options =
                cli::option_list {
                    cli::option { .name = "theme"sv,
                                  .v = cli::value { ""s },
                                  .helpText = "Color theme: auto, dark, light, or mono (default: auto)."sv,
                                  .placeholder = "THEME"sv },
                    cli::option { .name = "units"sv,
                                  .v = cli::value { ""s },
                                  .helpText = "Size units: binary (1024) or si (1000) (default: binary)."sv,
                                  .placeholder = "SYSTEM"sv },
                    cli::option { .name = "size-mode"sv,
                                  .v = cli::value { ""s },
                                  .helpText = "Size metric: apparent or disk (default: apparent)."sv,
                                  .placeholder = "MODE"sv },
                    cli::option {
                        .name = "sort"sv,
                        .v = cli::value { ""s },
                        .helpText =
                            "Initial sort: size-desc, size-asc, name, items, date (default: size-desc)."sv,
                        .placeholder = "ORDER"sv },
                    cli::option { .name = "config"sv,
                                  .v = cli::value { ""s },
                                  .helpText =
                                      "Load configuration from this YAML file instead of the default."sv,
                                  .placeholder = "PATH"sv },
                    cli::option { .name = "cross-devices"sv,
                                  .v = cli::value { false },
                                  .helpText = "Cross filesystem boundaries while scanning."sv },
                    cli::option { .name = "follow-symlinks"sv,
                                  .v = cli::value { false },
                                  .helpText = "Traverse into symlinked directories."sv },
                    cli::option { .name = "no-dedupe-hardlinks"sv,
                                  .v = cli::value { false },
                                  .helpText =
                                      "Count hardlinked files once per link instead of deduplicating."sv },
                    cli::option { .name = "help"sv,
                                  .v = cli::value { false },
                                  .helpText = "Show this help and exit."sv },
                    cli::option { .name = "version"sv,
                                  .v = cli::value { false },
                                  .helpText = "Show the version and exit."sv },
                },
            .verbatim = cli::verbatim { .placeholder = "PATH",
                                        .helpText = "Directory to scan (default: current directory)." },
        };
    }

    /// A non-empty string flag value, or @c std::nullopt when the flag was left at its default.
    [[nodiscard]] std::optional<std::string> strOpt(cli::flag_store const& flags, std::string_view name)
    {
        auto const& value = flags.str(key(name));
        if (value.empty())
            return std::nullopt;
        return value;
    }
} // namespace

void CliOptions::applyTo(AppConfig& config) const
{
    if (rootPath)
        config.rootPath = *rootPath;
    if (themeMode)
        config.themeMode = *themeMode;
    if (units)
        config.units = *units;
    if (sizeMode)
        config.sizeMode = *sizeMode;
    if (sortMode)
        config.sortMode = *sortMode;
    if (crossDevices)
        config.scanOptions.crossDevices = *crossDevices;
    if (followSymlinks)
        config.scanOptions.followSymlinks = *followSymlinks;
    if (dedupeHardlinks)
        config.scanOptions.dedupeHardlinks = *dedupeHardlinks;
}

CliParse parseCommandLine(int argc, char const* const* argv)
{
    auto const command = makeCommand();

    std::optional<cli::flag_store> parsed;
    try
    {
        parsed = cli::parse(command, argc, argv);
    }
    catch (std::exception const& e)
    {
        std::println(stderr, "tuidu: {}", e.what());
        return CliParse { .options = std::nullopt, .exitCode = 2 };
    }

    if (!parsed)
    {
        std::println(stderr, "tuidu: failed to parse command line. Try --help.");
        return CliParse { .options = std::nullopt, .exitCode = 2 };
    }

    auto const& flags = *parsed;

    if (flags.boolean(key("version")))
    {
        std::println("tuidu {}", TUIDU_VERSION);
        return CliParse { .options = std::nullopt, .exitCode = 0 };
    }

    if (flags.boolean(key("help")))
    {
        std::print("{}", cli::helpText(command, cli::help_display_style {}, HelpMargin));
        return CliParse { .options = std::nullopt, .exitCode = 0 };
    }

    // Unknown flags are not recognized as options, so crispy collects them as verbatim tokens.
    // A verbatim token that looks like a flag is therefore a usage error; at most one remaining
    // token is the scan path.
    std::optional<std::string> rootPath;
    for (auto const token: flags.verbatim)
    {
        if (token.starts_with('-'))
        {
            std::println(stderr, "tuidu: unknown option '{}'. Try --help.", token);
            return CliParse { .options = std::nullopt, .exitCode = 2 };
        }
        if (rootPath)
        {
            std::println(stderr, "tuidu: unexpected extra argument '{}'. Try --help.", token);
            return CliParse { .options = std::nullopt, .exitCode = 2 };
        }
        rootPath = std::string { token };
    }

    CliOptions options;
    options.rootPath = rootPath;
    options.configPath = strOpt(flags, "config");

    if (auto const theme = strOpt(flags, "theme"))
    {
        auto const mode = themeModeFromString(*theme);
        if (!mode)
        {
            std::println(
                stderr, "tuidu: invalid --theme value '{}' (expected auto|dark|light|mono).", *theme);
            return CliParse { .options = std::nullopt, .exitCode = 2 };
        }
        options.themeMode = *mode;
    }

    if (auto const units = strOpt(flags, "units"))
    {
        auto const system = unitSystemFromString(*units);
        if (!system)
        {
            std::println(stderr, "tuidu: invalid --units value '{}' (expected binary|si).", *units);
            return CliParse { .options = std::nullopt, .exitCode = 2 };
        }
        options.units = *system;
    }

    if (auto const mode = strOpt(flags, "size-mode"))
    {
        auto const sizeMode = sizeModeFromString(*mode);
        if (!sizeMode)
        {
            std::println(stderr, "tuidu: invalid --size-mode value '{}' (expected apparent|disk).", *mode);
            return CliParse { .options = std::nullopt, .exitCode = 2 };
        }
        options.sizeMode = *sizeMode;
    }

    if (auto const sort = strOpt(flags, "sort"))
    {
        auto const index = sortModeIndexFromKey(*sort);
        if (!index)
        {
            std::println(stderr,
                         "tuidu: invalid --sort value '{}' (expected size-desc|size-asc|name|items|date).",
                         *sort);
            return CliParse { .options = std::nullopt, .exitCode = 2 };
        }
        options.sortMode = *index;
    }

    if (flags.boolean(key("cross-devices")))
        options.crossDevices = true;
    if (flags.boolean(key("follow-symlinks")))
        options.followSymlinks = true;
    if (flags.boolean(key("no-dedupe-hardlinks")))
        options.dedupeHardlinks = false;

    return CliParse { .options = std::move(options), .exitCode = 0 };
}

} // namespace tuidu
