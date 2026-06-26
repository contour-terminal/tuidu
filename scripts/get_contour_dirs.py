#!/usr/bin/env python3
"""Fetches the crispy source directory from the contour-terminal repository.

tuidu only depends on crispy::core (transitively, via endo-platform); vtparser is
not part of the coro/platform/tui chain, so it is intentionally not fetched.
"""

import os
import shutil
import stat
import subprocess
import sys
from pathlib import Path


def _force_remove_readonly(func, path, _exc_info):
    """Error handler for shutil.rmtree to remove read-only files (e.g. .git pack files on Windows)."""
    os.chmod(path, stat.S_IWRITE)
    func(path)


def _rmtree(path: Path) -> None:
    """Recursively removes @p path, forcing removal of read-only files.

    Bridges the shutil.rmtree error-handler keyword rename: Python 3.12+ uses
    ``onexc`` while older versions use ``onerror``. Picking the right one keeps the
    fetch working across the Python versions found on developer/CI machines.
    """
    if sys.version_info >= (3, 12):
        shutil.rmtree(path, onexc=_force_remove_readonly)
    else:
        shutil.rmtree(path, onerror=_force_remove_readonly)


def get_repo_root() -> Path:
    """Returns the root directory of the current git repository."""
    result = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        capture_output=True,
        text=True,
        check=True,
    )
    return Path(result.stdout.strip())


def main() -> None:
    repo_root = get_repo_root()
    src_dir = repo_root / "src"
    contour_dir = repo_root / "contour"

    # Clean up any previous copies
    for name in ("crispy",):
        target = src_dir / name
        if target.exists():
            _rmtree(target)

    if contour_dir.exists():
        _rmtree(contour_dir)

    # Sparse-clone only the directories we need
    subprocess.run(
        [
            "git", "clone",
            "-n", "--depth=1", "--filter=tree:0",
            "https://github.com/contour-terminal/contour.git",
            str(contour_dir),
        ],
        check=True,
    )
    subprocess.run(
        ["git", "sparse-checkout", "set", "--no-cone", "src/crispy"],
        cwd=contour_dir,
        check=True,
    )
    subprocess.run(
        ["git", "checkout"],
        cwd=contour_dir,
        check=True,
    )

    # Move the directories into place
    for name in ("crispy",):
        shutil.move(str(contour_dir / "src" / name), str(src_dir / name))

    # Clean up the clone
    _rmtree(contour_dir)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"Command failed: {e}", file=sys.stderr)
        sys.exit(1)
