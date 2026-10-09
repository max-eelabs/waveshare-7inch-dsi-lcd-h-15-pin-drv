# AI-assisted firmware operation scripts

These silent Python wrappers are intended for AI-assisted local operations.
GitHub CI uses `idf.py build` directly and must not invoke these wrappers.
For manual compilation, use the standard ESP-IDF commands in the
[repository README](../README.md#build-and-tests).

Run these helpers from an **activated ESP-IDF 6.1 terminal**. That environment
supplies Python, CMake, Ninja, and OpenOCD. The scripts locate the repository
from their own file paths, so their defaults work from any current directory.
The commands below assume your terminal is at the driver repository root.

## Build the tests with no arguments

```sh
python scripts/build.py
```

This selects the Unity test application in `test/`, configures `build/test/`
automatically if it is new, then builds the firmware. Subsequent runs reuse the
configuration and perform an incremental build. Successful commands are silent;
failures print their output and return a nonzero exit code.

To configure or reconfigure explicitly:

```sh
python scripts/configure.py
```

This uses the same `test/` and `build/test/` defaults. The target is ESP32-P4,
and existing `sdkconfig` settings are retained.

Building creates firmware files. Unity tests execute when that firmware runs on
the board; compilation alone does not run them.

## Build the display example

```sh
python scripts/build.py --example
```

`--example` selects `examples/basic/` and `build/basic/`, configuring automatically
on the first build. For explicit configuration, use:

```sh
python scripts/configure.py --example
```

## Flash the selected application

Connect the ESP32-P4's built-in USB-JTAG interface, then run either command:

```sh
python scripts/flash.py            # Build and flash the Unity test firmware
python scripts/flash.py --example  # Build and flash the display example
```

Flashing replaces the firmware on the board. OpenOCD verifies the images and
resets the board. These commands use USB-JTAG, so no COM-port argument is needed.
Choose the chip revision and example power-relay setting described in the
[repository README](../README.md) for your hardware before flashing.

To flash binaries you have already built:

```sh
python scripts/flash.py --skip-build
python scripts/flash.py --example --skip-build
```

OpenOCD is discovered from project-local tools, the activated terminal's PATH,
or installed ESP-IDF tools. If needed, supply its executable explicitly:

```sh
python scripts/flash.py --openocd C:/path/to/openocd-esp32/bin/openocd.exe
```

If flashing works in the ESP-IDF extension but fails here, compare its configured
OpenOCD executable with the one on your terminal's PATH. They may be different
releases. Use `--openocd` to select the same executable as the extension.

For a persistent local default, place that complete OpenOCD installation at:

```text
.espressif/openocd-esp32-<version>/openocd-esp32/
  bin/openocd.exe
  share/openocd/scripts/
```

Project-local tools take precedence over PATH. The `.espressif/` directory is
ignored by Git; tool binaries remain separate from the driver package. Keep the
executable and its matching scripts from the same installation together.

## Optional arguments

All three scripts accept the same application selection options:

| Option | Default / behavior |
| --- | --- |
| No options | Application `test/`, output `build/test/` |
| `--example` | Application `examples/basic/`, output `build/basic/` |
| `--project PATH` | Choose another ESP-IDF application; output defaults to `build/<application directory name>/` |
| `--build-dir PATH` | Override the output directory |
| `--help` | Show arguments and exit |

`--example` and `--project` are alternatives. Relative paths are resolved from
the **repository root**; absolute paths also work. Supplying only `--build-dir`
for an existing configuration reuses the application recorded in that directory.
For a new directory, the default application is `test/`.

Examples with explicit paths, supported by configure, build, and flash:

```sh
python scripts/configure.py --project examples/basic
python scripts/build.py --project examples/basic
python scripts/build.py --example --build-dir build/my-demo
```

The flash script also accepts:

| Option | Default / behavior |
| --- | --- |
| `--skip-build` | Building first is the default; this option uses existing images |
| `--openocd PATH` | Automatic discovery is the default; this option selects an executable |

`common.py` shares path selection and quiet output handling between the scripts;
the three command-line entry points are configure, build, and flash.

## Common errors

- **Activate ESP-IDF:** open your installed ESP-IDF 6.1 terminal, then retry.
- **Build directory configured for another application:** use the default
  directory for the selected application or choose a new `--build-dir`.
- **Missing flash images:** run the matching build command before using
  `--skip-build`.
- **OpenOCD not found:** activate ESP-IDF or specify `--openocd` with its matching
  installation's ESP32-P4 board scripts.
- **Component registry connection failure:** check connectivity and retry
  configuration; a fresh configuration may download dependencies.
