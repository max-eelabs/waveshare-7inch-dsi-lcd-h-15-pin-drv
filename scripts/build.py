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

"""Build the driver tests by default, configuring automatically when needed."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

from common import (
    DEFAULT_BUILD_DIR,
    DEFAULT_PROJECT,
    add_project_arguments,
    cache_value,
    check_project,
    project_paths,
    run_quiet,
)
from configure import configure


def configured_ninja(build_dir: Path) -> str:
    """Use the Ninja executable selected by CMake for this particular build."""
    ninja = cache_value(build_dir, "CMAKE_MAKE_PROGRAM")
    if not ninja:
        raise RuntimeError(f"no CMAKE_MAKE_PROGRAM in {build_dir}/CMakeCache.txt")
    return ninja


def build(project: Path = DEFAULT_PROJECT, build_dir: Path = DEFAULT_BUILD_DIR) -> int:
    """Configure a fresh output directory, then perform an incremental build."""
    try:
        check_project(project, build_dir)
    except RuntimeError as error:
        print(f"Build setup error: {error}", file=sys.stderr)
        return 2

    # A fresh checkout needs no separate configure command. Existing caches are
    # reused so subsequent builds retain the application's configured settings.
    if not (build_dir / "CMakeCache.txt").is_file() or not (build_dir / "build.ninja").is_file():
        result = configure(project, build_dir)
        if result:
            return result
    try:
        ninja = configured_ninja(build_dir)
    except RuntimeError as error:
        print(f"Build setup error: {error}", file=sys.stderr)
        return 2
    return run_quiet([ninja, "-C", str(build_dir)])


def main(argv: list[str] | None = None) -> int:
    """No arguments build test/ into build/test/; --example selects the demo."""
    parser = argparse.ArgumentParser(description=__doc__)
    add_project_arguments(parser)
    project, build_dir = project_paths(parser.parse_args(argv))
    return build(project, build_dir)


if __name__ == "__main__":
    raise SystemExit(main())
