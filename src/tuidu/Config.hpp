// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file Config.hpp
/// @brief YAML configuration file: discovery (XDG / per-OS) and loading onto an @ref AppConfig.
///
/// Loading goes through the injected @c endo::platform::FileSystem seam (so tests drive it with an
/// @c InMemoryFileSystem), and yaml-cpp is confined to @c Config.cpp — it never appears in this header.

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include <platform/FileSystem.hpp>
#include <tuidu/App.hpp>

namespace tuidu
{

/// Looks up an environment variable by name. Injected so config-path resolution is testable
/// without touching the real process environment.
/// @param name The variable name (e.g. "XDG_CONFIG_HOME").
/// @return The value if set and non-empty, otherwise @c std::nullopt.
using EnvLookup = std::function<std::optional<std::string>(std::string_view name)>;

/// @return An @ref EnvLookup backed by the real process environment (@c std::getenv).
[[nodiscard]] EnvLookup systemEnv();

/// Resolves the default config-file path from the environment.
///
/// Honors @c XDG_CONFIG_HOME on every platform (`$XDG_CONFIG_HOME/tuidu/config.yaml`); otherwise
/// falls back to `$HOME/.config/tuidu/config.yaml` on POSIX and `%APPDATA%\tuidu\config.yaml` on
/// Windows. Returns @c std::nullopt when none of the relevant variables are set.
/// @param env The environment lookup to use.
/// @return The default config path, or @c std::nullopt if it cannot be determined.
[[nodiscard]] std::optional<std::filesystem::path> defaultConfigPath(EnvLookup const& env);

/// @copydoc defaultConfigPath(EnvLookup const&)
/// Convenience overload using @ref systemEnv.
[[nodiscard]] std::optional<std::filesystem::path> defaultConfigPath();

/// Loads @p path (if present) and overlays its settings onto @p config.
///
/// A missing file is not an error — it simply leaves @p config untouched and returns @c false.
/// Malformed YAML or an unrecognized enum/threshold value returns a human-readable error message.
/// Recognized keys: `root`, `theme`, `units`, `size_mode`, `sort`, the `scan` map
/// (`cross_devices`/`follow_symlinks`/`dedupe_hardlinks`) and the `colors` map
/// (`large_threshold`/`huge_threshold`).
/// @param fs The filesystem seam used to read the file.
/// @param path The config-file path to load.
/// @param config The configuration to overlay onto (mutated only on success).
/// @return @c true if a file was read and applied, @c false if no file was present, or an error
///         message if the file could not be read or parsed.
[[nodiscard]] std::expected<bool, std::string> applyConfigFile(endo::platform::FileSystem const& fs,
                                                               std::filesystem::path const& path,
                                                               AppConfig& config);

} // namespace tuidu
