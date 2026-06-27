// SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>
#include <vector>

#include <platform/MessageQueue.hpp>
#include <platform/testing/InMemoryFileSystem.hpp>
#include <tuidu/DeleteProgress.hpp>
#include <tuidu/DeleteWorker.hpp>

using namespace tuidu;
using endo::platform::MessageQueue;
using endo::platform::testing::InMemoryFileSystem;

namespace
{

/// Drains the queue until a `done` message is seen or the timeout elapses.
[[nodiscard]] std::vector<DeleteProgress> waitForDone(MessageQueue<DeleteProgress>& queue)
{
    std::vector<DeleteProgress> all;
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline)
    {
        std::vector<DeleteProgress> batch;
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

TEST_CASE("DeleteWorker: removes a whole directory tree and reports done", "[deleteworker]")
{
    InMemoryFileSystem fs;
    fs.addDirectory("/root");
    fs.addDirectory("/root/sub");
    fs.addFile("/root/sub/x.txt", "xxxx");
    fs.addFile("/root/sub/y.txt", "yy");
    fs.addFile("/root/a.txt", "a");
    // 4 entries under /root (sub, x, y, a) + /root itself = 5.
    constexpr std::uint64_t Total = 5;

    MessageQueue<DeleteProgress> queue;
    {
        DeleteWorker worker { fs, queue };
        worker.start("/root", Total, DeleteMode::Recursive);

        auto const messages = waitForDone(queue);
        REQUIRE_FALSE(messages.empty());
        auto const& last = messages.back();
        CHECK(last.done);
        CHECK_FALSE(last.cancelled);
        CHECK_FALSE(last.error.has_value());
        CHECK(last.deleted == Total);
    }

    CHECK_FALSE(fs.exists("/root"));
    CHECK_FALSE(fs.exists("/root/sub/x.txt"));
}

TEST_CASE("DeleteWorker: removes a single file", "[deleteworker]")
{
    InMemoryFileSystem fs;
    fs.addFile("/lonely.txt", "data");

    MessageQueue<DeleteProgress> queue;
    {
        DeleteWorker worker { fs, queue };
        worker.start("/lonely.txt", 1, DeleteMode::SingleEntry);
        auto const messages = waitForDone(queue);
        REQUIRE_FALSE(messages.empty());
        CHECK(messages.back().done);
        CHECK(messages.back().deleted == 1);
        CHECK_FALSE(messages.back().error.has_value());
    }
    CHECK_FALSE(fs.exists("/lonely.txt"));
}

TEST_CASE("DeleteWorker: reports an error when the target does not exist", "[deleteworker]")
{
    InMemoryFileSystem fs;
    MessageQueue<DeleteProgress> queue;
    {
        DeleteWorker worker { fs, queue };
        worker.start("/does/not/exist", 1, DeleteMode::SingleEntry);
        auto const messages = waitForDone(queue);
        REQUIRE_FALSE(messages.empty());
        CHECK(messages.back().done);
        CHECK(messages.back().error.has_value());
    }
}
