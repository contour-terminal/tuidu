// SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

#include <tuidu/SortMode.hpp>
#include <tuidu/Tree.hpp>

using namespace tuidu;

namespace
{
/// Tree with three siblings of known size/items/mtime/name for comparator tests.
struct SortFixture
{
    Tree tree;
    NodeId root {}, big {}, mid {}, small {};

    SortFixture()
    {
        root = tree.createRoot("/r");
        // names chosen so name order (a<b<c) differs from size order.
        big = tree.addChild(root, "c_big");
        mid = tree.addChild(root, "b_mid");
        small = tree.addChild(root, "a_small");

        tree.at(big).aggSize = 1000;
        tree.at(big).itemCount = 10;
        tree.at(big).mtime = 300;
        tree.at(mid).aggSize = 500;
        tree.at(mid).itemCount = 5;
        tree.at(mid).mtime = 200;
        tree.at(small).aggSize = 100;
        tree.at(small).itemCount = 1;
        tree.at(small).mtime = 100;
    }

    /// Applies the sort mode at @p index and returns the ordered child ids.
    [[nodiscard]] std::vector<NodeId> ordered(std::size_t index)
    {
        tree.rebuildChildIndex(root, sortModes()[index].less);
        auto const span = tree.childrenOf(root);
        return { span.begin(), span.end() };
    }
};
} // namespace

TEST_CASE("SortMode: every table row produces a total order", "[sortmode][tables]")
{
    // Table-completeness: apply every sort mode; each must order all 3 children.
    SortFixture f;
    for (std::size_t i = 0; i < sortModes().size(); ++i)
    {
        INFO("sort mode: " << sortModes()[i].label);
        auto const order = f.ordered(i);
        CHECK(order.size() == 3);
    }
}

TEST_CASE("SortMode: size descending", "[sortmode]")
{
    SortFixture f;
    auto const order = f.ordered(0); // "Size ↓"
    CHECK(order[0] == f.big);
    CHECK(order[1] == f.mid);
    CHECK(order[2] == f.small);
}

TEST_CASE("SortMode: size ascending", "[sortmode]")
{
    SortFixture f;
    auto const order = f.ordered(1); // "Size ↑"
    CHECK(order[0] == f.small);
    CHECK(order[2] == f.big);
}

TEST_CASE("SortMode: name ascending", "[sortmode]")
{
    SortFixture f;
    auto const order = f.ordered(2); // "Name"
    CHECK(f.tree.name(order[0]) == "a_small");
    CHECK(f.tree.name(order[1]) == "b_mid");
    CHECK(f.tree.name(order[2]) == "c_big");
}

TEST_CASE("SortMode: items descending", "[sortmode]")
{
    SortFixture f;
    auto const order = f.ordered(3); // "Items ↓"
    CHECK(order[0] == f.big);
    CHECK(order[2] == f.small);
}

TEST_CASE("SortMode: mtime descending (newest first)", "[sortmode]")
{
    SortFixture f;
    auto const order = f.ordered(4); // "Date ↓"
    CHECK(order[0] == f.big);        // mtime 300
    CHECK(order[2] == f.small);      // mtime 100
}

TEST_CASE("SortMode: equal sizes tie-break by name", "[sortmode]")
{
    Tree t;
    auto root = t.createRoot("/r");
    auto b = t.addChild(root, "b");
    auto a = t.addChild(root, "a");
    t.at(a).aggSize = 100;
    t.at(b).aggSize = 100;
    t.rebuildChildIndex(root, sortModes()[0].less); // size desc, name tie-break
    auto kids = t.childrenOf(root);
    CHECK(t.name(kids[0]) == "a");
    CHECK(t.name(kids[1]) == "b");
}

TEST_CASE("nextSortMode: selecting a new action jumps to its first row", "[sortmode]")
{
    // From "Size ↓" (0), pressing SortByName selects "Name" (2).
    CHECK(nextSortMode(0, Action::SortByName) == 2);
    // From "Name" (2), pressing SortByItems selects "Items ↓" (3).
    CHECK(nextSortMode(2, Action::SortByItems) == 3);
}

TEST_CASE("nextSortMode: re-pressing cycles within the same action", "[sortmode]")
{
    // "Size ↓"(0) and "Size ↑"(1) share SortBySize: repeated press toggles them.
    CHECK(nextSortMode(0, Action::SortBySize) == 1);
    CHECK(nextSortMode(1, Action::SortBySize) == 0);
}

TEST_CASE("nextSortMode: single-row action stays put on repeat", "[sortmode]")
{
    // "Name"(2) is the only SortByName row; repeating keeps it.
    CHECK(nextSortMode(2, Action::SortByName) == 2);
}

TEST_CASE("nextSortMode: non-sort action leaves selection unchanged", "[sortmode]")
{
    CHECK(nextSortMode(1, Action::Quit) == 1);
}
