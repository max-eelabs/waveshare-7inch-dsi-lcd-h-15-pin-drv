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

#include "waveshare_7inch_h_dsi_15pin.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define SCREEN_WIDTH WAVESHARE_7INCH_H_DSI_15PIN_WIDTH
#define SCREEN_HEIGHT WAVESHARE_7INCH_H_DSI_15PIN_HEIGHT
#define HEADER_HEIGHT 64
#define FONT_SCALE 3
#define GLYPH_WIDTH 5
#define GLYPH_HEIGHT 7
#define TEXT_HEIGHT (GLYPH_HEIGHT * FONT_SCALE)
#define TEXT_TOP ((HEADER_HEIGHT - TEXT_HEIGHT) / 2)
#define RGB_BYTES_PER_PIXEL 3
#define TOUCH_POLL_INTERVAL_MS 20

static waveshare_7inch_h_dsi_15pin_t s_panel;
// Borrowed RGB888 framebuffer; the panel driver owns its lifetime.
static uint8_t *s_framebuffer;
// Render the small text strip in internal RAM before touching the live image.
static uint8_t s_text_buffer[SCREEN_WIDTH * TEXT_HEIGHT * RGB_BYTES_PER_PIXEL];

static bool IRAM_ATTR on_vsync(esp_lcd_panel_handle_t panel,
                              esp_lcd_dpi_panel_event_data_t *event,
                              void *user_context)
{
    (void)panel;
    (void)event;
    BaseType_t task_woken = pdFALSE;
    vTaskNotifyGiveFromISR((TaskHandle_t)user_context, &task_woken);
    return task_woken == pdTRUE;
}

// Five columns per glyph, with the least significant bit at the top.
static const uint8_t kDigits[10][GLYPH_WIDTH] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e},  // 0
    {0x00, 0x42, 0x7f, 0x40, 0x00},  // 1
    {0x42, 0x61, 0x51, 0x49, 0x46},  // 2
    {0x21, 0x41, 0x45, 0x4b, 0x31},  // 3
    {0x18, 0x14, 0x12, 0x7f, 0x10},  // 4
    {0x27, 0x45, 0x45, 0x45, 0x39},  // 5
    {0x3c, 0x4a, 0x49, 0x49, 0x30},  // 6
    {0x01, 0x71, 0x09, 0x05, 0x03},  // 7
    {0x36, 0x49, 0x49, 0x49, 0x36},  // 8
    {0x06, 0x49, 0x49, 0x29, 0x1e},  // 9
};
static const uint8_t kLetters[26][GLYPH_WIDTH] = {
    {0x7e, 0x11, 0x11, 0x11, 0x7e},  // A
    {0x7f, 0x49, 0x49, 0x49, 0x36},  // B
    {0x3e, 0x41, 0x41, 0x41, 0x22},  // C
    {0x7f, 0x41, 0x41, 0x22, 0x1c},  // D
    {0x7f, 0x49, 0x49, 0x49, 0x41},  // E
    {0x7f, 0x09, 0x09, 0x09, 0x01},  // F
    {0x3e, 0x41, 0x49, 0x49, 0x7a},  // G
    {0x7f, 0x08, 0x08, 0x08, 0x7f},  // H
    {0x00, 0x41, 0x7f, 0x41, 0x00},  // I
    {0x20, 0x40, 0x41, 0x3f, 0x01},  // J
    {0x7f, 0x08, 0x14, 0x22, 0x41},  // K
    {0x7f, 0x40, 0x40, 0x40, 0x40},  // L
    {0x7f, 0x02, 0x0c, 0x02, 0x7f},  // M
    {0x7f, 0x04, 0x08, 0x10, 0x7f},  // N
    {0x3e, 0x41, 0x41, 0x41, 0x3e},  // O
    {0x7f, 0x09, 0x09, 0x09, 0x06},  // P
    {0x3e, 0x41, 0x51, 0x21, 0x5e},  // Q
    {0x7f, 0x09, 0x19, 0x29, 0x46},  // R
    {0x46, 0x49, 0x49, 0x49, 0x31},  // S
    {0x01, 0x01, 0x7f, 0x01, 0x01},  // T
    {0x3f, 0x40, 0x40, 0x40, 0x3f},  // U
    {0x1f, 0x20, 0x40, 0x20, 0x1f},  // V
    {0x3f, 0x40, 0x38, 0x40, 0x3f},  // W
    {0x63, 0x14, 0x08, 0x14, 0x63},  // X
    {0x07, 0x08, 0x70, 0x08, 0x07},  // Y
    {0x61, 0x51, 0x49, 0x45, 0x43},  // Z
};

// Spaces and unsupported characters leave a blank cell in the fixed-width font.
static const uint8_t *find_glyph(char character)
{
    static const uint8_t colon[GLYPH_WIDTH] = {0x00, 0x36, 0x36, 0x00, 0x00};
    static const uint8_t dash[GLYPH_WIDTH] = {0x08, 0x08, 0x08, 0x08, 0x08};

    if (character >= '0' && character <= '9') {
        return kDigits[character - '0'];
    }
    if (character >= 'A' && character <= 'Z') {
        return kLetters[character - 'A'];
    }
    if (character == ':') {
        return colon;
    }
    if (character == '-') {
        return dash;
    }
    return NULL;
}

// Enlarge each font pixel into a square, writing white RGB888 pixels into the staging buffer.
static void draw_glyph(const uint8_t *glyph, int left, int top)
{
    for (int column = 0; column < GLYPH_WIDTH; ++column) {
        for (int row = 0; row < GLYPH_HEIGHT; ++row) {
            if ((glyph[column] & (1U << row)) == 0) {
                continue;
            }
            for (int dy = 0; dy < FONT_SCALE; ++dy) {
                for (int dx = 0; dx < FONT_SCALE; ++dx) {
                    const int x = left + column * FONT_SCALE + dx;
                    const int y = top + row * FONT_SCALE + dy;
                    uint8_t *pixel = s_text_buffer +
                        (y * SCREEN_WIDTH + x) * RGB_BYTES_PER_PIXEL;
                    memset(pixel, 255, RGB_BYTES_PER_PIXEL);
                }
            }
        }
    }
}

static void draw_text(const char *text)
{
    const int top = 0;
    int left = 16;

    for (; *text != '\0' && left + GLYPH_WIDTH * FONT_SCALE <= SCREEN_WIDTH;
         ++text, left += (GLYPH_WIDTH + 1) * FONT_SCALE) {
        const uint8_t *glyph = find_glyph(*text);
        if (glyph != NULL) {
            draw_glyph(glyph, left, top);
        }
    }
}

static void show_status(const char *text)
{
    memset(s_text_buffer, 0, sizeof(s_text_buffer));
    draw_text(text);

    // Discard old notifications and wait for a fresh frame boundary. Keep the
    // live update short: all clearing and glyph rendering happened off-screen.
    ulTaskNotifyTake(pdTRUE, 0);
    ESP_ERROR_CHECK(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) > 0
                        ? ESP_OK : ESP_ERR_TIMEOUT);
    uint8_t *destination = s_framebuffer +
        TEXT_TOP * SCREEN_WIDTH * RGB_BYTES_PER_PIXEL;
    memcpy(destination, s_text_buffer, sizeof(s_text_buffer));

    // Flush only the text rows for display DMA; leave the color bars untouched.
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
        s_panel.panel, 0, TEXT_TOP, SCREEN_WIDTH, TEXT_TOP + TEXT_HEIGHT,
        destination));
}

static void show_bars(void)
{
    static const uint8_t colors[8][3] = {
        {255, 255, 255},  // White
        {255, 255,   0},  // Yellow
        {  0, 255, 255},  // Cyan
        {  0, 255,   0},  // Green
        {255,   0, 255},  // Magenta
        {255,   0,   0},  // Red
        {  0,   0, 255},  // Blue
        {  0,   0,   0},  // Black
    };
    // Hardware test patterns cover the whole screen, so use RGB888 pixels
    // instead to leave room for a status line above the bars.
    for (int y = HEADER_HEIGHT; y < SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            memcpy(s_framebuffer + (y * SCREEN_WIDTH + x) * RGB_BYTES_PER_PIXEL,
                   colors[x * 8 / SCREEN_WIDTH], RGB_BYTES_PER_PIXEL);
        }
    }
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel.panel, 0, 0,
                                            SCREEN_WIDTH, SCREEN_HEIGHT,
                                            s_framebuffer));
}

typedef struct {
    bool pressed;
    bool have_coordinates;
    uint16_t last_x;
    uint16_t last_y;
    unsigned tap_count;
} touch_state_t;

// Only a new press increments the counter. Release retains the last position.
static void poll_touch(touch_state_t *state)
{
    if (s_panel.touch == NULL) {
        return;
    }

    // Use the same read API as the application's LVGL touch adapter.
    esp_lcd_touch_point_data_t point = {0};
    uint8_t count = 0;
    ESP_ERROR_CHECK(esp_lcd_touch_read_data(s_panel.touch));
    ESP_ERROR_CHECK(esp_lcd_touch_get_data(s_panel.touch, &point, &count, 1));

    const bool touching = count > 0;
    if (touching) {
        state->last_x = point.x;
        state->last_y = point.y;
        state->have_coordinates = true;
        if (!state->pressed) {
            ++state->tap_count;
        }
    }
    state->pressed = touching;
}

static void format_status(char *text, size_t capacity, const touch_state_t *state)
{
    const uint64_t seconds = (uint64_t)esp_timer_get_time() / 1000000;
    if (s_panel.touch == NULL) {
        snprintf(text, capacity, "UPTIME:%8" PRIu64 "S  GT911 NOT FOUND", seconds);
        return;
    }

    // Every field has a fixed width, so UP/DOWN and changing numeric values
    // do not shift the coordinates. Dashes mean no touch has been received yet.
    char x_text[8] = "----";
    char y_text[8] = "----";
    if (state->have_coordinates) {
        snprintf(x_text, sizeof(x_text), "%4u", state->last_x);
        snprintf(y_text, sizeof(y_text), "%4u", state->last_y);
    }
    snprintf(text, capacity,
             "UPTIME:%8" PRIu64 "S  TOUCH %-4s  X:%4s Y:%4s  TAPS:%5u",
             seconds, state->pressed ? "DOWN" : "UP", x_text, y_text,
             state->tap_count);
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_psram_is_initialized() ? ESP_OK : ESP_ERR_INVALID_STATE);

    // Select -1 for independent power, or the relay GPIO in the example config.
    waveshare_7inch_h_dsi_15pin_config_t config =
        waveshare_7inch_h_dsi_15pin_default_config();
    config.power_enable_gpio = CONFIG_WAVESHARE_EXAMPLE_POWER_GPIO;
    ESP_ERROR_CHECK(waveshare_7inch_h_dsi_15pin_init(&s_panel, &config));
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(
        s_panel.panel, 1, (void **)&s_framebuffer));

    const esp_lcd_dpi_panel_event_callbacks_t callbacks = {
        .on_vsync = on_vsync,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(
        s_panel.panel, &callbacks, xTaskGetCurrentTaskHandle()));

    show_bars();
    show_status("STARTING GT911");
    const esp_err_t touch_error = waveshare_7inch_h_dsi_15pin_init_touch(&s_panel);
    if (touch_error == ESP_ERR_NOT_FOUND) {
        ESP_LOGW("basic", "No GT911; running display only");
    } else {
        ESP_ERROR_CHECK(touch_error);
    }

    touch_state_t touch_state = {0};
    char previous_status[96] = "";
    while (true) {
        poll_touch(&touch_state);

        // Refresh only the header when its contents change. Color bars and
        // brightness remain steady throughout the touch test.
        char status[96];
        format_status(status, sizeof(status), &touch_state);
        if (strcmp(status, previous_status) != 0) {
            show_status(status);
            memcpy(previous_status, status, strlen(status) + 1);
        }
        vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_INTERVAL_MS));
    }
}
