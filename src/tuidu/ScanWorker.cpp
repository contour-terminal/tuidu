// SPDX-License-Identifier: Apache-2.0
#include <exception>
#include <optional>
#include <string>
#include <utility>

#include <tuidu/ScanWorker.hpp>
#include <tuidu/Scanner.hpp>

namespace tuidu
{

ScanWorker::ScanWorker(endo::platform::FileInfoProvider const& provider,
                       Tree& tree,
                       endo::platform::MessageQueue<ScanProgress>& progress,
                       ScanOptions options,
                       std::mutex& treeMutex) noexcept:
    _provider(provider), _tree(tree), _progress(progress), _options(options), _treeMutex(treeMutex)
{
}

ScanWorker::~ScanWorker()
{
    requestStop();
    if (_thread.joinable())
        _thread.join();
}

void ScanWorker::requestStop() noexcept
{
    _stop.request_stop();
}

void ScanWorker::start(NodeId rootId)
{
    _thread = std::thread([this, rootId] {
        Scanner scanner(_provider, _tree, _options, &_treeMutex);

        // Push every progress message onto the queue; its wakeup nudges the UI thread.
        auto sink = [this](ScanProgress const& p) {
            _progress.push(p);
        };

        // The scan task only co_awaits sub-scan tasks (no external I/O awaitables), so a
        // single resume drives the whole walk to completion. Seed the inherited StopToken
        // first so cooperative cancellation reaches every nested scanDir frame.
        auto task = scanner.scan(rootId, sink);
        task.handle().promise().setStopToken(_stop.get_token());
        task.handle().resume();
        try
        {
            // The root Task captures any body exception and rethrows it here.
            task.result();
        }
        catch (endo::coro::OperationCancelled const&)
        {
            // Cancelled mid-scan: emit a final done message so the UI stops the spinner.
            _progress.push(ScanProgress { .node = rootId, .currentPath = std::nullopt, .done = true });
        }
        catch (std::exception const& ex)
        {
            // Any other failure (e.g. a filesystem/encoding error from the provider) must not
            // escape the worker thread — an unhandled exception here would call std::terminate
            // and kill the process with no diagnostic. Surface it to the UI instead.
            _progress.push(ScanProgress { .node = rootId,
                                          .currentPath = std::nullopt,
                                          .done = true,
                                          .error = std::string { ex.what() } });
        }
        catch (...)
        {
            _progress.push(ScanProgress { .node = rootId,
                                          .currentPath = std::nullopt,
                                          .done = true,
                                          .error = std::string { "unknown error during scan" } });
        }
    });
}

} // namespace tuidu
