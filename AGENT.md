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
                          DiskUsageModel ──▶ tui::TreeTableView (generic browser) ──▶ Screen
```

### Component Map

| Directory / File | Purpose |
|------------------|---------|
| `src/coro/` | C++23 coroutine primitives: `Task<T>`, `whenAll`, `StopToken` cancellation. **Vendored — keep in sync with endo.** |
| `src/platform/` | OS abstraction: `FileInfoProvider` (lstat-based directory listing), `FileSystem`, `Wakeup`, `MessageQueue`, `SignalHandler`. **Vendored — keep in sync with endo.** |
| `src/tui/` | Terminal UI: `Screen`, `Component`, `Canvas`, `Terminal`, `TreeTableView` (generic browser), `StatusBar`, `Theme`, `KeyBindings`, coroutine `runtime/`. **Vendored — keep in sync with endo.** |
| `src/crispy/` | Utility library, auto-fetched from contour-terminal (gitignored). |
| `src/tuidu/` | **The application** (see below). |

### Application modules (`src/tuidu/`)

| File | Responsibility | Injected dependencies |
|------|----------------|------------------------|
| `Node.hpp`, `Tree.{hpp,cpp}` | Flat-arena disk-usage tree; aggregated sizes; sorted child index | — |
| `Scanner.{hpp,cpp}` | Coroutine-first recursive walker; aggregation, dedup, cross-device, symlink/error policy | `FileInfoProvider const&`, `Tree&` |
| `ScanWorker.{hpp,cpp}` | `std::jthread` host driving the scanner; pushes progress | `FileInfoProvider`, `Tree`, `MessageQueue`, `std::mutex` |
| `SizeFormat`, `SortMode`, `Columns`, `ColorRules`, `Keymap`, `Search` | **Data-driven tables** — each adds a feature by adding a row | — |
| `DiskUsageModel.{hpp,cpp}` | Adapts `Tree` to the generic `tui::TreeTableModel` (the DI seam) | `Tree&` |
| `ThemeController.{hpp,cpp}` | Configurable theming + auto dark/light detection (DEC 2031) | `tui::Terminal`/`ThemeManager` |
| `App.{hpp,cpp}` | Wires terminal/screen/runtime/worker; owns the coroutine main loop | `Terminal&`, `EventSource&`, `FileInfoProvider&`, `MessageQueue&` |
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

- `src/platform/FileInfoProvider.hpp` — directory-listing seam (`Linux`/`Darwin`/`Windows` impls,
  `testing/MockFileInfoProvider` for tests). The core seam for the scanner.
- `src/platform/FileSystem.hpp` — file-system abstraction (+ `testing/InMemoryFileSystem`).
- `tui::Terminal` accepts a custom `TerminalOutput` (use `MockTerminalOutput` in tests).
- `tui::runtime::EventSource` — the runtime's wait source (`TerminalEventSource` in production,
  a scripted source in tests).
- `tui::TreeTableModel` — the seam between the generic browser view and any domain data; tuidu
  implements it with `DiskUsageModel` over its `Tree`.

When adding functionality that touches the OS, define/extend an abstract interface in
`src/platform/` (or the relevant component), implement per-platform, and inject it.

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
cancellation path (`endo::coro::OperationCancelled`) and truly exceptional situations.

### Memory Management

Smart pointers for ownership; RAII for resources. The disk-usage `Tree` is a flat arena of
`Node`s addressed by 32-bit `NodeId` handles (not pointers), so re-sorting and growth never
invalidate references — but never hold a `Node&` across a tree mutation; hold a `NodeId`.

---

## The vendored libraries are shared with endo — keep them generic and in sync

`src/coro`, `src/platform`, and `src/tui` are **vendored copies of the same libraries in the
sibling `../endo` project** and must stay byte-for-byte synchronizable in both directions.

- **No tuidu-specific concept may leak into them** — no disk-usage notions, no `Node`/`Tree`/
  `Scanner`. Anything du-specific lives only in `src/tuidu/`.
- **Every change to these libs must be a generic improvement** that makes sense in endo on its own
  merits (e.g. the `FileEntry` block/identity fields are standard `stat(2)` data; the generic
  `tui::TreeTableView` browser is reusable by any tree-of-rows app).
- **After editing a vendored file, mirror the identical change into `../endo/src/{coro,platform,
  tui}/`** and run endo's tests to confirm no regression.

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
- Naming/static-analysis rules live in `.clang-tidy` (authoritative; runs in debug builds). Do not
  suppress findings with `NOLINT` — fix the underlying issue.
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

First configure auto-fetches `crispy` from contour-terminal (needs git + network). The project
must be a git repository before the first configure.

Run the app: `./build/clang-debug/src/tuidu/tuidu [path]` (defaults to the current directory).

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
- **App** — end-to-end key flows (`j`/`l`/`q`, descend/quit) through a scripted `EventSource` +
  `MockTerminalOutput`, asserting the resulting tree/model state.

A module is not done until its tests are green. Keep coverage high; the only deliberately-untested
code is the thin OS shims (`*FileInfoProvider` syscalls, `TerminalEventSource::wait`).

---

## Adding Features

### A new key binding / column / sort mode / color rule / size unit

Add a row to the relevant table (`kDefaultKeymap`, `kColumns`, `kSortModes`, `kColorRules`,
`kBinaryUnits`/`kSiUnits`). Extend the `[tables]` test data so the new row is covered. No new
branching logic.

### A new platform capability

Define an abstract interface in `src/platform/` (or extend `FileEntry`/`FileInfoProvider`),
implement per-platform, inject it. If the change is generic, mirror it into endo.

### A new browser interaction

The generic motions (move/descend/ascend/sort/page) live in `tui::TreeTableView`; the data and
navigation come from the `TreeTableModel`. Add app-specific behavior in `App::dispatch` and the
`Keymap`, keeping the view domain-agnostic.

---

## Workflow (post-implementation checklist)

1. Run `clang-format` on added/changed C++ files.
2. Run the full suite: `ctest --preset clang-debug` (must be fully green).
3. If you changed a vendored file (`coro`/`platform`/`tui`), mirror it into `../endo/src/…` and run
   endo's tests.
4. Run `clang-coverage` and note the coverage numbers.
5. In the summary, include: **performance impact**, **risk assessment**, and **code-coverage
   results**.
