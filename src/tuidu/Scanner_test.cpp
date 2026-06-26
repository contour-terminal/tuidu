// SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include <coro/Cancellation.hpp>
#include <platform/testing/MockFileInfoProvider.hpp>
#include <tuidu/Scanner.hpp>
#include <tuidu/Tree.hpp>

using namespace tuidu;
using endo::platform::FileEntry;
using endo::platform::testing::MockFileInfoProvider;

namespace
{
/// Drives the scan task to completion synchronously (the scan only awaits sub-scans,
/// so a single resume runs the whole walk). Optionally seeds a stop token.
void runScan(Scanner& scanner, NodeId root, Scanner::ProgressSink sink = {}, endo::coro::StopToken token = {})
{
    auto task = scanner.scan(root, std::move(sink));
    task.handle().promise().setStopToken(std::move(token));
    task.handle().resume();
    task.result(); // surface any body exception (none expected in these cases)
}

/// A regular file entry with the given name/size, on device @p dev.
[[nodiscard]] FileEntry file(std::string name,
                             std::int64_t size,
                             std::uint64_t dev = 1,
                             std::uint64_t ino = 0)
{
    return FileEntry { .name = std::move(name),
                       .size = size,
                       .blocks = (size + 511) / 512,
                       .mode = 0644,
                       .mtime = 100,
                       .dev = dev,
                       .ino = ino,
                       .isDir = false,
                       .isSymlink = false };
}

/// A directory entry with the given name, on device @p dev.
[[nodiscard]] FileEntry dir(std::string name, std::uint64_t dev = 1, std::uint64_t ino = 0)
{
    return FileEntry { .name = std::move(name),
                       .size = 0,
                       .blocks = 0,
                       .mode = 0755,
                       .mtime = 100,
                       .dev = dev,
                       .ino = ino,
                       .isDir = true,
                       .isSymlink = false };
}
} // namespace

TEST_CASE("Scanner: aggregates sizes up the tree", "[scanner]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 100), dir("sub") });
    provider.setEntries("/r/sub", { file("x.txt", 50), file("y.txt", 30) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;
    Scanner scanner(provider, tree, ScanOptions {});
    runScan(scanner, root);

    // root subtree = a(100) + sub(x50 + y30) = 180.
    CHECK(tree[root].aggSize == 180);
    // sub subtree = 80.
    auto kids = tree.childrenOf(root);
    // childrenOf needs a sort first; instead find children by walking firstChild range.
    auto const a = tree[root].firstChild; // a.txt
    auto const sub = a + 1;               // sub
    CHECK(tree.name(a) == "a.txt");
    CHECK(tree.name(sub) == "sub");
    CHECK(tree[sub].aggSize == 80);
    (void) kids;
}

TEST_CASE("Scanner: item counts include all descendants", "[scanner]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 1), dir("sub") });
    provider.setEntries("/r/sub", { file("x.txt", 1), file("y.txt", 1) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;
    Scanner scanner(provider, tree, ScanOptions {});
    runScan(scanner, root);

    // root counts itself (1) + a + sub + x + y = 5.
    CHECK(tree[root].itemCount == 5);
}

TEST_CASE("Scanner: disk usage uses blocks, not apparent size", "[scanner]")
{
    MockFileInfoProvider provider;
    // A 100-byte file occupies 1 block (512 bytes) of disk.
    provider.setEntries("/r", { file("a.txt", 100) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;
    Scanner scanner(provider, tree, ScanOptions {});
    runScan(scanner, root);

    CHECK(tree[root].aggSize == 100);   // apparent
    CHECK(tree[root].aggBlocks == 512); // 1 block * 512
}

TEST_CASE("Scanner: cross-device child is flagged and excluded", "[scanner]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("local.txt", 100, /*dev=*/1), dir("mnt", /*dev=*/2) });
    provider.setEntries("/r/mnt", { file("big.txt", 9999, /*dev=*/2) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;
    Scanner scanner(provider, tree, ScanOptions { .crossDevices = false });
    runScan(scanner, root);

    auto const localFile = tree[root].firstChild;
    auto const mnt = localFile + 1;
    CHECK(tree.name(mnt) == "mnt");
    CHECK(tree[mnt].flags == (tree[mnt].flags | NodeFlag::OtherDevice));
    // mnt subtree excluded: root aggregate is only the local file.
    CHECK(tree[root].aggSize == 100);
}

TEST_CASE("Scanner: crossDevices=true traverses other devices", "[scanner]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { dir("mnt", /*dev=*/2) });
    provider.setEntries("/r/mnt", { file("big.txt", 500, /*dev=*/2) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;
    Scanner scanner(provider, tree, ScanOptions { .crossDevices = true });
    runScan(scanner, root);

    CHECK(tree[root].aggSize == 500);
}

TEST_CASE("Scanner: hardlink duplicate counts size once", "[scanner]")
{
    MockFileInfoProvider provider;
    // Two entries with the same (dev,ino) — a hardlink.
    provider.setEntries("/r",
                        { file("a", 100, /*dev=*/1, /*ino=*/42), file("b", 100, /*dev=*/1, /*ino=*/42) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;
    Scanner scanner(provider, tree, ScanOptions { .dedupeHardlinks = true });
    runScan(scanner, root);

    // Only the first link contributes 100; the second is deduped.
    CHECK(tree[root].aggSize == 100);
    auto const second = tree[root].firstChild + 1;
    CHECK(tree[second].flags == (tree[second].flags | NodeFlag::HardlinkDup));
}

TEST_CASE("Scanner: dedupeHardlinks=false counts both links", "[scanner]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a", 100, 1, 42), file("b", 100, 1, 42) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;
    Scanner scanner(provider, tree, ScanOptions { .dedupeHardlinks = false });
    runScan(scanner, root);

    CHECK(tree[root].aggSize == 200);
}

TEST_CASE("Scanner: symlink is not traversed by default", "[scanner]")
{
    MockFileInfoProvider provider;
    // A symlink that points at a directory; its target listing must be ignored.
    auto link = dir("link");
    link.isSymlink = true;
    provider.setEntries("/r", { std::vector<FileEntry> { link }[0] });
    provider.setEntries("/r/link", { file("hidden.txt", 9999) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;
    Scanner scanner(provider, tree, ScanOptions { .followSymlinks = false });
    runScan(scanner, root);

    auto const linkNode = tree[root].firstChild;
    CHECK(tree[linkNode].isSymlink());
    // Not traversed: the target's 9999-byte file is not counted.
    CHECK(tree[root].aggSize == 0);
}

TEST_CASE("Scanner: empty directory yields a zero aggregate", "[scanner]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", {});

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;
    Scanner scanner(provider, tree, ScanOptions {});
    runScan(scanner, root);

    CHECK(tree[root].aggSize == 0);
    CHECK(tree[root].itemCount == 1); // just itself
}

TEST_CASE("Scanner: progress sink receives a final done message", "[scanner]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 10) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;
    Scanner scanner(provider, tree, ScanOptions {});

    std::vector<ScanProgress> messages;
    runScan(scanner, root, [&](ScanProgress const& p) { messages.push_back(p); });

    REQUIRE_FALSE(messages.empty());
    CHECK(messages.back().done);
}

TEST_CASE("Scanner: cancellation throws OperationCancelled", "[scanner]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 10), file("b.txt", 10) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;
    Scanner scanner(provider, tree, ScanOptions {});

    endo::coro::StopSource source;
    source.request_stop(); // already cancelled before the first entry

    auto task = scanner.scan(root, {});
    task.handle().promise().setStopToken(source.get_token());
    task.handle().resume();
    // The body throws OperationCancelled; the root Task captures it and rethrows on result().
    CHECK_THROWS_AS(task.result(), endo::coro::OperationCancelled);
}
