// SPDX-License-Identifier: Apache-2.0
#include <format>
#include <optional>
#include <ranges>
#include <system_error>
#include <utility>
#include <vector>

#include <tuidu/DeleteWorker.hpp>

namespace tuidu
{

DeleteWorker::DeleteWorker(endo::platform::FileSystem const& fs,
                           endo::platform::MessageQueue<DeleteProgress>& progress,
                           std::uint32_t progressEvery) noexcept:
    _fs(fs), _progress(progress), _progressEvery(progressEvery == 0 ? 1 : progressEvery)
{
}

DeleteWorker::~DeleteWorker()
{
    requestStop();
    if (_thread.joinable())
        _thread.join();
}

void DeleteWorker::requestStop() noexcept
{
    _thread.request_stop();
}

void DeleteWorker::start(std::filesystem::path target, std::uint64_t total, DeleteMode mode)
{
    _thread = std::jthread([this, target = std::move(target), total, mode](std::stop_token const& stop) {
        run(stop, target, total, mode);
    });
}

void DeleteWorker::run(std::stop_token const& stop,
                       std::filesystem::path const& target,
                       std::uint64_t total,
                       DeleteMode mode)
{
    auto deleted = std::uint64_t { 0 };

    auto emit = [&](std::string path, bool done, bool cancelled, std::optional<std::string> error) {
        _progress.push(DeleteProgress { .deleted = deleted,
                                        .total = total,
                                        .currentPath = std::move(path),
                                        .done = done,
                                        .cancelled = cancelled,
                                        .error = std::move(error) });
    };

    if (!_fs.exists(target))
    {
        emit(target.string(),
             /*done=*/true,
             /*cancelled=*/false,
             std::format("'{}' no longer exists", target.string()));
        return;
    }

    if (mode == DeleteMode::Recursive)
    {
        // Enumerate the subtree (parents before contents). Pass an out-error so a partial
        // enumeration (e.g. a permission-denied subtree) aborts the destructive operation.
        auto entries = std::vector<endo::platform::FileSystem::DirectoryEntry> {};
        auto walkError = std::error_code {};
        for (auto const& entry: _fs.walkDirectoryRecursive(target, &walkError))
        {
            if (stop.stop_requested())
            {
                emit({}, /*done=*/true, /*cancelled=*/true, std::nullopt);
                return;
            }
            entries.push_back(entry);
        }
        if (walkError)
        {
            emit(target.string(), /*done=*/true, /*cancelled=*/false, walkError.message());
            return;
        }

        // Remove deepest-first: the walk is a pre-order DFS, so reversing it guarantees every
        // child is gone before its parent directory — letting the directory removal succeed.
        for (auto const& entry: std::ranges::reverse_view(entries))
        {
            if (stop.stop_requested())
            {
                emit({}, /*done=*/true, /*cancelled=*/true, std::nullopt);
                return;
            }
            if (auto const removed = _fs.remove(entry.path); !removed)
            {
                emit(entry.path.string(), /*done=*/true, /*cancelled=*/false, removed.error());
                return;
            }
            ++deleted;
            if (deleted % _progressEvery == 0)
                emit(entry.path.string(), /*done=*/false, /*cancelled=*/false, std::nullopt);
        }
    }

    // Remove the target itself: an empty directory after the walk above, or a single file/symlink
    // (which is unlinked, not followed) when not recursing.
    if (auto const removed = _fs.remove(target); !removed)
    {
        emit(target.string(), /*done=*/true, /*cancelled=*/false, removed.error());
        return;
    }
    ++deleted;
    emit(target.string(), /*done=*/true, /*cancelled=*/false, std::nullopt);
}

} // namespace tuidu
