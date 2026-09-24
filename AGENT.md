# tuidu — Agent Guidelines

## Project Overview

**tuidu** is a TUI-first `du` (disk usage) application — an *ncdu* competitor — with vim-like
motions, built **coroutine-first** in C++23. It scans a directory tree on a background thread and
presents a live, navigable, sorted view of where disk space goes.

### Data Flow

```
FileInfoProvider (lstat) → Scanner (coroutine walk) → Tree (flat arena, aggregated sizes)
                                                          │
                          ScanWorker (jthread) ── MessageQueue ──▶ App main-loop (TuiRuntime)
                                                          │
                          DiskUsageModel ──▶ core::tui::TreeTableView (generic browser) ──▶ Screen
```

### Component Map

| Directory / File | Purpose |
|------------------|---------|
| `src/tuidu/` | **The application** (see below). |
| [core-cpp](https://github.com/contour-terminal/core-cpp) (CPM, pinned in `cmake/EndoThirdParties.cmake`) | The shared foundation tuidu builds on: `core::async` (`Task<T>`, `whenAll`, `StopToken` cancellation), `core::platform` (`FileInfoProvider`, `FileSystem`, `Wakeup`, `MessageQueue`, `SignalHandler`), `core::net` (`EventLoop`, the scheduler the TUI runtime is composed on), `core::tui` (`Screen`, `Component`, `Canvas`, `Terminal`, `TreeTableView`, `StatusBar`, `Theme`, `KeyBindings`, the coroutine `runtime/`), `core::cli` (the command-line parser) and `core::testing`. |

### Application modules (`src/tuidu/`)

| File | Responsibility | Injected dependencies |
|------|----------------|------------------------|
| `Node.hpp`, `Tree.{hpp,cpp}` | Flat-arena disk-usage tree; aggregated sizes; sorted child index | — |
| `Scanner.{hpp,cpp}` | Coroutine-first recursive walker; aggregation, dedup, cross-device, symlink/error policy | `FileInfoProvider const&`, `Tree&` |
| `ScanWorker.{hpp,cpp}` | `std::jthread` host driving the scanner; pushes progress | `FileInfoProvider`, `Tree`, `MessageQueue`, `std::mutex` |
| `SizeFormat`, `SortMode`, `Columns`, `ColorRules`, `Keymap`, `Search` | **Data-driven tables** — each adds a feature by adding a row | — |
| `DiskUsageModel.{hpp,cpp}` | Adapts `Tree` to the generic `core::tui::TreeTableModel` (the DI seam) | `Tree&` |
| `ThemeController.{hpp,cpp}` | Configurable theming + auto dark/light detection (DEC 2031) | `core::tui::Terminal`/`ThemeManager` |
| `App.{hpp,cpp}` | Wires terminal/screen/runtime/worker; owns the coroutine main flow | `EventLoop&`, `Terminal&`, `InputSource&`, `Wakeup const&`, `FileInfoProvider&`, `MessageQueue&` |
| `main.cpp` | Composition root — the **only** place naming concrete platform classes | — |

---

## Design Patterns & Principles

**Dependency injection and data-driven design are always prioritized.** Treat both as the default
for new code, not as optional refinements. Deviate only with a strong, explicitly-stated reason
(e.g. a measured hot path where indirection is unacceptable), and call out that justification in
the code and the PR summary.

### Dependency Injection via Constructor Injection

All OS, file-system, and I/O access is abstracted behind interfaces and injected via constructors.
Never hard-code side effects.

- `<core/platform/FileInfoProvider.hpp>` — directory-listing seam (`nativeFileInfoProvider()` in
  production, `core::platform::testing::MockFileInfoProvider` in tests). The core seam for the
  scanner.
- `<core/platform/FileSystem.hpp>` — file-system abstraction (+ `testing::InMemoryFileSystem`).
- `core::tui::Terminal` accepts a custom `TerminalOutput` (use `MockTerminalOutput` in tests).
- `core::net::EventLoop` + `core::tui::runtime::InputSource` — the loop the App runs on and where
  its input comes from (`TerminalInputSource` in production; in tests a
  `core::tui::runtime::testing::ScriptedInputSource` over a `SystemPipe`).
- `core::tui::TreeTableModel` — the seam between the generic browser view and any domain data;
  tuidu implements it with `DiskUsageModel` over its `Tree`.

When adding functionality that touches the OS, define/extend an abstract interface — in tuidu if
it is du-specific, in core-cpp if it is generic — implement per-platform, and inject it.

### Data-Driven Design

Drive behavior from data — tables, descriptors, configuration — rather than hand-written per-case
logic. Adding a new case should mean adding a data row, not writing new branches.

Canonical examples in tuidu: `kDefaultKeymap` (key → `Action`), `kColumns` (column formatters),
`kSortModes` (label + comparator), the binary/SI `UnitRow` tables, and `kColorRules` (predicate →
palette slot). Each table is consumed by many features and has a parameterized "every row works"
test (`[tables]` tag).

### Error Handling: `std::expected<T, E>`

Use `std::expected` for fallible operations; prefer monadic chaining (`and_then`, `or_else`,
`transform`, `transform_error`) over if/else ladders. Reserve exceptions for the coroutine
cancellation path (`core::async::OperationCancelled`) and truly exceptional situations.

### Memory Management

Smart pointers for ownership; RAII for resources. The disk-usage `Tree` is a flat arena of
`Node`s addressed by 32-bit `NodeId` handles (not pointers), so re-sorting and growth never
invalidate references — but never hold a `Node&` across a tree mutation; hold a `NodeId`.

---

## The shared layers come from core-cpp — fix them there

The coroutine, platform, TUI and CLI layers are **core-cpp's**, shared with endo, contour and
fastcached, and tuidu pins a released tag of it. There is no copy of them in this repository.

- **No tuidu-specific concept may enter core-cpp** — no disk-usage notions, no `Node`/`Tree`/
  `Scanner`. Anything du-specific lives only in `src/tuidu/`.
- **A change tuidu needs in those layers is a core-cpp change**: make it there as a generic
  improvement (e.g. the `FileEntry` block/identity fields are standard `stat(2)` data; the generic
  `core::tui::TreeTableView` browser is reusable by any tree-of-rows app), let core-cpp release it,
  then move the pin in `cmake/EndoThirdParties.cmake`. Never patch core-cpp from tuidu's side.
- To iterate against a local core-cpp checkout, configure with
  `-DCPM_core-cpp_SOURCE=/path/to/core-cpp`.

---

## C++ Coding Guidelines

- **Requires C++23** (enforced at configure time): `constexpr`, `std::ranges`, `std::format`,
  `std::print`, `std::expected`, structured bindings, coroutines.
- C-style loops are forbidden; use range-based loops and `std::views`.
- Use `std::span` for contiguous sequences; `auto` for readability; `const` correctness throughout.
- Mark `[[nodiscard]]` where ignoring a result is a bug.
- Document new public functions, classes, structs, and members with Doxygen (`/// @param`,
  `/// @return`).
- Coroutine parameters must be passed **by value** (references can dangle across suspension).
- Naming conventions (including constant casing — `CamelCase`, never a `k` prefix) and all other
  static-analysis rules are **mandated by `.clang-tidy`** (authoritative; runs in debug builds).
  Consult it as the source of truth, and do not suppress findings with `NOLINT` — fix the underlying issue.
- Run `clang-format` after changes; formatting rules are in `.clang-format`.

---

## Building

```bash
# Configure (debug with ASAN, UBSAN, clang-tidy). Use Homebrew LLVM on macOS:
#   export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug

# Coverage
cmake --preset clang-coverage
cmake --build --preset clang-coverage --target coverage

# Release
cmake --preset clang-release && cmake --build --preset clang-release
```

First configure fetches core-cpp and the other dependencies through CPM (needs git + network).
The project must be a git repository before the first configure.

Run the app: `./build/clang-debug/src/tuidu/tuidu [path]` (defaults to the current directory).

### Windows (clang-cl)

Windows uses the `clangcl-debug` / `clangcl-release` presets (Ninja + `clang-cl`). Because Ninja
needs the MSVC environment (INCLUDE/LIB for the MSVC STL + Windows SDK), run from an **"x64 Native
Tools Command Prompt for VS"**, or import the dev environment first in PowerShell:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -SkipAutomaticLocation
cmake --preset clangcl-debug
cmake --build --preset clangcl-debug
ctest --preset clangcl-debug
.\build\clangcl-debug\src\tuidu\tuidu.exe [path]
```

The Windows code paths (`WindowsFileInfoProvider`, the Windows terminal I/O, `WindowsWakeup`, the
event loop's Windows backend) are core-cpp's, which selects them per platform. Sanitizers,
coverage, and clang-tidy are not wired up for the clang-cl presets (Clang/GCC-only by design).
`tuidu` is a full-screen console app: it requires a real console and exits with
`failed to initialize terminal` if stdin/stdout are redirected.

---

## Testing

Tests are **Catch2** unit/integration tests, co-located in the source tree as `*_test.cpp` and
gated on `TUIDU_TESTING` (the `test-tuidu` target). There is no separate E2E runner.

**Every feature has an automated test:**
- **Pure logic** (Tree, SizeFormat, SortMode, Columns, ColorRules, Keymap, Search) — direct unit
  tests, including a parameterized `[tables]` test that exercises every descriptor-table row.
- **Scanner** — every edge case (aggregation, disk-usage vs apparent, hardlink dedup, cross-device,
  symlink, permission/error, cancellation) with `MockFileInfoProvider`.
- **ScanWorker** — thread lifecycle, progress drain, cancellation, destructor-join.
- **Theming / model** — `ThemeController` mapping + runtime switch; `DiskUsageModel` adapter.
- **App** — end-to-end key flows (`j`/`l`/`q`, descend/quit) on a real `core::net::EventLoop`,
  typed through a `ScriptedInputSource` over a `SystemPipe` into a `MockTerminalOutput`-backed
  terminal, asserting the resulting tree/model state. Each scripted step waits, bounded, for the
  state it types into and names it when it never arrives.

A module is not done until its tests are green. Keep coverage high; the only deliberately-untested
code is `main.cpp`, the composition root over the real terminal and file system.

---

## Adding Features

### A new key binding / column / sort mode / color rule / size unit

Add a row to the relevant table (`kDefaultKeymap`, `kColumns`, `kSortModes`, `kColorRules`,
`kBinaryUnits`/`kSiUnits`). Extend the `[tables]` test data so the new row is covered. No new
branching logic.

### A new platform capability

A generic capability (extending `FileEntry`/`FileInfoProvider`, a new OS seam) is a core-cpp change:
make it there, release it, and move tuidu's pin. A du-specific one gets its interface in
`src/tuidu/`, implemented per platform and injected.

### A new browser interaction

The generic motions (move/descend/ascend/sort/page) live in `core::tui::TreeTableView`; the data and
navigation come from the `TreeTableModel`. Add app-specific behavior in `App::dispatch` and the
`Keymap`, keeping the view domain-agnostic.

---

## Workflow (post-implementation checklist)

1. Run `clang-format` on added/changed C++ files.
2. Run the full suite: `ctest --preset clang-debug` (must be fully green).
3. If tuidu needed a change in core-cpp, it went to core-cpp and tuidu moved its pin to the release
   that carries it.
4. Run `clang-coverage` and note the coverage numbers.
5. In the summary, include: **performance impact**, **risk assessment**, and **code-coverage
   results**.
