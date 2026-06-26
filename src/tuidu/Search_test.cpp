// SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <tuidu/Search.hpp>
#include <tuidu/Tree.hpp>

using namespace tuidu;

namespace
{
/// Builds a tree of named children under a root and returns (tree, child ids).
struct NamedTree
{
    Tree tree;
    NodeId root {};
    std::vector<NodeId> ids;

    explicit NamedTree(std::vector<std::string> const& names)
    {
        root = tree.createRoot("/r");
        for (auto const& n: names)
            ids.push_back(tree.addChild(root, n));
    }

    [[nodiscard]] std::span<NodeId const> children() const { return ids; }

    [[nodiscard]] std::vector<std::string> rankedNames(std::string_view query) const
    {
        auto const scored = rankMatches(tree, children(), query);
        std::vector<std::string> out;
        out.reserve(scored.size());
        for (auto const& s: scored)
            out.emplace_back(tree.name(s.node));
        return out;
    }
};
} // namespace

TEST_CASE("Search: empty query returns all in original order", "[search]")
{
    NamedTree t({ "banana", "apple", "cherry" });
    auto const names = t.rankedNames("");
    REQUIRE(names.size() == 3);
    CHECK(names[0] == "banana");
    CHECK(names[1] == "apple");
    CHECK(names[2] == "cherry");
}

TEST_CASE("Search: contiguous substring outranks scattered fuzzy", "[search]")
{
    // "abc" is a substring of "xabcx" but only a scattered match in "a_b_c".
    NamedTree t({ "a_b_c", "xabcx" });
    auto const names = t.rankedNames("abc");
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "xabcx"); // substring wins
    CHECK(names[1] == "a_b_c"); // fuzzy second
}

TEST_CASE("Search: among substrings, earlier position wins", "[search]")
{
    NamedTree t({ "xxfoo", "fooxx" });
    auto const names = t.rankedNames("foo");
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "fooxx"); // match at position 0
    CHECK(names[1] == "xxfoo"); // match at position 2
}

TEST_CASE("Search: among equal-position substrings, shorter name wins", "[search]")
{
    NamedTree t({ "foobar", "foo" });
    auto const names = t.rankedNames("foo");
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "foo"); // both match at 0; shorter first
    CHECK(names[1] == "foobar");
}

TEST_CASE("Search: smart case — lowercase query is case-insensitive", "[search]")
{
    NamedTree t({ "Downloads", "documents" });
    auto const names = t.rankedNames("do");
    // Both contain "do"/"Do" case-insensitively.
    REQUIRE(names.size() == 2);
}

TEST_CASE("Search: smart case — uppercase query is case-sensitive", "[search]")
{
    NamedTree t({ "Downloads", "downloads" });
    auto const names = t.rankedNames("Down");
    // Only "Downloads" matches the case-sensitive "Down" as a substring.
    REQUIRE(names.size() == 1);
    CHECK(names[0] == "Downloads");
}

TEST_CASE("Search: no match yields empty result", "[search]")
{
    NamedTree t({ "alpha", "beta" });
    auto const names = t.rankedNames("zzzz");
    CHECK(names.empty());
}

TEST_CASE("Search: fuzzy matches are found when no substring exists", "[search]")
{
    // "dcm" is not a contiguous substring of "Documents" but is a fuzzy match (D.c.m...).
    NamedTree t({ "Documents" });
    auto const scored = rankMatches(t.tree, t.children(), "dcm");
    REQUIRE(scored.size() == 1);
    CHECK_FALSE(scored[0].substring);
}
