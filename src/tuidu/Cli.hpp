// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file Cli.hpp
/// @brief Command-line parsing for tuidu: flags, `--help`/`--version`, and the scan path.
///
/// The parser is built on @c crispy::cli, but none of its types appear here: parsing yields
/// a plain @ref CliOptions overlay of "only what the user explicitly set", so it can be applied
/// *after* the YAML config layer to win precedence. @c crispy and its variant-based @c flag_store
/// stay confined to @c Cli.cpp.

#include <cstddef>
#include <optional>
#include <string>

#include <tuidu/App.hpp>
#include <tuidu/Columns.hpp>
#include <tuidu/ScanProgress.hpp>
#include <tuidu/SizeFormat.hpp>
#include <tuidu/ThemeController.hpp>

namespace tuidu
{

/// The options a user explicitly passed on the command line. Each is engaged only when the
/// corresponding flag was present, so @ref applyTo overlays them onto a config without
/// clobbering values that came from the config file or the built-in defaults.
struct CliOptions
{
    std::optional<std::string> rootPath;   ///< Positional directory to scan.
    std::optional<std::string> configPath; ///< Explicit `--config <path>` (consumed before YAML load).
    std::optional<ThemeMode> themeMode;    ///< `--theme`.
    std::optional<UnitSystem> units;       ///< `--units`.
    std::optional<SizeMode> sizeMode;      ///< `--size-mode`.
    std::optional<std::size_t> sortMode;   ///< `--sort` (index into @ref sortModes).
    std::optional<bool> crossDevices;      ///< `--cross-devices`.
    std::optional<bool> followSymlinks;    ///< `--follow-symlinks`.
    std::optional<bool> dedupeHardlinks;   ///< `--no-dedupe-hardlinks` (sets this to false).

    /// Overlays every engaged option onto @p config (CLI wins over whatever is already there).
    /// @param config The configuration to mutate.
    void applyTo(AppConfig& config) const;
};

/// The result of parsing the command line.
///
/// When @ref options is engaged the program should continue with that overlay; when it is empty
/// the program should exit immediately with @ref exitCode (0 for `--help`/`--version`, non-zero
/// for a usage error). @ref parseCommandLine prints help/version/errors itself.
struct CliParse
{
    std::optional<CliOptions> options; ///< Engaged → continue; empty → exit with @ref exitCode.
    int exitCode = 0;                  ///< Process exit code to use when @ref options is empty.
};

/// Parses @p argc / @p argv into a @ref CliParse.
///
/// Handles `--help` and `--version` by printing to stdout and requesting a clean exit, and reports
/// unknown flags / bad enum values / surplus arguments to stderr with a non-zero exit code.
/// @param argc Argument count (as received by @c main).
/// @param argv Argument vector (as received by @c main); @c argv[0] is ignored.
/// @return The parsed overlay, or an exit request.
[[nodiscard]] CliParse parseCommandLine(int argc, char const* const* argv);

} // namespace tuidu
