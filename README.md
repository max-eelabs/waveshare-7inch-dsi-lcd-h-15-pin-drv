# Waveshare 7inch DSI LCD (H), 15-pin ESP32-P4 driver

`waveshare_7inch_h_dsi_15pin` is an ESP-IDF component for the **Waveshare
7inch DSI LCD (H), 1280 x 720**, connected through the ESP32-P4 Function-EV
board's **15-pin MIPI DSI port**. Repository:
`waveshare-7inch-dsi-lcd-h-15-pin-drv`. It includes display and GT911 touch
support and has no LVGL dependency.

Supported software: ESP-IDF `>=6.1,<6.2`, ESP32-P4, and PSRAM enabled.
The component owns I2C0, DSI0, the bridge at 0x45, and LDO channel 3.
Only one active display instance is supported. This is the H-model driver;
the register sequence and timings are not intended for other Waveshare panels.

## Wiring and defaults

The 15-pin designation identifies the board-side connection. Use the matching
15-to-22-pin **DSI** cable to the display's 22-pin input, plus external 5 V and
common ground. A CSI camera adapter is not interchangeable. Connect with power
off and check ribbon contact orientation.

| Signal | Default |
| --- | --- |
| I2C SDA / SCL | GPIO7 / GPIO8, carried by the DSI ribbon |
| Display power | External 5 V supply, at least 0.5 A for the display |
| Optional active-high power relay | Disabled (`-1`); set GPIO21 for the Function-EV relay setup |
| Power off / power on delay | 250 ms / 250 ms |
| Touch | Poll GT911 at 0x14, then 0x5D; no separate reset or interrupt wire |

A relay input needs a hardware pull-down to keep display power off while the
ESP32-P4 resets. Deinitialization releases driver resources but does not turn
off an external relay.

Video uses RGB888, two DSI lanes at 1200 Mbps, an 80 MHz pixel clock, and
64-pixel/line sync widths and front/back porches. The PHY uses LDO3 at 2.5 V.
These settings preserve the existing Function-EV configuration. The framebuffer
uses PSRAM and DMA2D. Tear-free animation is not guaranteed.

## Install and use

Declare the component in the application's `main/idf_component.yml`:

```yaml
dependencies:
  waveshare_7inch_h_dsi_15pin:
    git: "https://github.com/max-eelabs/waveshare-7inch-dsi-lcd-h-15-pin-drv.git"
    version: "main"
```

Component Manager checks out the remote revision into `managed_components`.
For reproducible builds, replace `main` with a full commit SHA from the GitHub
repository. Uncommitted local edits do not affect consuming applications. Do not
copy driver sources under the application's `components` directory or edit the
managed copy.
The bundled test and example applications use local component wrappers to build
this working tree directly, so they include uncommitted driver changes.

```c
#include "waveshare_7inch_h_dsi_15pin.h"

#include "esp_err.h"

static waveshare_7inch_h_dsi_15pin_t panel;

void start_display(void)
{
    waveshare_7inch_h_dsi_15pin_config_t config =
        waveshare_7inch_h_dsi_15pin_default_config();
    ESP_ERROR_CHECK(waveshare_7inch_h_dsi_15pin_init(&panel, &config));
    esp_err_t error = waveshare_7inch_h_dsi_15pin_init_touch(&panel);
    if (error != ESP_ERR_NOT_FOUND) {
        ESP_ERROR_CHECK(error);
    }
    // Borrow panel.panel for drawing and panel.touch for touch input.
}
```

The application owns GUI integration. An LVGL adapter can borrow `panel.panel`,
`panel.dsi_io`, and `panel.touch` when touch initialization succeeds. Choose
buffers and rotation settings for your application. The bundled basic example
uses the panel's single RGB888 framebuffer directly and has no LVGL dependency.
Do not apply a second touch-axis transform: the GT911 configuration already maps
native portrait coordinates to landscape.

Keep the driver struct alive and do not copy it. Serialize lifecycle calls and
touch reads.
Initialize a fresh or deinitialized instance only. Failed initialization releases
resources acquired so far. Missing GT911 returns `ESP_ERR_NOT_FOUND`; the
application chooses whether to continue. Other errors must be handled separately.

Call `waveshare_7inch_h_dsi_15pin_set_brightness` with 0 to 255 (off to maximum
brightness).
`waveshare_7inch_h_dsi_15pin_run_diagnostics` temporarily shows hardware color
bars; pass zero to skip. Backlight is enabled after DSI startup, before
application drawing.

Before calling `waveshare_7inch_h_dsi_15pin_deinit`, stop GUI activity and remove
application-owned touch/display adapters. Do not delete borrowed panel, touch,
bus, or I/O handles yourself. Cleanup accepts a zeroed instance and can be repeated.

## Build and tests

From this repository in an activated ESP-IDF 6.1 terminal, build with the
standard ESP-IDF command:

```sh
idf.py -C test -B ../build/test build
idf.py -C examples/basic -B ../../build/basic build
```

Both applications target ESP32-P4 through their `sdkconfig.defaults`. The first
command compiles the driver with the Unity tests; the second compiles it with
the basic display example. These builds use the repository's current sources.

The Python scripts in `scripts/` are silent wrappers for AI-assisted local
operations. They are not the CI build interface. AI agents use
`python scripts/build.py` for tests and add `--example` for the example;
configuration happens automatically when needed. See the
[scripts README](scripts/README.md) for wrapper defaults, overrides, flashing,
and troubleshooting.

The wrappers print output only on error. Flashing uses the built-in USB-JTAG
interface through OpenOCD; it does not take a serial COM-port argument. Select
chip revision settings matching your board before flashing. The basic example assumes independent panel power;
set `CONFIG_WAVESHARE_EXAMPLE_POWER_GPIO=21` if using the Function-EV relay setup.

The single `test/` directory is a standalone ESP-IDF test application:

```text
test/
  CMakeLists.txt
  sdkconfig.defaults
  dependencies.lock
  pytest_driver.py
  main/
    CMakeLists.txt
    main.c
    test_waveshare_7inch_h_dsi_15pin.c
  components/
    waveshare_7inch_h_dsi_15pin/  # Builds the repository's driver sources
```

The four Unity tests cover defaults, invalid arguments and GPIO configurations,
empty/repeated cleanup, and touch initialization state guards. They do not
initialize display hardware, but execute on an ESP32-P4. When explicitly ready
to replace the running firmware:

```sh
python scripts/flash.py
pytest test/pytest_driver.py --embedded-services esp,idf --target esp32p4 --app-path test --build-dir ../build/test --port COM12
```

Expect four tests and zero failures. Building is not execution of these tests.
The pytest command may flash the test image as part of DUT setup. It requires
the `pytest-embedded` ESP/IDF services; replace `COM12` with your board's serial
port.

The basic example draws eight steady RGB888 color bars below a status line.
It shows uptime in seconds and, when GT911 is present, press/release state,
landscape coordinates, and a tap count. The count increments once per new press
anywhere on the screen. Touch is polled every 20 ms; the last coordinates remain
visible after release. With GT911 absent, the status line shows `GT911 NOT FOUND`
and the display continues running. Other touch errors stop the example.

The example renders status text in an internal-RAM staging buffer, waits for a
fresh VSYNC, then copies and flushes only the text rows to the live framebuffer.
Brightness stays at maximum and the color bars remain unchanged. This differs
from the driver API's full-screen hardware color-bar diagnostic.

To flash the example when ready to replace the running firmware:

```sh
python scripts/flash.py --example
```

Check the color order (white, yellow, cyan, green, magenta, red, blue, black),
advancing uptime, press/release state, retained coordinates after release, and
one counter increment per new press. Check landscape coordinates near all four
corners and display-only startup with GT911 absent. Brightness control requires
an application call to `waveshare_7inch_h_dsi_15pin_set_brightness`; the example
does not cycle brightness. Touch calibration and hardware I/O failure checks
require physical verification and are separate from the Unity tests.

## Driver source layout

| File | Responsibility |
| --- | --- |
| `include/waveshare_7inch_h_dsi_15pin.h` | Public configuration, resource handles, and API declarations |
| `src/waveshare_7inch_h_dsi_15pin.c` | Defaults, optional power cycling, initialization, and cleanup |
| `src/waveshare_7inch_h_dsi_15pin_bridge.c` | Bridge I2C setup and inverted backlight control |
| `src/waveshare_7inch_h_dsi_15pin_video.c` | DSI resources, RGB888 timings, panel commands, and diagnostics |
| `src/waveshare_7inch_h_dsi_15pin_touch.c` | GT911 discovery and landscape coordinate configuration |
| `src/waveshare_7inch_h_dsi_15pin_private.h` | Doxygen-documented internal bridge and video setup interfaces |

## Firmware CI

The [Firmware build workflow](.github/workflows/build.yml) runs on pull requests
and pushes to `main`, including merges. Separate jobs compile `test/` and
`examples/basic/` for ESP32-P4 using ESP-IDF `v6.1` and Espressif's
[ESP-IDF CI action](https://github.com/espressif/esp-idf-ci-action).
Each job runs `idf.py build` directly. CI does not use the Python wrappers,
which are intended for AI-assisted local operations.

CI compiles and links the driver and both applications. It does not flash a
board or execute the Unity tests; those require an ESP32-P4.

## License header checks

The [License headers workflow](.github/workflows/license.yml) runs on pull
requests and pushes to `main`, including merges. It installs
`github.com/google/addlicense@v1.2.0` and checks tracked files for license headers.
This workflow checks headers; it does not build firmware or run hardware tests.

With `addlicense` installed, run the same check locally in Git Bash or another
shell with `xargs`:

```sh
git ls-files -z | xargs -0 addlicense -check -l apache -s -c "Maxim Pavlov"
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution requirements, build
checks, and license headers. Every contribution commit must include both a DCO
sign-off and a cryptographic signature: `git commit -s -S`.

## Releases

See [RELEASES.md](RELEASES.md) for the step-by-step GitHub release process and
SNAPSHOT -> release -> SNAPSHOT version flow. ESP Component Registry publication
is TBD.

## Sources and license

The source files are licensed under Apache-2.0; see [LICENSE](LICENSE).
Dependencies retain their own licenses.

- [Waveshare H-model documentation](https://www.waveshare.com/wiki/7inch_DSI_LCD_(H))
- [Waveshare register sequence and timings](https://github.com/waveshareteam/Waveshare-ESP32-components/tree/d081959d3841e0b370c2957c122bf8604ab42bc8/display/lcd/esp_lcd_dsi)
- [Waveshare GT911 and backlight reference](https://github.com/waveshareteam/Waveshare-ESP32-components/blob/d081959d3841e0b370c2957c122bf8604ab42bc8/bsp/esp32_p4_nano/esp32_p4_nano.c)
- [Function-EV v1.5.2 schematic](https://dl.espressif.com/dl/schematics/esp32-p4-function-ev-board-schematics_v1.52.pdf)
