// SPDX-License-Identifier: Apache-2.0
#include <tui/Theme.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

#include <tuidu/Action.hpp>
#include <tuidu/DiskUsageModel.hpp>
#include <tuidu/Tree.hpp>

using namespace tuidu;

namespace
{
/// Builds a small populated tree:
///   /r  (agg 300)
///     ├── big/  (dir, agg 200, 3 items)
///     │     └── inner.txt (100)
///     └── small.txt (100)
struct ModelFixture
{
    Tree tree;
    NodeId root {}, big {}, inner {}, small {};

    ModelFixture()
    {
        root = tree.createRoot("/r");
        big = tree.addChild(root, "big");
        small = tree.addChild(root, "small.txt");
        inner = tree.addChild(big, "inner.txt");

        tree.at(big).flags = NodeFlag::IsDir;
        tree.at(big).aggSize = 200;
        tree.at(big).itemCount = 2;
        tree.at(small).aggSize = 100;
        tree.at(inner).aggSize = 100;
        tree.at(root).aggSize = 300;
        tree.at(root).itemCount = 4;
    }
};
} // namespace

TEST_CASE("DiskUsageModel: rows are the current directory's children", "[model]")
{
    ModelFixture f;
    DiskUsageModel model(f.tree);
    auto const rows = model.rows();
    REQUIRE(rows.size() == 2); // big, small.txt
}

TEST_CASE("DiskUsageModel: columns mirror the tuidu column table", "[model]")
{
    ModelFixture f;
    DiskUsageModel model(f.tree);
    CHECK(model.columns().size() == columns().size());
}

TEST_CASE("DiskUsageModel: cellText delegates to the column formatters", "[model]")
{
    ModelFixture f;
    DiskUsageModel model(f.tree);

    // Default sort is "Size ↓": big (200) before small (100).
    auto const rows = model.rows();
    REQUIRE(rows.size() == 2);
    // The name column is the last column; the dir gets a trailing slash.
    auto const nameCol = columns().size() - 1;
    CHECK(model.cellText(rows[0], nameCol) == "big/");
    CHECK(model.cellText(rows[1], nameCol) == "small.txt");
}

TEST_CASE("DiskUsageModel: descend and ascend change the current node", "[model]")
{
    ModelFixture f;
    DiskUsageModel model(f.tree);

    auto const rows = model.rows();
    auto const bigRow = static_cast<tui::RowId>(f.big);
    REQUIRE(model.canDescend(bigRow));
    REQUIRE(model.descend(bigRow));
    CHECK(model.currentDir() == f.big);
    CHECK(model.rows().size() == 1); // inner.txt

    REQUIRE(model.ascend());
    CHECK(model.currentDir() == f.root);
}

TEST_CASE("DiskUsageModel: cannot descend into a file", "[model]")
{
    ModelFixture f;
    DiskUsageModel model(f.tree);
    CHECK_FALSE(model.canDescend(static_cast<tui::RowId>(f.small)));
    CHECK_FALSE(model.descend(static_cast<tui::RowId>(f.small)));
}

TEST_CASE("DiskUsageModel: sortBy reorders the rows", "[model]")
{
    ModelFixture f;
    DiskUsageModel model(f.tree);

    // Default "Size ↓": big first. Sorting by name puts "big" before "small.txt" too,
    // so switch to name then verify order is alphabetical and stable.
    model.sortBy(static_cast<int>(Action::SortByName));
    auto const rows = model.rows();
    REQUIRE(rows.size() == 2);
    CHECK(f.tree.name(static_cast<NodeId>(rows[0])) == "big");
    CHECK(f.tree.name(static_cast<NodeId>(rows[1])) == "small.txt");
}

TEST_CASE("DiskUsageModel: size mode switches apparent vs disk in cell text", "[model]")
{
    ModelFixture f;
    f.tree.at(f.small).aggBlocks = 4096; // distinct from aggSize 100
    DiskUsageModel model(f.tree);

    auto const smallRow = static_cast<tui::RowId>(f.small);
    model.setSizeMode(SizeMode::Apparent);
    auto const apparent = model.cellText(smallRow, 0); // size column
    model.setSizeMode(SizeMode::Disk);
    auto const disk = model.cellText(smallRow, 0);
    CHECK(apparent != disk);
}

TEST_CASE("DiskUsageModel: currentTitle is the node's full path", "[model]")
{
    ModelFixture f;
    DiskUsageModel model(f.tree);
    CHECK(model.currentTitle() == "/r");
    model.descend(static_cast<tui::RowId>(f.big));
    CHECK(model.currentTitle() == "/r/big");
}
