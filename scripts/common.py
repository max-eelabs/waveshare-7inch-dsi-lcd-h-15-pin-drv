# Copyright 2026 Maxim Pavlov
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

"""Shared application defaults and quiet command handling for the wrappers."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


# Locate everything from this file so commands work from any current directory.
ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROJECT = ROOT / "test"
DEFAULT_BUILD_DIR = ROOT / "build" / "test"
EXAMPLE_PROJECT = ROOT / "examples" / "basic"


def repo_path(path: Path) -> Path:
    """Interpret relative options from the repository root, not the terminal cwd."""
    return (ROOT / path).resolve()


def cache_value(build_dir: Path, key: str) -> str | None:
    """Read one CMake cache setting without assuming its declared value type."""
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file():
        return None
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        name, separator, value = line.partition("=")
        if separator and name.split(":", 1)[0] == key:
            return value
    return None


def add_project_arguments(parser: argparse.ArgumentParser) -> None:
    """Give all three scripts the same application and output-directory options."""
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--example", action="store_true",
                           help="select examples/basic and build/basic")
    selection.add_argument("--project", type=Path,
                           help="application directory (default: test)")
    parser.add_argument("--build-dir", type=Path,
                        help="output directory (default: build/<application name>)")


def project_paths(args: argparse.Namespace) -> tuple[Path, Path]:
    """Select matching source/output paths, preserving custom configured builds."""
    project = EXAMPLE_PROJECT if args.example else repo_path(args.project or DEFAULT_PROJECT)
    build_dir = repo_path(args.build_dir) if args.build_dir else ROOT / "build" / project.name

    # --build-dir alone also works with an existing custom/example build: use the
    # source directory recorded by CMake instead of accidentally choosing tests.
    if args.build_dir and args.project is None and not args.example:
        cached_project = cache_value(build_dir, "CMAKE_HOME_DIRECTORY")
        if cached_project:
            project = Path(cached_project).resolve()
    return project, build_dir


def check_project(project: Path, build_dir: Path) -> None:
    """Reject missing sources and reuse of a build directory for another app."""
    if not (project / "CMakeLists.txt").is_file():
        raise RuntimeError(f"{project} has no CMakeLists.txt; check --project")
    cached_project = cache_value(build_dir, "CMAKE_HOME_DIRECTORY")
    if cached_project and Path(cached_project).resolve() != project:
        raise RuntimeError(
            f"{build_dir} is configured for {cached_project}; choose another --build-dir"
        )


def run_quiet(command: list[str]) -> int:
    """Keep successful commands silent and retain their full failure output."""
    try:
        # Argument lists avoid shell interpretation of paths and special characters.
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                errors="replace")
    except OSError as error:
        print(f"Could not run {command[0]}: {error}. Use an activated ESP-IDF terminal.",
              file=sys.stderr)
        return 2
    if result.returncode:
        output = result.stdout + result.stderr
        sys.stderr.write(output or f"Command failed: {' '.join(command)}\n")
    return result.returncode
