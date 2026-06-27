// SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include <platform/testing/InMemoryFileSystem.hpp>
#include <tuidu/Cli.hpp>
#include <tuidu/Config.hpp>

using namespace tuidu;
using endo::platform::testing::InMemoryFileSystem;

namespace
{
constexpr auto ConfigPath = "/cfg/config.yaml";

/// A fully-populated config document exercising every recognized key.
constexpr auto FullYaml = R"(
root: /data
theme: dark
units: si
size_mode: disk
sort: name
scan:
  cross_devices: true
  follow_symlinks: true
  dedupe_hardlinks: false
colors:
  large_threshold: 0.1
  huge_threshold: 0.6
)";
} // namespace

TEST_CASE("Config: a full document overlays every field", "[config]")
{
    InMemoryFileSystem const fs { { { .path = ConfigPath, .content = FullYaml } } };
    AppConfig config {};

    auto const applied = applyConfigFile(fs, ConfigPath, config);
    REQUIRE(applied.has_value());
    CHECK(*applied == true);

    CHECK(config.rootPath == "/data");
    CHECK(config.themeMode == ThemeMode::Dark);
    CHECK(config.units == UnitSystem::SI);
    CHECK(config.sizeMode == SizeMode::Disk);
    CHECK(config.sortMode == 2); // "name"
    CHECK(config.scanOptions.crossDevices == true);
    CHECK(config.scanOptions.followSymlinks == true);
    CHECK(config.scanOptions.dedupeHardlinks == false);
    CHECK(config.largeThreshold == 0.1);
    CHECK(config.hugeThreshold == 0.6);
}

TEST_CASE("Config: a missing file leaves the config untouched and is not an error", "[config]")
{
    InMemoryFileSystem const fs {};
    AppConfig config {};

    auto const applied = applyConfigFile(fs, "/nope/config.yaml", config);
    REQUIRE(applied.has_value());
    CHECK(*applied == false);
    CHECK(config.themeMode == ThemeMode::Auto); // unchanged default
    CHECK(config.rootPath == ".");
}

TEST_CASE("Config: an empty document applies nothing", "[config]")
{
    InMemoryFileSystem const fs { { { .path = ConfigPath, .content = "" } } };
    AppConfig config {};

    auto const applied = applyConfigFile(fs, ConfigPath, config);
    REQUIRE(applied.has_value());
    CHECK(config.themeMode == ThemeMode::Auto);
}

TEST_CASE("Config: only-present keys are overlaid; the rest keep defaults", "[config]")
{
    InMemoryFileSystem const fs { { { .path = ConfigPath, .content = "theme: light\n" } } };
    AppConfig config {};

    auto const applied = applyConfigFile(fs, ConfigPath, config);
    REQUIRE(applied.has_value());
    CHECK(config.themeMode == ThemeMode::Light);
    CHECK(config.units == UnitSystem::Binary); // untouched default
    CHECK(config.sizeMode == SizeMode::Apparent);
}

TEST_CASE("Config: an unrecognized enum value is a descriptive error", "[config]")
{
    InMemoryFileSystem const fs { { { .path = ConfigPath, .content = "theme: chartreuse\n" } } };
    AppConfig config {};

    auto const applied = applyConfigFile(fs, ConfigPath, config);
    REQUIRE_FALSE(applied.has_value());
    CHECK(applied.error().find("theme") != std::string::npos);
    CHECK(config.themeMode == ThemeMode::Auto); // not mutated on failure
}

TEST_CASE("Config: malformed YAML is reported as a parse error", "[config]")
{
    InMemoryFileSystem const fs { { { .path = ConfigPath, .content = "scan: [unterminated\n" } } };
    AppConfig config {};

    auto const applied = applyConfigFile(fs, ConfigPath, config);
    REQUIRE_FALSE(applied.has_value());
    CHECK_FALSE(applied.error().empty());
}

TEST_CASE("Config: CLI options override the config file", "[config][cli]")
{
    InMemoryFileSystem const fs { { { .path = ConfigPath, .content = "theme: dark\nunits: si\n" } } };
    AppConfig config {};

    REQUIRE(applyConfigFile(fs, ConfigPath, config).has_value());
    CHECK(config.themeMode == ThemeMode::Dark);
    CHECK(config.units == UnitSystem::SI);

    // The CLI layer applies last, so an explicit --theme wins over the file while unspecified
    // options (here: units) keep the file's value.
    CliOptions cli {};
    cli.themeMode = ThemeMode::Light;
    cli.applyTo(config);

    CHECK(config.themeMode == ThemeMode::Light); // CLI won
    CHECK(config.units == UnitSystem::SI);       // file value preserved
}
