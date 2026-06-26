// SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <platform/MessageQueue.hpp>
#include <platform/testing/MockFileInfoProvider.hpp>
#include <tuidu/ScanWorker.hpp>
#include <tuidu/Tree.hpp>

using namespace tuidu;
using endo::platform::FileEntry;
using endo::platform::MessageQueue;
using endo::platform::testing::MockFileInfoProvider;

namespace
{
[[nodiscard]] FileEntry file(std::string name, std::int64_t size)
{
    return FileEntry { .name = std::move(name),
                       .size = size,
                       .blocks = (size + 511) / 512,
                       .mode = 0644,
                       .mtime = 100,
                       .dev = 1,
                       .ino = 0,
                       .isDir = false,
                       .isSymlink = false };
}

/// Drains the queue until a `done` message is seen or the timeout elapses.
/// @return All messages drained (the last is `done` on success).
[[nodiscard]] std::vector<ScanProgress> waitForDone(MessageQueue<ScanProgress>& queue)
{
    std::vector<ScanProgress> all;
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline)
    {
        std::vector<ScanProgress> batch;
        queue.drainTo(batch);
        for (auto& m: batch)
        {
            auto const done = m.done;
            all.push_back(std::move(m));
            if (done)
                return all;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return all;
}
} // namespace

TEST_CASE("ScanWorker: scans on a background thread and reports done", "[scanworker]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 100), file("b.txt", 200) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;

    MessageQueue<ScanProgress> queue;
    std::mutex treeMutex;
    ScanWorker worker(provider, tree, queue, ScanOptions {}, treeMutex);
    worker.start(root);

    auto const messages = waitForDone(queue);
    REQUIRE_FALSE(messages.empty());
    CHECK(messages.back().done);

    // The shared tree is fully populated once the done message arrives.
    CHECK(tree[root].aggSize == 300);
    CHECK(tree[root].itemCount == 3);
}

TEST_CASE("ScanWorker: requestStop cancels and still emits done", "[scanworker]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 1) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;

    MessageQueue<ScanProgress> queue;
    std::mutex treeMutex;
    ScanWorker worker(provider, tree, queue, ScanOptions {}, treeMutex);
    worker.requestStop(); // cancel before starting
    worker.start(root);

    auto const messages = waitForDone(queue);
    REQUIRE_FALSE(messages.empty());
    CHECK(messages.back().done);
}

TEST_CASE("ScanWorker: destructor joins a running worker", "[scanworker]")
{
    MockFileInfoProvider provider;
    provider.setEntries("/r", { file("a.txt", 1) });

    Tree tree;
    auto root = tree.createRoot("/r");
    tree.at(root).dev = 1;

    MessageQueue<ScanProgress> queue;
    {
        std::mutex treeMutex;
        ScanWorker worker(provider, tree, queue, ScanOptions {}, treeMutex);
        worker.start(root);
        // Worker goes out of scope here; the destructor must stop+join without hanging.
    }
    SUCCEED("ScanWorker destructor returned");
}
