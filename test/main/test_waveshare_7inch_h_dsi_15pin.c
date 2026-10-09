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

#include "unity.h"
#include "waveshare_7inch_h_dsi_15pin.h"

TEST_CASE("Waveshare 7inch DSI defaults match the EV-board wiring", "[waveshare_7inch_h_dsi_15pin]")
{
    const waveshare_7inch_h_dsi_15pin_config_t config = waveshare_7inch_h_dsi_15pin_default_config();

    TEST_ASSERT_EQUAL_INT(7, config.i2c_sda_gpio);
    TEST_ASSERT_EQUAL_INT(8, config.i2c_scl_gpio);
    TEST_ASSERT_EQUAL_INT(WAVESHARE_7INCH_H_DSI_15PIN_POWER_GPIO_NONE, config.power_enable_gpio);
    TEST_ASSERT_EQUAL_UINT32(250, config.power_off_delay_ms);
    TEST_ASSERT_EQUAL_UINT32(250, config.power_on_delay_ms);
    TEST_ASSERT_EQUAL(1280, WAVESHARE_7INCH_H_DSI_15PIN_WIDTH);
    TEST_ASSERT_EQUAL(720, WAVESHARE_7INCH_H_DSI_15PIN_HEIGHT);
    TEST_ASSERT_EQUAL_HEX8(0x45, WAVESHARE_7INCH_H_DSI_15PIN_I2C_ADDRESS);
}

TEST_CASE("driver rejects invalid arguments without accessing hardware", "[waveshare_7inch_h_dsi_15pin]")
{
    waveshare_7inch_h_dsi_15pin_t display = {0};
    waveshare_7inch_h_dsi_15pin_config_t config = waveshare_7inch_h_dsi_15pin_default_config();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, waveshare_7inch_h_dsi_15pin_init(NULL, &config));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, waveshare_7inch_h_dsi_15pin_init(&display, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, waveshare_7inch_h_dsi_15pin_init_touch(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, waveshare_7inch_h_dsi_15pin_deinit(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, waveshare_7inch_h_dsi_15pin_set_brightness(NULL, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, waveshare_7inch_h_dsi_15pin_set_brightness(&display, 255));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, waveshare_7inch_h_dsi_15pin_run_diagnostics(NULL, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, waveshare_7inch_h_dsi_15pin_run_diagnostics(&display, 1));
    config.power_enable_gpio = 64;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, waveshare_7inch_h_dsi_15pin_init(&display, &config));
    config = waveshare_7inch_h_dsi_15pin_default_config();
    config.i2c_scl_gpio = config.i2c_sda_gpio;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, waveshare_7inch_h_dsi_15pin_init(&display, &config));
}

TEST_CASE("empty driver cleanup is repeatable", "[waveshare_7inch_h_dsi_15pin]")
{
    waveshare_7inch_h_dsi_15pin_t display = {0};
    TEST_ASSERT_EQUAL(ESP_OK, waveshare_7inch_h_dsi_15pin_deinit(&display));
    TEST_ASSERT_EQUAL(ESP_OK, waveshare_7inch_h_dsi_15pin_deinit(&display));
    TEST_ASSERT_NULL(display.i2c_bus);
    TEST_ASSERT_NULL(display.panel);
    TEST_ASSERT_NULL(display.touch);
}

TEST_CASE("touch initialization requires panel and rejects existing touch", "[waveshare_7inch_h_dsi_15pin]")
{
    waveshare_7inch_h_dsi_15pin_t display = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, waveshare_7inch_h_dsi_15pin_init_touch(&display));
    /* Sentinel handles are never dereferenced: these paths must reject early. */
    int sentinel;
    display.i2c_bus = (i2c_master_bus_handle_t)&sentinel;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, waveshare_7inch_h_dsi_15pin_init_touch(&display));
    display.panel = (esp_lcd_panel_handle_t)&sentinel;
    display.touch = (esp_lcd_touch_handle_t)&sentinel;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, waveshare_7inch_h_dsi_15pin_init_touch(&display));
    display.touch = NULL;
    display.touch_io = (esp_lcd_panel_io_handle_t)&sentinel;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, waveshare_7inch_h_dsi_15pin_init_touch(&display));
}
