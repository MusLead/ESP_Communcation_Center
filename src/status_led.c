#include "status_led.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef STATUS_LED_GPIO
#define STATUS_LED_GPIO 5
#endif

#ifndef STATUS_LED_ACTIVE_LEVEL
#define STATUS_LED_ACTIVE_LEVEL 0
#endif

#ifndef STATUS_LED_BLINK_INTERVAL_MS
#define STATUS_LED_BLINK_INTERVAL_MS 250
#endif

static const char *TAG = "STATUS_LED";

static volatile bool wifi_connected = false;
static volatile bool remote_connected = false;
static bool require_remote_connection = false;
static bool initialized = false;

static void status_led_apply(bool on)
{
    gpio_set_level((gpio_num_t)STATUS_LED_GPIO,
                   on ? STATUS_LED_ACTIVE_LEVEL : !STATUS_LED_ACTIVE_LEVEL);
}

static void status_led_task(void *pvParameters)
{
    bool led_on = false;

    while (1)
    {
        bool healthy = wifi_connected && (!require_remote_connection || remote_connected);

        if (healthy)
        {
            led_on = false;
            status_led_apply(false);
        }
        else
        {
            led_on = !led_on;
            status_led_apply(led_on);
        }

        vTaskDelay(pdMS_TO_TICKS(STATUS_LED_BLINK_INTERVAL_MS));
    }
}

void status_led_init(bool should_require_remote_connection)
{
    if (initialized)
    {
        require_remote_connection = should_require_remote_connection;
        return;
    }

    require_remote_connection = should_require_remote_connection;

    gpio_reset_pin((gpio_num_t)STATUS_LED_GPIO);
    gpio_set_direction((gpio_num_t)STATUS_LED_GPIO, GPIO_MODE_OUTPUT);
    status_led_apply(false);

    xTaskCreate(status_led_task, "status_led_task", 2048, NULL, 1, NULL);

    initialized = true;

    ESP_LOGI(TAG,
             "Status LED enabled on GPIO%d (active level=%d, require_remote=%s)",
             STATUS_LED_GPIO,
             STATUS_LED_ACTIVE_LEVEL,
             require_remote_connection ? "true" : "false");
}

void status_led_set_wifi_connected(bool connected)
{
    wifi_connected = connected;
}

void status_led_set_remote_connected(bool connected)
{
    remote_connected = connected;
}
