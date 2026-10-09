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

"""Configure the driver tests by default; use --example for the display demo."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys

from common import add_project_arguments, check_project, project_paths, run_quiet


def configure(project: Path, build_dir: Path) -> int:
    """Generate a matching ESP32-P4 build without resetting existing sdkconfig."""
    if not os.environ.get("IDF_PATH"):
        print("Configure setup error: use an activated ESP-IDF 6.1 terminal.", file=sys.stderr)
        return 2
    try:
        check_project(project, build_dir)
    except RuntimeError as error:
        print(f"Configure setup error: {error}", file=sys.stderr)
        return 2

    # This component targets ESP32-P4. Use the active environment's Python for
    # IDF's tools so CMake does not pick an unrelated system Python installation.
    return run_quiet([
        "cmake", "-S", str(project), "-B", str(build_dir), "-G", "Ninja",
        "-DIDF_TARGET=esp32p4", f"-DPYTHON={sys.executable}",
    ])


def main(argv: list[str] | None = None) -> int:
    """Parse optional overrides; no arguments configure test/ into build/test/."""
    parser = argparse.ArgumentParser(description=__doc__)
    add_project_arguments(parser)
    project, build_dir = project_paths(parser.parse_args(argv))
    return configure(project, build_dir)


if __name__ == "__main__":
    raise SystemExit(main())
