# tuidu

A **TUI-first `du`** — a terminal disk-usage analyzer with vim-like motions, built coroutine-first
in C++23. An [ncdu](https://dev.yorhel.nl/ncdu) competitor.

## Features

- **Live scanning** on a background thread — the tree fills in and bars animate while you browse.
- **Vim motions**: `j`/`k` move, `g`/`G` jump to top/bottom, `l`/Enter descend, `h`/Backspace
  ascend, `PageUp`/`PageDown` page.
- **Data-driven columns**: size, proportional bar, %-of-parent, item count, date, name.
- **Sort modes**: by size (↓/↑), name, item count, date — toggle with `s`/`n`/`C`/`M`.
- **Real disk usage**: `lstat`-based block accounting (`st_blocks`), hardlink dedup, cross-device
  boundary detection — toggle apparent vs disk with `a`.
- **Fuzzy ranked search** (`/`): contiguous substring matches outrank scattered fuzzy matches.
- **Configurable theming** with automatic dark/light detection (DEC mode 2031) that follows the
  terminal at startup and at runtime.

## Building

Requires a C++23 compiler (Clang ≥ 17 / GCC ≥ 13), CMake ≥ 3.30, Ninja, Python 3, and git.

```bash
# On macOS, use Homebrew LLVM:
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"

cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
```

The first configure fetches the vendored `crispy` utility library from contour-terminal (needs
network).

## Usage

```bash
./build/clang-debug/src/tuidu/tuidu [path]   # defaults to the current directory
```

Press `?` for help, `q` to quit.

## Architecture

See [AGENT.md](AGENT.md). In short: an injectable `FileInfoProvider` feeds a coroutine `Scanner`
that builds a flat-arena `Tree`; a background `ScanWorker` streams progress to a `TuiRuntime`
main loop; a `DiskUsageModel` adapts the tree to the generic, reusable `tui::TreeTableView` browser.
Every OS touch is behind a dependency-injected interface; every behavioral axis (keys, columns,
sorts, units, colors) is a data table.

## License

Apache-2.0. See [LICENSE.txt](LICENSE.txt).
