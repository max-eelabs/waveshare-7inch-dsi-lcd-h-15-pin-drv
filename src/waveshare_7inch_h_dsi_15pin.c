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

// Public lifecycle: power the panel, configure the bridge, and start video.
// The display instance owns all acquired handles; callers serialize its use.
#include "waveshare_7inch_h_dsi_15pin_private.h"

#include <inttypes.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Continue releasing resources while preserving the first cleanup failure.
static void retain_first_error(esp_err_t *result, esp_err_t error)
{
    if (*result == ESP_OK && error != ESP_OK) {
        *result = error;
    }
}

// Function-EV connector defaults; external panel power needs no relay GPIO.
waveshare_7inch_h_dsi_15pin_config_t waveshare_7inch_h_dsi_15pin_default_config(void)
{
    return (waveshare_7inch_h_dsi_15pin_config_t){
        .i2c_sda_gpio = 7,
        .i2c_scl_gpio = 8,
        .power_enable_gpio = WAVESHARE_7INCH_H_DSI_15PIN_POWER_GPIO_NONE,
        .power_off_delay_ms = 250,
        .power_on_delay_ms = 250,
    };
}

// Cycle an optional active-high relay, then allow the panel to power up.
// Independently powered panels still need the configured startup delay.
static esp_err_t cycle_panel_power(const waveshare_7inch_h_dsi_15pin_config_t *config)
{
    if (config->power_enable_gpio != WAVESHARE_7INCH_H_DSI_15PIN_POWER_GPIO_NONE) {
        ESP_LOGI(WAVESHARE_DSI_TAG, "Resetting display through the 5 V relay");
        const gpio_config_t relay_config = {
            .pin_bit_mask = 1ULL << config->power_enable_gpio,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t error = gpio_config(&relay_config);
        ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "configure display relay");
        error = gpio_set_level(config->power_enable_gpio, 0);
        ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "turn display power off");
        vTaskDelay(pdMS_TO_TICKS(config->power_off_delay_ms));
        error = gpio_set_level(config->power_enable_gpio, 1);
        ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "turn display power on");
    }
    ESP_LOGI(WAVESHARE_DSI_TAG, "Waiting %" PRIu32 " ms for display power-up",
        config->power_on_delay_ms);

    vTaskDelay(pdMS_TO_TICKS(config->power_on_delay_ms));
    return ESP_OK;
}

// Initialize a fresh or deinitialized instance. Acquired handles stay in the
// instance so a failure at any stage can use the normal teardown path.
esp_err_t waveshare_7inch_h_dsi_15pin_init(waveshare_7inch_h_dsi_15pin_t *display,
    const waveshare_7inch_h_dsi_15pin_config_t *config)
{
    if (display == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(display, 0, sizeof(*display));

    if (!GPIO_IS_VALID_OUTPUT_GPIO(config->i2c_sda_gpio) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(config->i2c_scl_gpio) ||
        config->i2c_sda_gpio == config->i2c_scl_gpio ||
        (config->power_enable_gpio != WAVESHARE_7INCH_H_DSI_15PIN_POWER_GPIO_NONE &&
            (!GPIO_IS_VALID_OUTPUT_GPIO(config->power_enable_gpio) ||
                config->power_enable_gpio == config->i2c_sda_gpio ||
                config->power_enable_gpio == config->i2c_scl_gpio))) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t error = cycle_panel_power(config);
    if (error == ESP_OK) {
        error = waveshare_dsi_init_bridge(display, config);
    }
    if (error == ESP_OK) {
        error = waveshare_dsi_start_video(display);
    }
    // Keep the backlight off until the video path is ready.
    if (error == ESP_OK) {
        error = waveshare_7inch_h_dsi_15pin_set_brightness(display, UINT8_MAX);
    }
    if (error != ESP_OK) {
        // Report cleanup failures without hiding the original setup error.
        const esp_err_t cleanup_error = waveshare_7inch_h_dsi_15pin_deinit(display);
        if (cleanup_error != ESP_OK) {
            ESP_LOGW(WAVESHARE_DSI_TAG, "Initialization cleanup: %s",
                esp_err_to_name(cleanup_error));
        }
        return error;
    }
    ESP_LOGI(WAVESHARE_DSI_TAG, "Display ready");
    return ESP_OK;
}

// Callers must detach GUI adapters and stop using borrowed handles first.
// Releasing driver resources does not switch off the external power relay.
esp_err_t waveshare_7inch_h_dsi_15pin_deinit(waveshare_7inch_h_dsi_15pin_t *display)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t result = ESP_OK;

    // Reverse acquisition order. Guards also cover partial initialization.
    if (display->touch != NULL) {
        retain_first_error(&result, esp_lcd_touch_del(display->touch));
    }
    if (display->touch_io != NULL) {
        retain_first_error(&result, esp_lcd_panel_io_del(display->touch_io));
    }
    if (display->panel != NULL) {
        retain_first_error(&result, esp_lcd_panel_del(display->panel));
    }
    if (display->dsi_io != NULL) {
        retain_first_error(&result, esp_lcd_panel_io_del(display->dsi_io));
    }
    if (display->dsi_bus != NULL) {
        retain_first_error(&result, esp_lcd_del_dsi_bus(display->dsi_bus));
    }
    if (display->phy_power != NULL) {
        retain_first_error(&result, esp_ldo_release_channel(display->phy_power));
    }
    if (display->bridge_i2c != NULL) {
        retain_first_error(&result, i2c_master_bus_rm_device(display->bridge_i2c));
    }
    if (display->i2c_bus != NULL) {
        retain_first_error(&result, i2c_del_master_bus(display->i2c_bus));
    }
    // Invalidate every handle, including when a release operation failed.
    memset(display, 0, sizeof(*display));
    return result;
}
