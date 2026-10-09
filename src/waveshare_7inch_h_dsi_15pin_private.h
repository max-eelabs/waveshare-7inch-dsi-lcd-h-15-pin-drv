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

/**
 * @file waveshare_7inch_h_dsi_15pin_private.h
 * @brief Internal bridge and video initialization interfaces.
 */

#pragma once

#include "waveshare_7inch_h_dsi_15pin.h"

/** @brief Shared log tag for lifecycle, bridge, and video operations. */
#define WAVESHARE_DSI_TAG "waveshare_dsi"

/**
 * @brief Create the panel I2C bus, wake the bridge, and configure its registers.
 *
 * The setup sequence keeps the backlight off until video is ready. This
 * function blocks while the bridge settles.
 *
 * @param[in,out] display Display instance receiving the owned I2C bus and
 *                       bridge device handles.
 * @param[in] config Validated GPIO configuration for the panel I2C bus.
 *
 * @pre Both pointers are non-null, and the GPIO configuration is valid.
 * @pre The display is powered and its I2C handles are not yet acquired.
 * @pre The caller serializes access to the display instance.
 *
 * @return ESP_OK on success, otherwise the first I2C setup or transfer error.
 * @note Partial acquisitions remain in display on failure. The caller must
 *       release them with waveshare_7inch_h_dsi_15pin_deinit().
 */
esp_err_t waveshare_dsi_init_bridge(waveshare_7inch_h_dsi_15pin_t *display,
    const waveshare_7inch_h_dsi_15pin_config_t *config);

/**
 * @brief Acquire DSI resources and start the RGB888 landscape video stream.
 *
 * Acquires PHY power, the DSI bus, command I/O, and the DPI panel in that order.
 * Sends the panel wake-up commands and waits for their settling delays before
 * initializing video output. The caller enables the backlight afterward.
 *
 * @param[in,out] display Display instance receiving the owned PHY power, DSI
 *                       bus, command I/O, and DPI panel handles.
 *
 * @pre display is non-null and bridge initialization has succeeded.
 * @pre The display's video resources are not yet acquired.
 * @pre The caller serializes access to the display instance.
 *
 * @return ESP_OK on success, otherwise the first PHY power, DSI setup, panel
 *         command, or panel initialization error.
 * @note Partial acquisitions remain in display on failure. The caller must
 *       release them with waveshare_7inch_h_dsi_15pin_deinit().
 */
esp_err_t waveshare_dsi_start_video(waveshare_7inch_h_dsi_15pin_t *display);
