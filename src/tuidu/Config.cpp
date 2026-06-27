// SPDX-License-Identifier: Apache-2.0
#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <format>

#include <tuidu/Columns.hpp>
#include <tuidu/Config.hpp>
#include <tuidu/SizeFormat.hpp>
#include <tuidu/SortMode.hpp>
#include <tuidu/ThemeController.hpp>

namespace tuidu
{

namespace
{
    /// Applies an enum-valued scalar at @p node onto @p target via @p parse.
    /// @return An error message if the scalar is present but unrecognized.
    template <typename T, typename Parser>
    [[nodiscard]] std::optional<std::string> applyEnum(YAML::Node const& node,
                                                       std::string_view keyName,
                                                       Parser parse,
                                                       T& target)
    {
        if (!node)
            return std::nullopt;
        auto const text = node.as<std::string>();
        auto const parsed = parse(text);
        if (!parsed)
            return std::format("invalid '{}' value '{}'", keyName, text);
        target = *parsed;
        return std::nullopt;
    }
} // namespace

EnvLookup systemEnv()
{
    return [](std::string_view name) -> std::optional<std::string> {
        auto const variable = std::string { name }; // env APIs need a null-terminated string.
#if defined(_WIN32)
        // MSVC deprecates std::getenv; use the bounds-checked _dupenv_s instead.
        char* buffer = nullptr;
        std::size_t size = 0;
        if (_dupenv_s(&buffer, &size, variable.c_str()) != 0 || buffer == nullptr)
            return std::nullopt;
        auto value = std::string { buffer };
        std::free(buffer);
        if (value.empty())
            return std::nullopt;
        return value;
#else
        char const* const value = std::getenv(variable.c_str());
        if (value == nullptr || *value == '\0')
            return std::nullopt;
        return std::string { value };
#endif
    };
}

std::optional<std::filesystem::path> defaultConfigPath(EnvLookup const& env)
{
    namespace fs = std::filesystem;

    // XDG_CONFIG_HOME wins on every platform when it is set.
    if (auto const xdg = env("XDG_CONFIG_HOME"))
        return fs::path { *xdg } / "tuidu" / "config.yaml";

#if defined(_WIN32)
    if (auto const appData = env("APPDATA"))
        return fs::path { *appData } / "tuidu" / "config.yaml";
#else
    if (auto const home = env("HOME"))
        return fs::path { *home } / ".config" / "tuidu" / "config.yaml";
#endif

    return std::nullopt;
}

std::optional<std::filesystem::path> defaultConfigPath()
{
    return defaultConfigPath(systemEnv());
}

std::expected<bool, std::string> applyConfigFile(endo::platform::FileSystem const& fs,
                                                 std::filesystem::path const& path,
                                                 AppConfig& config)
{
    // A missing config file is a normal "nothing to do", not an error.
    if (!fs.exists(path))
        return false;

    auto const contents = fs.readFile(path);
    if (!contents)
        return std::unexpected(
            std::format("cannot read config file '{}': {}", path.string(), contents.error()));

    try
    {
        auto const root = YAML::Load(*contents);
        if (!root || root.IsNull())
            return true; // empty document: nothing to apply.

        if (auto const node = root["root"])
            config.rootPath = node.as<std::string>();

        if (auto const err = applyEnum(root["theme"], "theme", themeModeFromString, config.themeMode))
            return std::unexpected(*err);
        if (auto const err = applyEnum(root["units"], "units", unitSystemFromString, config.units))
            return std::unexpected(*err);
        if (auto const err = applyEnum(root["size_mode"], "size_mode", sizeModeFromString, config.sizeMode))
            return std::unexpected(*err);
        if (auto const err = applyEnum(root["sort"], "sort", sortModeIndexFromKey, config.sortMode))
            return std::unexpected(*err);

        if (auto const scan = root["scan"])
        {
            if (auto const node = scan["cross_devices"])
                config.scanOptions.crossDevices = node.as<bool>();
            if (auto const node = scan["follow_symlinks"])
                config.scanOptions.followSymlinks = node.as<bool>();
            if (auto const node = scan["dedupe_hardlinks"])
                config.scanOptions.dedupeHardlinks = node.as<bool>();
        }

        if (auto const colors = root["colors"])
        {
            if (auto const node = colors["large_threshold"])
                config.largeThreshold = node.as<double>();
            if (auto const node = colors["huge_threshold"])
                config.hugeThreshold = node.as<double>();
        }
    }
    catch (YAML::Exception const& e)
    {
        return std::unexpected(std::format("cannot parse config file '{}': {}", path.string(), e.what()));
    }

    return true;
}

} // namespace tuidu
