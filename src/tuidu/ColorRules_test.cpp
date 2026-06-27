// SPDX-License-Identifier: Apache-2.0
#include <tui/Theme.hpp>

#include <catch2/catch_test_macros.hpp>

#include <tuidu/ColorRules.hpp>
#include <tuidu/Node.hpp>
#include <tuidu/Tree.hpp>

using namespace tuidu;

namespace
{
constexpr double Large = 0.20;
constexpr double Huge = 0.50;

/// Distinct palette so each slot is identifiable by color in assertions.
[[nodiscard]] tui::ColorPalette markedPalette()
{
    tui::ColorPalette p {};
    p.error = tui::RgbColor { .r = 1, .g = 0, .b = 0 };
    p.warning = tui::RgbColor { .r = 2, .g = 0, .b = 0 };
    p.accent = tui::RgbColor { .r = 3, .g = 0, .b = 0 };
    p.primary = tui::RgbColor { .r = 4, .g = 0, .b = 0 };
    p.text = tui::RgbColor { .r = 5, .g = 0, .b = 0 };
    return p;
}

struct Fix
{
    Tree tree;
    NodeId root {}, node {};

    Fix()
    {
        root = tree.createRoot("/r");
        node = tree.addChild(root, "x");
    }

    [[nodiscard]] ColorContext ctx(double percent) const
    {
        return ColorContext { .tree = tree,
                              .node = node,
                              .percentOfParent = percent,
                              .largeThreshold = Large,
                              .hugeThreshold = Huge };
    }
};
} // namespace

TEST_CASE("ColorRules: every rule is reachable by some input", "[colorrules][tables]")
{
    // Table-completeness: drive an input that makes each rule the first match.
    auto const palette = markedPalette();

    SECTION("read error -> error slot")
    {
        Fix f;
        f.tree.at(f.node).flags |= NodeFlag::ReadError;
        CHECK(resolveColor(f.ctx(0.0), palette).color == palette.error);
    }
    SECTION("huge share -> warning slot, bold")
    {
        Fix f;
        auto const r = resolveColor(f.ctx(0.60), palette);
        CHECK(r.color == palette.warning);
        CHECK(r.bold);
    }
    SECTION("large share -> accent slot")
    {
        Fix f;
        CHECK(resolveColor(f.ctx(0.30), palette).color == palette.accent);
    }
    SECTION("directory -> primary slot, bold")
    {
        Fix f;
        f.tree.at(f.node).flags |= NodeFlag::IsDir;
        auto const r = resolveColor(f.ctx(0.05), palette);
        CHECK(r.color == palette.primary);
        CHECK(r.bold);
    }
    SECTION("default -> text slot")
    {
        Fix f;
        CHECK(resolveColor(f.ctx(0.05), palette).color == palette.text);
    }
}

TEST_CASE("ColorRules: earlier rule wins over a later one", "[colorrules]")
{
    // A read-error directory at a huge share still resolves to error (rule 0 first).
    auto const palette = markedPalette();
    Fix f;
    f.tree.at(f.node).flags |= NodeFlag::IsDir;
    f.tree.at(f.node).flags |= NodeFlag::ReadError;
    CHECK(resolveColor(f.ctx(0.99), palette).color == palette.error);
}

TEST_CASE("ColorRules: table ends with a catch-all", "[colorrules]")
{
    // The last rule must always match so resolveColor never falls through.
    auto const rules = colorRules();
    REQUIRE_FALSE(rules.empty());
    ColorContext anything { .tree = Tree {}, // unused by the catch-all predicate
                            .node = 0,
                            .percentOfParent = 0.0,
                            .largeThreshold = Large,
                            .hugeThreshold = Huge };
    CHECK(rules.back().match(anything));
}
