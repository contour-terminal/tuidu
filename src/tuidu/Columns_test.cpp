// SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

#include <tuidu/Columns.hpp>
#include <tuidu/Node.hpp>
#include <tuidu/Tree.hpp>

using namespace tuidu;

namespace
{
/// A root with one directory child and one file child, with known sizes.
struct ColFixture
{
    Tree tree;
    NodeId root {}, dir {}, file {};

    ColFixture()
    {
        root = tree.createRoot("/r");
        dir = tree.addChild(root, "subdir");
        file = tree.addChild(root, "data.bin");

        tree.at(dir).flags = NodeFlag::IsDir;
        tree.at(dir).aggSize = 2048;
        tree.at(dir).aggBlocks = 4096;
        tree.at(dir).itemCount = 7;
        tree.at(dir).mtime = 1'700'000'000; // a fixed epoch

        tree.at(file).aggSize = 1024;
        tree.at(file).aggBlocks = 1536;
        tree.at(file).itemCount = 1;

        tree.at(root).aggSize = 4096; // parent total (>= children)
        tree.at(root).aggBlocks = 8192;
    }

    [[nodiscard]] RenderCtx ctxFor(NodeId node, SizeMode mode = SizeMode::Apparent) const
    {
        return RenderCtx { .tree = tree,
                           .node = node,
                           .parent = tree[node].parent,
                           .sizeMode = mode,
                           .units = UnitSystem::Binary,
                           .barWidth = 10 };
    }
};
} // namespace

TEST_CASE("Columns: every column formats dir, file, and error nodes", "[columns][tables]")
{
    // Table-completeness: each column's formatter must produce output for each node kind
    // (directory, file) without throwing or returning garbage.
    ColFixture f;
    auto markError = f.tree.at(f.dir).flags |= NodeFlag::ReadError; // also exercise error flag
    (void) markError;

    for (auto const& col: columns())
    {
        for (auto const node: { f.dir, f.file })
        {
            auto const text = col.format(f.ctxFor(node));
            INFO("column '" << col.header << "'");
            CHECK_FALSE(text.empty()); // every formatter yields something (bar has width>0)
        }
    }
}

TEST_CASE("Columns: fmtSize respects size mode and units", "[columns]")
{
    ColFixture f;
    CHECK(fmtSize(f.ctxFor(f.file, SizeMode::Apparent)) == "1.0 KiB"); // aggSize 1024
    CHECK(fmtSize(f.ctxFor(f.file, SizeMode::Disk)) == "1.5 KiB");     // aggBlocks 1536
}

TEST_CASE("Columns: fmtPercent is node size over parent", "[columns]")
{
    ColFixture f;
    // file aggSize 1024 of parent 4096 = 25%.
    CHECK(fmtPercent(f.ctxFor(f.file)) == " 25%");
    // dir aggSize 2048 of parent 4096 = 50%.
    CHECK(fmtPercent(f.ctxFor(f.dir)) == " 50%");
}

TEST_CASE("Columns: fmtBar fills proportionally", "[columns]")
{
    ColFixture f;
    // dir is 50% of parent over a 10-wide bar -> 5 filled.
    auto const bar = fmtBar(f.ctxFor(f.dir));
    REQUIRE(bar.size() == 10);
    CHECK(std::ranges::count(bar, '#') == 5);
}

TEST_CASE("Columns: fmtItems prints subtree count", "[columns]")
{
    ColFixture f;
    CHECK(fmtItems(f.ctxFor(f.dir)) == "7");
    CHECK(fmtItems(f.ctxFor(f.file)) == "1");
}

TEST_CASE("Columns: fmtName marks directories with a trailing slash", "[columns]")
{
    ColFixture f;
    CHECK(fmtName(f.ctxFor(f.dir)) == "subdir/");
    CHECK(fmtName(f.ctxFor(f.file)) == "data.bin");
}

TEST_CASE("Columns: fmtMtime renders a YYYY-MM-DD HH:MM stamp", "[columns]")
{
    ColFixture f;
    auto const text = fmtMtime(f.ctxFor(f.dir));
    // Format shape: 16 chars "YYYY-MM-DD HH:MM".
    CHECK(text.size() == 16);
    CHECK(text[4] == '-');
    CHECK(text[7] == '-');
    CHECK(text[10] == ' ');
    CHECK(text[13] == ':');
}

TEST_CASE("sizeModeFromString: every name maps; unknown is rejected", "[columns][tables]")
{
    CHECK(sizeModeFromString("apparent") == SizeMode::Apparent);
    CHECK(sizeModeFromString("disk") == SizeMode::Disk);
    CHECK_FALSE(sizeModeFromString("Disk").has_value()); // case-sensitive
    CHECK_FALSE(sizeModeFromString("blocks").has_value());
}
