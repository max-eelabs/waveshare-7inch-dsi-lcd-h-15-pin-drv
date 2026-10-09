/*
 * Copyright 2026 Maxim Pavlov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_ldo_regulator.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WAVESHARE_7INCH_H_DSI_15PIN_WIDTH 1280
#define WAVESHARE_7INCH_H_DSI_15PIN_HEIGHT 720
#define WAVESHARE_7INCH_H_DSI_15PIN_I2C_ADDRESS 0x45
#define WAVESHARE_7INCH_H_DSI_15PIN_POWER_GPIO_NONE (-1)

/**
 * @brief Configuration for the Waveshare 7inch DSI LCD (H).
 *
 * @param i2c_sda_gpio GPIO routed to the display connector's I2C SDA signal.
 * @param i2c_scl_gpio GPIO routed to the display connector's I2C SCL signal.
 * @param power_enable_gpio Active-high GPIO controlling the external 5 V
 *        relay, or WAVESHARE_7INCH_H_DSI_15PIN_POWER_GPIO_NONE when the panel is
 *        powered independently.
 * @param power_off_delay_ms Relay-off time before display power is enabled.
 * @param power_on_delay_ms Panel power-up time before I2C communication.
 */
typedef struct {
    int i2c_sda_gpio;
    int i2c_scl_gpio;
    int power_enable_gpio;
    uint32_t power_off_delay_ms;
    uint32_t power_on_delay_ms;
} waveshare_7inch_h_dsi_15pin_config_t;

/**
 * @brief Display resources created by waveshare_7inch_h_dsi_15pin_init().
 *
 * The handles are exposed so an application can attach LVGL and touch drivers
 * to the same DSI and I2C buses. Applications must not delete these handles;
 * use waveshare_7inch_h_dsi_15pin_deinit() instead. Remove all GUI input/display
 * adapters before deinit. Keep this struct alive until deinit; do not copy it.
 * Lifecycle operations must be serialized by the caller.
 */
typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t bridge_i2c;
    esp_ldo_channel_handle_t phy_power;
    esp_lcd_dsi_bus_handle_t dsi_bus;
    esp_lcd_panel_io_handle_t dsi_io;
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t touch_io;
    esp_lcd_touch_handle_t touch;
} waveshare_7inch_h_dsi_15pin_t;

/**
 * @brief Return defaults for the ESP32-P4 Function-EV board.
 */
waveshare_7inch_h_dsi_15pin_config_t waveshare_7inch_h_dsi_15pin_default_config(void);

/**
 * @brief Initialize the panel bridge and start the DSI video stream.
 *
 * When power_enable_gpio is set, this function resets the display through the
 * external 5 V relay before waiting for the panel to power up. The backlight
 * remains off until DSI video is running. Pass a fresh instance or one already
 * deinitialized; do not initialize an active instance. On failure, acquired
 * resources are released. An externally controlled relay remains powered on.
 */
esp_err_t waveshare_7inch_h_dsi_15pin_init(waveshare_7inch_h_dsi_15pin_t *display,
                                   const waveshare_7inch_h_dsi_15pin_config_t *config);

/**
 * @brief Initialize the on-panel GT911 touch controller without a GUI dependency.
 *
 * Requires an initialized display. Returns ESP_ERR_NOT_FOUND when neither GT911
 * address responds. Other failures are propagated; repeated calls return
 * ESP_ERR_INVALID_STATE. The landscape mapping is applied by esp_lcd_touch.
 * On success, display->touch can be attached to an application-owned GUI input.
 * The driver releases the touch controller and its I2C interface at deinit.
 */
esp_err_t waveshare_7inch_h_dsi_15pin_init_touch(waveshare_7inch_h_dsi_15pin_t *display);

/**
 * @brief Set backlight brightness.
 *
 * @param brightness 0 for off, 255 for maximum brightness.
 */
esp_err_t waveshare_7inch_h_dsi_15pin_set_brightness(waveshare_7inch_h_dsi_15pin_t *display,
                                             uint8_t brightness);

/**
 * @brief Show DSI-generated vertical color bars for a fixed duration.
 *
 * This checks the DSI video path independently of LVGL. Pass zero to skip it.
 */
esp_err_t waveshare_7inch_h_dsi_15pin_run_diagnostics(waveshare_7inch_h_dsi_15pin_t *display,
                                              uint32_t color_bar_duration_ms);

/**
 * @brief Stop and release resources created by waveshare_7inch_h_dsi_15pin_init().
 */
esp_err_t waveshare_7inch_h_dsi_15pin_deinit(waveshare_7inch_h_dsi_15pin_t *display);

#ifdef __cplusplus
}
#endif
