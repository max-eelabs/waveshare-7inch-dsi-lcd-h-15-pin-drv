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

// DSI transport, RGB888 video timing, panel commands, and diagnostics.
#include "waveshare_7inch_h_dsi_15pin_private.h"

#include <stdint.h>

#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const int kPhyLdoChannel = 3;
static const int kPhyLdoVoltageMv = 2500;
static const int kDsiLaneRateMbps = 1200;
static const int kDsiPixelClockMhz = 80;
static const uint32_t kSleepOutDelayMs = 120;
static const uint32_t kDisplayOnDelayMs = 20;

// Acquire PHY power, transport, command I/O, and the framebuffer in order.
// Store each handle in the display for cleanup by the public initializer.
esp_err_t waveshare_dsi_start_video(waveshare_7inch_h_dsi_15pin_t *display)
{
    const esp_ldo_channel_config_t phy_config = {
        .chan_id = kPhyLdoChannel,
        .voltage_mv = kPhyLdoVoltageMv,
    };
    esp_err_t error = esp_ldo_acquire_channel(&phy_config, &display->phy_power);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "enable DSI PHY LDO3");

    const esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
        .phy_clk_src = 0,
        .lane_bit_rate_mbps = kDsiLaneRateMbps,
    };

    error = esp_lcd_new_dsi_bus(&bus_config, &display->dsi_bus);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "create DSI bus");

    // DBI carries panel commands on the same DSI bus used for DPI video.
    const esp_lcd_dbi_io_config_t command_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };

    error =
        esp_lcd_new_panel_io_dbi(display->dsi_bus, &command_config, &display->dsi_io);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "create DSI command interface");

    // One RGB888 framebuffer supplies the fixed landscape timing. Applications
    // borrow it from the panel and must flush CPU writes for display DMA.
    const esp_lcd_dpi_panel_config_t panel_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = kDsiPixelClockMhz,
        .in_color_format = LCD_COLOR_FMT_RGB888,
        .out_color_format = LCD_COLOR_FMT_RGB888,
        .num_fbs = 1,
        .video_timing = {
            .h_size = WAVESHARE_7INCH_H_DSI_15PIN_WIDTH,
            .v_size = WAVESHARE_7INCH_H_DSI_15PIN_HEIGHT,
            .hsync_pulse_width = 64,
            .hsync_back_porch = 64,
            .hsync_front_porch = 64,
            .vsync_pulse_width = 64,
            .vsync_back_porch = 64,
            .vsync_front_porch = 64,
        },
    };
    error = esp_lcd_new_panel_dpi(display->dsi_bus, &panel_config, &display->panel);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "create DPI panel");
    error = esp_lcd_dpi_panel_enable_dma2d(display->panel);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "enable DMA2D");

    // Preserve the panel command payloads and required wake-up delays.
    // Start framebuffer output only after the display-on command has settled.
    const uint8_t zero = 0;
    error = esp_lcd_panel_io_tx_param(display->dsi_io, LCD_CMD_MADCTL, &zero, 1);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "send DSI MADCTL");
    error = esp_lcd_panel_io_tx_param(display->dsi_io, LCD_CMD_SLPOUT, &zero, 1);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "send DSI sleep out");
    vTaskDelay(pdMS_TO_TICKS(kSleepOutDelayMs));
    error = esp_lcd_panel_io_tx_param(display->dsi_io, LCD_CMD_DISPON, &zero, 1);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "send DSI display on");
    vTaskDelay(pdMS_TO_TICKS(kDisplayOnDelayMs));
    return esp_lcd_panel_init(display->panel);
}

// Temporarily replace framebuffer video with DSI-generated color bars.
// This blocks the calling task; a zero duration leaves the output untouched.
esp_err_t waveshare_7inch_h_dsi_15pin_run_diagnostics(
    waveshare_7inch_h_dsi_15pin_t *display, uint32_t color_bar_duration_ms)
{
    if (display == NULL || display->panel == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (color_bar_duration_ms == 0) {
        return ESP_OK;
    }
    ESP_LOGI(WAVESHARE_DSI_TAG, "Showing DSI hardware color bars");
    esp_err_t error =
        esp_lcd_dpi_panel_set_pattern(display->panel, MIPI_DSI_PATTERN_BAR_VERTICAL);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "enable DSI hardware color bars");
    vTaskDelay(pdMS_TO_TICKS(color_bar_duration_ms));
    return esp_lcd_dpi_panel_set_pattern(display->panel, MIPI_DSI_PATTERN_NONE);
}
