// SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <optional>
#include <string>

#include <tuidu/Config.hpp>

using namespace tuidu;

namespace
{
/// Builds an @ref EnvLookup over a fixed name → value map (absent names return std::nullopt).
[[nodiscard]] EnvLookup envFrom(std::map<std::string, std::string> vars)
{
    return [vars = std::move(vars)](std::string_view name) -> std::optional<std::string> {
        auto const it = vars.find(std::string { name });
        if (it == vars.end())
            return std::nullopt;
        return it->second;
    };
}
} // namespace

TEST_CASE("defaultConfigPath: XDG_CONFIG_HOME wins on every platform", "[config][paths]")
{
    auto const env =
        envFrom({ { "XDG_CONFIG_HOME", "/xdg" }, { "HOME", "/home/u" }, { "APPDATA", "C:/AD" } });
    auto const path = defaultConfigPath(env);
    REQUIRE(path.has_value());
    CHECK(path->generic_string() == "/xdg/tuidu/config.yaml");
}

TEST_CASE("defaultConfigPath: falls back to the per-OS location", "[config][paths]")
{
#if defined(_WIN32)
    auto const env = envFrom({ { "APPDATA", "C:/Users/u/AppData/Roaming" } });
    auto const path = defaultConfigPath(env);
    REQUIRE(path.has_value());
    CHECK(path->generic_string() == "C:/Users/u/AppData/Roaming/tuidu/config.yaml");
#else
    auto const env = envFrom({ { "HOME", "/home/u" } });
    auto const path = defaultConfigPath(env);
    REQUIRE(path.has_value());
    CHECK(path->generic_string() == "/home/u/.config/tuidu/config.yaml");
#endif
}

TEST_CASE("defaultConfigPath: nothing set yields no path", "[config][paths]")
{
    auto const path = defaultConfigPath(envFrom({}));
    CHECK_FALSE(path.has_value());
}
