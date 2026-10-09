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

// GT911 discovery and its native portrait-to-landscape coordinate mapping.
#include "waveshare_7inch_h_dsi_15pin.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_timer.h"

static const uint32_t kTouchTimeoutMs = 100;
static const int64_t kTouchLogIntervalUs = 500000;
static const char *const kTag = "waveshare_dsi";

// This callback observes native coordinates before esp_lcd_touch applies
// the configured axis transforms. It does not modify the reported points.
static void log_touch_coordinates(esp_lcd_touch_handle_t touch, uint16_t *x,
    uint16_t *y, uint16_t *strength, uint8_t *point_count, uint8_t max_points)
{
    (void)touch;
    (void)strength;
    (void)max_points;
    // Shared diagnostic throttle for the single supported display instance.
    // Touch reads must be serialized by the application.
    static int64_t last_log_us;
    const int64_t now_us = esp_timer_get_time();
    if (*point_count > 0 && now_us - last_log_us >= kTouchLogIntervalUs) {
        ESP_LOGI(kTag, "GT911 raw touch: x=%u y=%u; landscape: x=%u y=%u", x[0], y[0],
            y[0], WAVESHARE_7INCH_H_DSI_15PIN_HEIGHT - x[0]);
        last_log_us = now_us;
    }
}

// Try the backup address first. Only an absent device permits fallback;
// propagate bus errors rather than treating them as a missing controller.
static esp_err_t find_touch_address(i2c_master_bus_handle_t i2c_bus, uint8_t *address)
{
    *address = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
    esp_err_t error = i2c_master_probe(i2c_bus, *address, kTouchTimeoutMs);
    if (error != ESP_ERR_NOT_FOUND) {
        return error;
    }

    *address = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
    error = i2c_master_probe(i2c_bus, *address, kTouchTimeoutMs);
    if (error == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(kTag, "GT911 not found at 0x14 or 0x5D");
        return ESP_ERR_NOT_FOUND;
    }
    return error;
}

// Attach touch to the display-owned I2C bus after video initialization.
// Reject repeated calls so existing touch handles cannot be overwritten.
esp_err_t waveshare_7inch_h_dsi_15pin_init_touch(waveshare_7inch_h_dsi_15pin_t *display)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (display->i2c_bus == NULL || display->panel == NULL ||
        display->touch_io != NULL || display->touch != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t address;
    esp_err_t error = find_touch_address(display->i2c_bus, &address);
    ESP_RETURN_ON_ERROR(error, kTag, "find GT911 touch controller");

    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_config.dev_addr = address;
    error = esp_lcd_new_panel_io_i2c(display->i2c_bus, &io_config, &display->touch_io);
    ESP_RETURN_ON_ERROR(error, kTag, "create GT911 I2C interface");

    // The GT911 driver retains this pointer for the touch device lifetime.
    static esp_lcd_touch_io_gt911_config_t primary_config = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
    };
    static esp_lcd_touch_io_gt911_config_t backup_config = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP,
    };
    // Native coordinates span 720 by 1280. Mirror the native X axis, then
    // swap axes to match the landscape video orientation.
    const esp_lcd_touch_config_t touch_config = {
        .x_max = WAVESHARE_7INCH_H_DSI_15PIN_HEIGHT,
        .y_max = WAVESHARE_7INCH_H_DSI_15PIN_WIDTH,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .driver_data = address == ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS ? &primary_config
                                                                     : &backup_config,
        .process_coordinates = log_touch_coordinates,
        .flags = {
            .swap_xy = true,
            .mirror_x = true,
        },
    };

    error =
        esp_lcd_touch_new_i2c_gt911(display->touch_io, &touch_config, &display->touch);
    if (error != ESP_OK) {
        // Roll back this attempt without tearing down the working display.
        esp_lcd_panel_io_del(display->touch_io);
        display->touch_io = NULL;
        return error;
    }

    ESP_LOGI(kTag, "GT911 touch ready at 0x%02X; swap_xy=1 mirror_x=1", address);
    return ESP_OK;
}
