// SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include <tuidu/Tree.hpp>

using namespace tuidu;

namespace
{
/// Builds a small fixture tree:
///   root("/r")
///     ├── a.txt
///     ├── sub/
///     │     ├── x.txt
///     │     └── y.txt
///     └── b.txt
/// Returns the ids for assertions.
struct Fixture
{
    Tree tree;
    NodeId root {}, a {}, sub {}, x {}, y {}, b {};

    Fixture()
    {
        root = tree.createRoot("/r");
        a = tree.addChild(root, "a.txt");
        sub = tree.addChild(root, "sub");
        // children of root must be contiguous, so add sub's children only after root's.
        b = tree.addChild(root, "b.txt");
        x = tree.addChild(sub, "x.txt");
        y = tree.addChild(sub, "y.txt");
    }
};
} // namespace

TEST_CASE("Node flags: bit operations", "[tree][node]")
{
    auto f = NodeFlag::IsDir | NodeFlag::ReadError;
    CHECK(hasFlag(f, NodeFlag::IsDir));
    CHECK(hasFlag(f, NodeFlag::ReadError));
    CHECK_FALSE(hasFlag(f, NodeFlag::IsSymlink));

    f &= ~NodeFlag::IsDir;
    CHECK_FALSE(hasFlag(f, NodeFlag::IsDir));
    CHECK(hasFlag(f, NodeFlag::ReadError));
}

TEST_CASE("Node convenience predicates reflect flags", "[tree][node]")
{
    Node n;
    n.flags = NodeFlag::IsDir | NodeFlag::IsSymlink;
    CHECK(n.isDir());
    CHECK(n.isSymlink());
    CHECK_FALSE(n.hasReadError());
}

TEST_CASE("Tree: createRoot establishes the root", "[tree]")
{
    Tree tree;
    auto root = tree.createRoot("/home/user");
    CHECK(tree.root() == root);
    CHECK(tree.size() == 1);
    CHECK(tree.name(root) == "/home/user");
    CHECK(tree[root].parent == InvalidNode);
}

TEST_CASE("Tree: addChild links parent and child", "[tree]")
{
    Fixture f;
    CHECK(f.tree[f.a].parent == f.root);
    CHECK(f.tree[f.sub].parent == f.root);
    CHECK(f.tree[f.x].parent == f.sub);
    CHECK(f.tree.name(f.a) == "a.txt");
    CHECK(f.tree.name(f.x) == "x.txt");

    // root has 3 contiguous children: a, sub, b
    CHECK(f.tree[f.root].childCount == 3);
    CHECK(f.tree[f.root].firstChild == f.a);
    // sub has 2: x, y
    CHECK(f.tree[f.sub].childCount == 2);
    CHECK(f.tree[f.sub].firstChild == f.x);
}

TEST_CASE("Tree: name arena slices correctly across many entries", "[tree]")
{
    Tree tree;
    auto root = tree.createRoot("/r");
    auto first = tree.addChild(root, "alpha");
    auto second = tree.addChild(root, "beta-longer-name");
    auto third = tree.addChild(root, "γ");
    CHECK(tree.name(first) == "alpha");
    CHECK(tree.name(second) == "beta-longer-name");
    CHECK(tree.name(third) == "γ");
}

TEST_CASE("Tree: fullPath walks to the root", "[tree]")
{
    Fixture f;
    CHECK(f.tree.fullPath(f.root) == "/r");
    CHECK(f.tree.fullPath(f.a) == "/r/a.txt");
    CHECK(f.tree.fullPath(f.x) == "/r/sub/x.txt");
}

TEST_CASE("Tree: childrenOf is empty before sorting, populated after", "[tree]")
{
    Fixture f;
    CHECK(f.tree.childrenOf(f.root).empty());

    auto byNameAsc = [](Tree const& t, NodeId lhs, NodeId rhs) {
        return t.name(lhs) < t.name(rhs);
    };
    f.tree.rebuildChildIndex(f.root, byNameAsc);

    auto kids = f.tree.childrenOf(f.root);
    REQUIRE(kids.size() == 3);
    CHECK(f.tree.name(kids[0]) == "a.txt");
    CHECK(f.tree.name(kids[1]) == "b.txt");
    CHECK(f.tree.name(kids[2]) == "sub");
}

TEST_CASE("Tree: rebuildChildIndex re-sorts without touching the arena", "[tree]")
{
    Fixture f;
    auto byNameAsc = [](Tree const& t, NodeId lhs, NodeId rhs) {
        return t.name(lhs) < t.name(rhs);
    };
    auto byNameDesc = [](Tree const& t, NodeId lhs, NodeId rhs) {
        return t.name(lhs) > t.name(rhs);
    };

    f.tree.rebuildChildIndex(f.root, byNameAsc);
    CHECK(f.tree.name(f.tree.childrenOf(f.root).front()) == "a.txt");

    f.tree.rebuildChildIndex(f.root, byNameDesc);
    auto kids = f.tree.childrenOf(f.root);
    REQUIRE(kids.size() == 3);
    CHECK(f.tree.name(kids[0]) == "sub");
    CHECK(f.tree.name(kids[2]) == "a.txt");

    // The node arena is untouched: parent/child links remain valid.
    CHECK(f.tree[f.a].parent == f.root);
    CHECK(f.tree[f.sub].childCount == 2);
}

TEST_CASE("Tree: sorting multiple directories keeps slices independent", "[tree]")
{
    Fixture f;
    auto byNameAsc = [](Tree const& t, NodeId lhs, NodeId rhs) {
        return t.name(lhs) < t.name(rhs);
    };

    f.tree.rebuildChildIndex(f.root, byNameAsc);
    f.tree.rebuildChildIndex(f.sub, byNameAsc);

    auto rootKids = f.tree.childrenOf(f.root);
    auto subKids = f.tree.childrenOf(f.sub);
    REQUIRE(rootKids.size() == 3);
    REQUIRE(subKids.size() == 2);
    CHECK(f.tree.name(subKids[0]) == "x.txt");
    CHECK(f.tree.name(subKids[1]) == "y.txt");
    // root slice still intact after sub was sorted
    CHECK(f.tree.name(rootKids[0]) == "a.txt");
}

TEST_CASE("Tree: propagateUp rolls aggregates to the root", "[tree]")
{
    Fixture f;
    // Simulate the scanner: leaf x has 100 bytes / 8 blocks*512, leaf y has 50/512.
    f.tree.at(f.x).ownSize = 100;
    f.tree.propagateUp(f.x, 100, 4096, 0);
    f.tree.at(f.y).ownSize = 50;
    f.tree.propagateUp(f.y, 50, 512, 0);
    f.tree.at(f.a).ownSize = 10;
    f.tree.propagateUp(f.a, 10, 512, 0);

    CHECK(f.tree[f.sub].aggSize == 150);
    CHECK(f.tree[f.sub].aggBlocks == 4608);
    CHECK(f.tree[f.root].aggSize == 160);
    CHECK(f.tree[f.root].aggBlocks == 5120);
}

TEST_CASE("Tree: arena growth does not invalidate NodeIds", "[tree]")
{
    Tree tree;
    auto root = tree.createRoot("/r");
    // Add enough children to force several reallocations.
    std::vector<NodeId> ids;
    ids.reserve(1000);
    for (int i = 0; i < 1000; ++i)
        ids.push_back(tree.addChild(root, "child"));

    CHECK(tree.size() == 1001);
    CHECK(tree[root].childCount == 1000);
    // First and last child ids still resolve and point at root.
    CHECK(tree[ids.front()].parent == root);
    CHECK(tree[ids.back()].parent == root);
}
