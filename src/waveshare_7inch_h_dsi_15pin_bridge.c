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

// I2C bridge wake-up, register setup, and inverted backlight control.
#include "waveshare_7inch_h_dsi_15pin_private.h"

#include <stddef.h>
#include <stdint.h>

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum {
    kBridgeBacklightRegister = 0xAB,
    kBridgeOutputRegister = 0xAD,
};
static const uint32_t kBridgeTimeoutMs = 100;
static const uint32_t kBridgeOutputSettleDelayMs = 200;
static const uint32_t kBridgeSetupDelayMs = 1500;

typedef struct {
    uint8_t reg;
    uint8_t value;
} bridge_register_write_t;

// Preserve the bridge setup order. The initial 0xFF backlight value keeps
// the panel dark until the public initializer has started video.
static const bridge_register_write_t kBridgeSetup[] = {
    {0xC0, 0x01},
    {0xC2, 0x01},
    {0xAC, 0x01},
    {kBridgeBacklightRegister, 0xFF},
    {0xAA, 0x01},
    {kBridgeOutputRegister, 0x01},
};

// Each transfer writes one register address followed by its byte value.
static esp_err_t write_bridge_register(
    waveshare_7inch_h_dsi_15pin_t *display, uint8_t reg, uint8_t value)
{
    const uint8_t command[] = {reg, value};
    return i2c_master_transmit(
        display->bridge_i2c, command, sizeof(command), kBridgeTimeoutMs);
}

// The public range increases with brightness; the bridge range decreases.
esp_err_t waveshare_7inch_h_dsi_15pin_set_brightness(
    waveshare_7inch_h_dsi_15pin_t *display, uint8_t brightness)
{
    if (display == NULL || display->bridge_i2c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return write_bridge_register(
        display, kBridgeBacklightRegister, UINT8_MAX - brightness);
}

// The display owns I2C0 and the bridge device. Leave partial acquisitions
// attached to it so the public initializer can unwind on failure.
esp_err_t waveshare_dsi_init_bridge(waveshare_7inch_h_dsi_15pin_t *display,
    const waveshare_7inch_h_dsi_15pin_config_t *config)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = config->i2c_sda_gpio,
        .scl_io_num = config->i2c_scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t error = i2c_new_master_bus(&bus_config, &display->i2c_bus);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "create panel I2C bus");

    // A probe wakes the bridge after a true 5 V cycle, including cold boot.
    error = i2c_master_probe(
        display->i2c_bus, WAVESHARE_7INCH_H_DSI_15PIN_I2C_ADDRESS, kBridgeTimeoutMs);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "wake panel bridge");

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = WAVESHARE_7INCH_H_DSI_15PIN_I2C_ADDRESS,
        .scl_speed_hz = 100000,
    };
    error = i2c_master_bus_add_device(
        display->i2c_bus, &device_config, &display->bridge_i2c);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "add panel bridge I2C device");

    error = write_bridge_register(display, kBridgeOutputRegister, 0x00);
    ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "disable bridge output");
    vTaskDelay(pdMS_TO_TICKS(kBridgeOutputSettleDelayMs));

    const size_t setup_count = sizeof(kBridgeSetup) / sizeof(kBridgeSetup[0]);
    for (size_t index = 0; index < setup_count; ++index) {
        const bridge_register_write_t *entry = &kBridgeSetup[index];
        error = write_bridge_register(display, entry->reg, entry->value);
        ESP_RETURN_ON_ERROR(error, WAVESHARE_DSI_TAG, "configure panel bridge");
    }

    // Allow bridge setup to settle before sending DSI panel commands.
    vTaskDelay(pdMS_TO_TICKS(kBridgeSetupDelayMs));
    return ESP_OK;
}
