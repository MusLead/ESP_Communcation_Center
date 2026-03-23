#include "oled_status.h"

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_sh1106.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_state.h"
#include "wifi.h"

#ifndef OLED_STATUS_ENABLE
#define OLED_STATUS_ENABLE 1
#endif

#ifndef OLED_STATUS_I2C_PORT
#define OLED_STATUS_I2C_PORT 0
#endif

#ifndef OLED_STATUS_SDA_GPIO
#define OLED_STATUS_SDA_GPIO 21
#endif

#ifndef OLED_STATUS_SCL_GPIO
#define OLED_STATUS_SCL_GPIO 22
#endif

#ifndef OLED_STATUS_I2C_ADDR
#define OLED_STATUS_I2C_ADDR 0x3C
#endif

#ifndef OLED_STATUS_PIXEL_CLOCK_HZ
#define OLED_STATUS_PIXEL_CLOCK_HZ (400 * 1000)
#endif

#ifndef OLED_STATUS_UPDATE_INTERVAL_MS
#define OLED_STATUS_UPDATE_INTERVAL_MS 1000
#endif

#ifndef OLED_STATUS_WIDTH
#define OLED_STATUS_WIDTH 128
#endif

#ifndef OLED_STATUS_HEIGHT
#define OLED_STATUS_HEIGHT 64
#endif

#ifndef OLED_STATUS_INVERT
#define OLED_STATUS_INVERT 0
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "UNKNOWN_WIFI"
#endif

#define OLED_CHAR_WIDTH 4
#define OLED_CHAR_HEIGHT 6
#define OLED_LINE_PITCH 8
#define OLED_LINES 8
#define OLED_LINE_CHARS (OLED_STATUS_WIDTH / OLED_CHAR_WIDTH)

typedef struct
{
    char c;
    uint8_t rows[5];
} glyph_t;

typedef struct
{
    bool wifi_connected;
    char ip[16];
    bool indoor_connected;
    char indoor_ip[16];
    bool outdoor_connected;
    char outdoor_ip[16];
} oled_snapshot_t;

static const char *TAG = "OLED_STATUS";
static bool initialized = false;
static bool oled_available = false;
static esp_lcd_panel_handle_t panel_handle = NULL;
static uint8_t oled_buffer[OLED_STATUS_WIDTH * OLED_STATUS_HEIGHT / 8];

static const glyph_t glyphs[] = {
    {' ', {0x0, 0x0, 0x0, 0x0, 0x0}},
    {'-', {0x0, 0x0, 0x7, 0x0, 0x0}},
    {'.', {0x0, 0x0, 0x0, 0x0, 0x2}},
    {':', {0x0, 0x2, 0x0, 0x2, 0x0}},
    {'/', {0x1, 0x1, 0x2, 0x4, 0x4}},
    {'_', {0x0, 0x0, 0x0, 0x0, 0x7}},
    {'?', {0x6, 0x1, 0x2, 0x0, 0x2}},
    {'0', {0x7, 0x5, 0x5, 0x5, 0x7}},
    {'1', {0x2, 0x6, 0x2, 0x2, 0x7}},
    {'2', {0x7, 0x1, 0x7, 0x4, 0x7}},
    {'3', {0x7, 0x1, 0x3, 0x1, 0x7}},
    {'4', {0x5, 0x5, 0x7, 0x1, 0x1}},
    {'5', {0x7, 0x4, 0x7, 0x1, 0x7}},
    {'6', {0x7, 0x4, 0x7, 0x5, 0x7}},
    {'7', {0x7, 0x1, 0x1, 0x1, 0x1}},
    {'8', {0x7, 0x5, 0x7, 0x5, 0x7}},
    {'9', {0x7, 0x5, 0x7, 0x1, 0x7}},
    {'A', {0x2, 0x5, 0x7, 0x5, 0x5}},
    {'B', {0x6, 0x5, 0x6, 0x5, 0x6}},
    {'C', {0x3, 0x4, 0x4, 0x4, 0x3}},
    {'D', {0x6, 0x5, 0x5, 0x5, 0x6}},
    {'E', {0x7, 0x4, 0x6, 0x4, 0x7}},
    {'F', {0x7, 0x4, 0x6, 0x4, 0x4}},
    {'G', {0x3, 0x4, 0x5, 0x5, 0x3}},
    {'H', {0x5, 0x5, 0x7, 0x5, 0x5}},
    {'I', {0x7, 0x2, 0x2, 0x2, 0x7}},
    {'J', {0x1, 0x1, 0x1, 0x5, 0x2}},
    {'K', {0x5, 0x5, 0x6, 0x5, 0x5}},
    {'L', {0x4, 0x4, 0x4, 0x4, 0x7}},
    {'M', {0x5, 0x7, 0x7, 0x5, 0x5}},
    {'N', {0x5, 0x7, 0x7, 0x7, 0x5}},
    {'O', {0x2, 0x5, 0x5, 0x5, 0x2}},
    {'P', {0x6, 0x5, 0x6, 0x4, 0x4}},
    {'Q', {0x2, 0x5, 0x5, 0x2, 0x1}},
    {'R', {0x6, 0x5, 0x6, 0x5, 0x5}},
    {'S', {0x3, 0x4, 0x2, 0x1, 0x6}},
    {'T', {0x7, 0x2, 0x2, 0x2, 0x2}},
    {'U', {0x5, 0x5, 0x5, 0x5, 0x7}},
    {'V', {0x5, 0x5, 0x5, 0x5, 0x2}},
    {'W', {0x5, 0x5, 0x7, 0x7, 0x5}},
    {'X', {0x5, 0x5, 0x2, 0x5, 0x5}},
    {'Y', {0x5, 0x5, 0x2, 0x2, 0x2}},
    {'Z', {0x7, 0x1, 0x2, 0x4, 0x7}},
};

static const uint8_t *lookup_glyph(char c)
{
    size_t i;

    for (i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++)
    {
        if (glyphs[i].c == c)
        {
            return glyphs[i].rows;
        }
    }

    return glyphs[6].rows;
}

static void oled_set_pixel(int x, int y, bool on)
{
    size_t index;
    uint8_t mask;

    if (x < 0 || x >= OLED_STATUS_WIDTH || y < 0 || y >= OLED_STATUS_HEIGHT)
    {
        return;
    }

    index = (size_t)x + ((size_t)y / 8U) * OLED_STATUS_WIDTH;
    mask = (uint8_t)(1U << (y % 8));

    if (on)
    {
        oled_buffer[index] |= mask;
    }
    else
    {
        oled_buffer[index] &= (uint8_t)~mask;
    }
}

static void oled_draw_char(int x, int y, char c)
{
    const uint8_t *rows = lookup_glyph((char)toupper((unsigned char)c));
    int row;
    int col;

    for (row = 0; row < 5; row++)
    {
        for (col = 0; col < 3; col++)
        {
            oled_set_pixel(x + col, y + row, ((rows[row] >> (2 - col)) & 0x1U) != 0U);
        }
    }
}

static void oled_draw_text_line(int line, const char *text)
{
    int x = 0;
    int y = line * OLED_LINE_PITCH;
    int count = 0;

    if (line < 0 || line >= OLED_LINES || text == NULL)
    {
        return;
    }

    while (*text != '\0' && count < OLED_LINE_CHARS)
    {
        oled_draw_char(x, y, *text);
        x += OLED_CHAR_WIDTH;
        text++;
        count++;
    }
}

static void oled_format_line(char *dest, size_t dest_size, const char *fmt, ...)
{
    va_list args;
    size_t i;

    if (dest == NULL || dest_size == 0)
    {
        return;
    }

    va_start(args, fmt);
    vsnprintf(dest, dest_size, fmt, args);
    va_end(args);

    for (i = 0; dest[i] != '\0'; i++)
    {
        dest[i] = (char)toupper((unsigned char)dest[i]);
    }
}

static void capture_snapshot(oled_snapshot_t *snapshot)
{
    esp_ip4_addr_t ip;
    bool indoor_available = false;
    bool outdoor_available = false;
    bool wind_available = false;

    if (snapshot == NULL)
    {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));

    snapshot->wifi_connected = wifi_is_connected();
    ip = wifi_get_ip();
    snprintf(snapshot->ip, sizeof(snapshot->ip), IPSTR, IP2STR(&ip));

    if (state_mutex != NULL)
    {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        indoor_available = indoor_data_available;
        outdoor_available = outdoor_data_available;
        wind_available = wind_data_available;
        snprintf(snapshot->indoor_ip, sizeof(snapshot->indoor_ip), "%s", indoor_ip_address);
        snprintf(snapshot->outdoor_ip, sizeof(snapshot->outdoor_ip), "%s", outdoor_ip_address);
        xSemaphoreGive(state_mutex);
    }
    else
    {
        indoor_available = indoor_data_available;
        outdoor_available = outdoor_data_available;
        wind_available = wind_data_available;
        snprintf(snapshot->indoor_ip, sizeof(snapshot->indoor_ip), "%s", indoor_ip_address);
        snprintf(snapshot->outdoor_ip, sizeof(snapshot->outdoor_ip), "%s", outdoor_ip_address);
    }

    snapshot->indoor_connected = indoor_available;
    snapshot->outdoor_connected = outdoor_available || wind_available;
}

static esp_err_t oled_hw_init(void)
{
    esp_err_t err;
    i2c_master_bus_handle_t i2c_bus = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = OLED_STATUS_I2C_PORT,
        .sda_io_num = OLED_STATUS_SDA_GPIO,
        .scl_io_num = OLED_STATUS_SCL_GPIO,
        .flags.enable_internal_pullup = true,
    };
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = OLED_STATUS_I2C_ADDR,
        .scl_speed_hz = OLED_STATUS_PIXEL_CLOCK_HZ,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
    };

    err = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (err != ESP_OK)
    {
        return err;
    }

    err = esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = esp_lcd_new_panel_sh1106(io_handle, &panel_config, &panel_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = esp_lcd_panel_reset(panel_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = esp_lcd_panel_init(panel_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = esp_lcd_panel_invert_color(panel_handle, OLED_STATUS_INVERT);
    if (err != ESP_OK)
    {
        return err;
    }

    err = esp_lcd_panel_disp_on_off(panel_handle, true);
    if (err != ESP_OK)
    {
        return err;
    }

    oled_available = true;
    memset(oled_buffer, 0, sizeof(oled_buffer));
    return ESP_OK;
}

static void oled_render(const oled_snapshot_t *snapshot)
{
    char line[OLED_LINE_CHARS + 1];

    if (snapshot == NULL || !oled_available || panel_handle == NULL)
    {
        return;
    }

    memset(oled_buffer, 0, sizeof(oled_buffer));

    oled_format_line(line,
                     sizeof(line),
                     "WIFI %s",
                     snapshot->wifi_connected ? "CONNECTED" : "CONNECTING");
    oled_draw_text_line(0, line);

    oled_format_line(line, sizeof(line), "%.*s", OLED_LINE_CHARS, WIFI_SSID);
    oled_draw_text_line(1, line);

    oled_format_line(line, sizeof(line), "IP: %s", snapshot->wifi_connected ? snapshot->ip : "0.0.0.0");
    oled_draw_text_line(2, line);

    oled_format_line(line,
                     sizeof(line),
                     "INDOOR ESP: %s",
                     snapshot->indoor_connected ? "CONNECTED" : "MISSING");
    oled_draw_text_line(4, line);

    oled_format_line(line, sizeof(line), "IP: %s", snapshot->indoor_ip);
    oled_draw_text_line(5, line);

    oled_format_line(line,
                     sizeof(line),
                     "OUTDOOR ESP: %s",
                     snapshot->outdoor_connected ? "CONNECTED" : "MISSING");
    oled_draw_text_line(6, line);

    oled_format_line(line, sizeof(line), "IP: %s", snapshot->outdoor_ip);
    oled_draw_text_line(7, line);

    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, OLED_STATUS_WIDTH, OLED_STATUS_HEIGHT, oled_buffer);
}

static void oled_status_task(void *pvParameters)
{
    oled_snapshot_t snapshot;

    if (oled_hw_init() != ESP_OK)
    {
        ESP_LOGW(TAG,
                 "OLED init failed on SDA=%d SCL=%d ADDR=0x%02X. Continuing without display.",
                 OLED_STATUS_SDA_GPIO,
                 OLED_STATUS_SCL_GPIO,
                 OLED_STATUS_I2C_ADDR);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG,
             "OLED status screen enabled on I2C port %d (SDA=%d, SCL=%d, ADDR=0x%02X)",
             OLED_STATUS_I2C_PORT,
             OLED_STATUS_SDA_GPIO,
             OLED_STATUS_SCL_GPIO,
             OLED_STATUS_I2C_ADDR);

    while (1)
    {
        capture_snapshot(&snapshot);
        oled_render(&snapshot);
        vTaskDelay(pdMS_TO_TICKS(OLED_STATUS_UPDATE_INTERVAL_MS));
    }
}

void oled_status_init(void)
{
    if (initialized || !OLED_STATUS_ENABLE)
    {
        return;
    }

    xTaskCreate(oled_status_task, "oled_status_task", 4096, NULL, 1, NULL);
    initialized = true;
}
