// SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include <tuidu/Cli.hpp>

using namespace tuidu;

namespace
{
/// Runs @ref parseCommandLine over @p args (an implicit "tuidu" argv[0] is prepended).
[[nodiscard]] CliParse run(std::vector<char const*> args)
{
    args.insert(args.begin(), "tuidu");
    return parseCommandLine(static_cast<int>(args.size()), args.data());
}
} // namespace

TEST_CASE("Cli: no arguments yields an empty overlay", "[cli]")
{
    auto const parsed = run({});
    REQUIRE(parsed.options.has_value());
    CHECK_FALSE(parsed.options->rootPath.has_value());
    CHECK_FALSE(parsed.options->themeMode.has_value());
    CHECK_FALSE(parsed.options->crossDevices.has_value());
}

TEST_CASE("Cli: every value flag maps to its option", "[cli]")
{
    auto const parsed = run({ "--theme", "light", "--units", "si", "--size-mode", "disk", "--sort", "name" });
    REQUIRE(parsed.options.has_value());
    CHECK(parsed.options->themeMode == ThemeMode::Light);
    CHECK(parsed.options->units == UnitSystem::SI);
    CHECK(parsed.options->sizeMode == SizeMode::Disk);
    CHECK(parsed.options->sortMode == 2); // "name"
}

TEST_CASE("Cli: boolean scan flags are presence-based", "[cli]")
{
    auto const parsed = run({ "--cross-devices", "--follow-symlinks", "--no-dedupe-hardlinks" });
    REQUIRE(parsed.options.has_value());
    CHECK(parsed.options->crossDevices == true);
    CHECK(parsed.options->followSymlinks == true);
    CHECK(parsed.options->dedupeHardlinks == false);
}

TEST_CASE("Cli: the positional argument is the scan path", "[cli]")
{
    auto const parsed = run({ "--theme", "dark", "/var/log" });
    REQUIRE(parsed.options.has_value());
    REQUIRE(parsed.options->rootPath.has_value());
    CHECK(*parsed.options->rootPath == "/var/log");
}

TEST_CASE("Cli: --config is captured for the loader", "[cli]")
{
    auto const parsed = run({ "--config", "/etc/tuidu.yaml" });
    REQUIRE(parsed.options.has_value());
    REQUIRE(parsed.options->configPath.has_value());
    CHECK(*parsed.options->configPath == "/etc/tuidu.yaml");
}

TEST_CASE("Cli: --help requests a clean exit", "[cli]")
{
    auto const parsed = run({ "--help" });
    CHECK_FALSE(parsed.options.has_value());
    CHECK(parsed.exitCode == 0);
}

TEST_CASE("Cli: --version requests a clean exit", "[cli]")
{
    auto const parsed = run({ "--version" });
    CHECK_FALSE(parsed.options.has_value());
    CHECK(parsed.exitCode == 0);
}

TEST_CASE("Cli: an unknown flag is a usage error", "[cli]")
{
    auto const parsed = run({ "--bogus" });
    CHECK_FALSE(parsed.options.has_value());
    CHECK(parsed.exitCode == 2);
}

TEST_CASE("Cli: an invalid enum value is a usage error", "[cli]")
{
    auto const parsed = run({ "--theme", "chartreuse" });
    CHECK_FALSE(parsed.options.has_value());
    CHECK(parsed.exitCode == 2);
}

TEST_CASE("Cli: a surplus positional argument is a usage error", "[cli]")
{
    auto const parsed = run({ "/a", "/b" });
    CHECK_FALSE(parsed.options.has_value());
    CHECK(parsed.exitCode == 2);
}
