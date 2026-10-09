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

"""Build and flash driver tests by default through built-in USB-JTAG."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import sys

from build import build
from common import ROOT, add_project_arguments, check_project, project_paths, repo_path, run_quiet


def openocd_scripts(executable: Path) -> Path:
    """Find the board configuration shipped alongside an OpenOCD executable."""
    scripts = executable.parent.parent / "share" / "openocd" / "scripts"
    if not executable.is_file() or not (scripts / "board/esp32p4-builtin.cfg").is_file():
        raise RuntimeError(f"{executable} needs matching ESP32-P4 OpenOCD scripts")
    return scripts


def openocd_paths() -> tuple[Path, Path]:
    """Prefer project-local tools, then the activated environment and IDF installs."""
    executable_name = "openocd.exe" if os.name == "nt" else "openocd"
    candidates = sorted(
        (ROOT / ".espressif").glob(f"openocd-esp32-*/openocd-esp32/bin/{executable_name}"),
        reverse=True,
    )
    # An activated IDF terminal normally puts its chosen OpenOCD on PATH.
    on_path = shutil.which("openocd")
    if on_path:
        candidates.append(Path(on_path))
    tools_roots = [Path(os.environ.get("IDF_TOOLS_PATH", str(Path.home() / ".espressif"))) / "tools"]
    if os.name == "nt":
        tools_roots.append(Path("C:/Espressif/tools"))
    for tools_root in tools_roots:
        candidates.extend(sorted(
            (tools_root / "openocd-esp32").glob(f"*/openocd-esp32/bin/{executable_name}"),
            reverse=True,
        ))
    for executable in candidates:
        try:
            return executable, openocd_scripts(executable)
        except RuntimeError:
            continue
    raise RuntimeError("no ESP32-P4 OpenOCD found; activate ESP-IDF or supply --openocd")


def tcl_path(path: Path) -> str:
    """Quote paths containing spaces or Tcl substitution characters for OpenOCD."""
    value = path.as_posix()
    for character in ['\\', '"', '$', '[', ']']:
        value = value.replace(character, '\\' + character)
    return '"' + value + '"'


def main(argv: list[str] | None = None) -> int:
    """Build the selected application unless --skip-build requests existing binaries."""
    parser = argparse.ArgumentParser(description=__doc__)
    add_project_arguments(parser)
    parser.add_argument("--skip-build", action="store_true",
                        help="flash existing binaries (default: build first)")
    parser.add_argument("--openocd", type=Path,
                        help="OpenOCD executable (default: discover installed ESP-IDF tool)")
    args = parser.parse_args(argv)
    project, build_dir = project_paths(args)

    if not args.skip_build:
        result = build(project, build_dir)
        if result:
            return result
    try:
        check_project(project, build_dir)
        # CMake generates the list of binary files and flash offsets for this app.
        # Keeping it tied to build_dir prevents mixing test and example images.
        if not (build_dir / "flasher_args.json").is_file():
            raise RuntimeError(f"{build_dir}/flasher_args.json is missing; build first")
        if args.openocd:
            openocd = repo_path(args.openocd)
            scripts = openocd_scripts(openocd)
        else:
            openocd, scripts = openocd_paths()
    except RuntimeError as error:
        print(f"Flash setup error: {error}", file=sys.stderr)
        return 2

    # OpenOCD programs the ESP32-P4's built-in USB-JTAG interface, verifies the
    # binaries, and resets the board. It does not use a serial COM-port argument.
    return run_quiet([
        str(openocd), "-s", str(scripts), "-f", "board/esp32p4-builtin.cfg",
        "-c", f"program_esp_bins {tcl_path(build_dir)} flasher_args.json verify reset exit",
    ])


if __name__ == "__main__":
    raise SystemExit(main())
