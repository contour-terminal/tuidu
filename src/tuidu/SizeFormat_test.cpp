// SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include <tuidu/SizeFormat.hpp>

using namespace tuidu;

TEST_CASE("formatSize: binary boundaries", "[sizeformat]")
{
    CHECK(formatSize(0) == "0 B");
    CHECK(formatSize(512) == "512 B");
    CHECK(formatSize(1023) == "1023 B");
    CHECK(formatSize(1024) == "1.0 KiB");
    CHECK(formatSize(1536) == "1.5 KiB"); // 1024 + 512
    CHECK(formatSize(1048576) == "1.0 MiB");
    CHECK(formatSize(1073741824) == "1.0 GiB");
    CHECK(formatSize(1099511627776LL) == "1.0 TiB");
}

TEST_CASE("formatSize: SI boundaries", "[sizeformat]")
{
    CHECK(formatSize(0, UnitSystem::SI) == "0 B");
    CHECK(formatSize(999, UnitSystem::SI) == "999 B");
    CHECK(formatSize(1000, UnitSystem::SI) == "1.0 kB");
    CHECK(formatSize(1500, UnitSystem::SI) == "1.5 kB");
    CHECK(formatSize(1000000, UnitSystem::SI) == "1.0 MB");
    CHECK(formatSize(1000000000LL, UnitSystem::SI) == "1.0 GB");
}

TEST_CASE("formatSize: binary vs SI differ at the same magnitude", "[sizeformat]")
{
    // 1000 bytes is "1000 B" in binary but "1.0 kB" in SI.
    CHECK(formatSize(1000, UnitSystem::Binary) == "1000 B");
    CHECK(formatSize(1000, UnitSystem::SI) == "1.0 kB");
}

TEST_CASE("formatSize: negative sizes carry a sign", "[sizeformat]")
{
    CHECK(formatSize(-512) == "-512 B");
    CHECK(formatSize(-2048) == "-2.0 KiB");
}

TEST_CASE("formatSize: very large magnitudes reach the top unit", "[sizeformat]")
{
    CHECK(formatSize(std::int64_t { 1 } << 50) == "1.0 PiB");
    CHECK(formatSize(std::int64_t { 1 } << 60) == "1.0 EiB");
}

TEST_CASE("formatSize: every binary unit row is reachable", "[sizeformat][tables]")
{
    // Table-completeness: one representative value per row must render with that suffix.
    struct Case
    {
        std::int64_t bytes;
        std::string_view suffix;
    };

    constexpr std::array<Case, 7> Cases { {
        { .bytes = 5, .suffix = "B" },
        { .bytes = std::int64_t { 5 } << 10, .suffix = "KiB" },
        { .bytes = std::int64_t { 5 } << 20, .suffix = "MiB" },
        { .bytes = std::int64_t { 5 } << 30, .suffix = "GiB" },
        { .bytes = std::int64_t { 5 } << 40, .suffix = "TiB" },
        { .bytes = std::int64_t { 5 } << 50, .suffix = "PiB" },
        { .bytes = std::int64_t { 5 } << 60, .suffix = "EiB" },
    } };
    for (auto const& c: Cases)
    {
        auto const text = formatSize(c.bytes, UnitSystem::Binary);
        INFO("bytes=" << c.bytes << " text=" << text);
        CHECK(text.find(c.suffix) != std::string::npos);
    }
}

TEST_CASE("unitSystemFromString: every name maps; unknown is rejected", "[sizeformat][tables]")
{
    CHECK(unitSystemFromString("binary") == UnitSystem::Binary);
    CHECK(unitSystemFromString("si") == UnitSystem::SI);
    CHECK_FALSE(unitSystemFromString("SI").has_value()); // case-sensitive
    CHECK_FALSE(unitSystemFromString("").has_value());
    CHECK_FALSE(unitSystemFromString("metric").has_value());
}
