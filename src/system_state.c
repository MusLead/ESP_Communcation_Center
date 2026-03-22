#include "system_state.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "esp_timer.h"
#include "sntp_time.h"
#include "mqtt_pub_sub.h"

static const char *TAG = "SYSTEM_STATE";

schedule_period_t schedule[MAX_PERIODS];
uint8_t schedule_count = 0;

// MQTT needed data
bool window = false;
bool door = false;
bool fan = false;
bool absorber_used = false;

// last states for change detection
static bool last_window = false;
static bool last_fan = false;
static bool last_door = false;
static bool last_absorber = false;

// mode
system_mode_t current_mode = MODE_MANUAL;

// indoor
float indoor_humidity = 0.0;
float indoor_temp = 0.0;
uint8_t indoor_aq = 0;
bool indoor_data_available = false;

// outdoor
float outdoor_humidity = 0.0;
float outdoor_temp = 0.0;
uint8_t outdoor_aq = 0;
bool outdoor_data_available = false;

float wind_speed = 0.0;
bool wind_data_available = false;

static int64_t indoor_last_update_ms = 0;
static int64_t outdoor_last_update_ms = 0;
static int64_t wind_last_update_ms = 0;
static char status_explanation[STATUS_EXPLANATION_MAX_LEN] = "Manual Mode, no Status explanation!";

SemaphoreHandle_t state_mutex;

static int64_t current_time_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void reset_indoor_sensor_state_locked(void)
{
    indoor_temp = 0.0f;
    indoor_humidity = 0.0f;
    indoor_aq = 0;
    indoor_data_available = false;
}

static void reset_outdoor_sensor_state_locked(void)
{
    outdoor_temp = 0.0f;
    outdoor_humidity = 0.0f;
    outdoor_aq = 0;
    outdoor_data_available = false;
}

static void reset_wind_sensor_state_locked(void)
{
    wind_speed = 0.0f;
    wind_data_available = false;
}

static void system_state_set_status_explanation_locked(const char *message)
{
    const char *safe_message = message;

    if (safe_message == NULL || safe_message[0] == '\0')
    {
        safe_message = "No status explanation available.";
    }

    snprintf(status_explanation, sizeof(status_explanation), "%s", safe_message);
}

static void system_state_set_status_explanationf_locked(const char *fmt, ...)
{
    va_list args;

    if (fmt == NULL || fmt[0] == '\0')
    {
        system_state_set_status_explanation_locked(NULL);
        return;
    }

    va_start(args, fmt);
    vsnprintf(status_explanation, sizeof(status_explanation), fmt, args);
    va_end(args);
}

static void system_state_append_status_explanationf_locked(const char *fmt, ...)
{
    char suffix[96];
    size_t len;
    va_list args;

    if (fmt == NULL || fmt[0] == '\0')
    {
        return;
    }

    va_start(args, fmt);
    vsnprintf(suffix, sizeof(suffix), fmt, args);
    va_end(args);

    len = strlen(status_explanation);

    if (len + 2 >= sizeof(status_explanation))
    {
        return;
    }

    snprintf(status_explanation + len,
             sizeof(status_explanation) - len,
             " %s",
             suffix);
}

void system_state_init()
{
    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == NULL)
    {
        printf("ERROR: Failed to create STATE mutex\n");
    }

    system_state_set_status_explanation("Manual Mode, no Status explanation!");
}

void system_state_update_indoor_sensor(float temp, float humidity, uint8_t aq)
{
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    indoor_temp = temp;
    indoor_humidity = humidity;
    indoor_aq = aq;
    indoor_data_available = true;
    indoor_last_update_ms = current_time_ms();
    xSemaphoreGive(state_mutex);
}

void system_state_update_outdoor_sensor(float temp, float humidity, uint8_t aq)
{
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    outdoor_temp = temp;
    outdoor_humidity = humidity;
    outdoor_aq = aq;
    outdoor_data_available = true;
    outdoor_last_update_ms = current_time_ms();
    xSemaphoreGive(state_mutex);
}

void system_state_update_wind_speed(float speed)
{
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    wind_speed = speed;
    wind_data_available = true;
    wind_last_update_ms = current_time_ms();
    xSemaphoreGive(state_mutex);
}

void system_state_refresh_sensor_timeouts(void)
{
    int64_t now = current_time_ms();
    bool indoor_timed_out = false;
    bool outdoor_timed_out = false;
    bool wind_timed_out = false;

    xSemaphoreTake(state_mutex, portMAX_DELAY);

    if (indoor_data_available && (now - indoor_last_update_ms) > INDOOR_SENSOR_TIMEOUT_MS)
    {
        reset_indoor_sensor_state_locked();
        indoor_timed_out = true;
    }

    if (outdoor_data_available && (now - outdoor_last_update_ms) > OUTDOOR_SENSOR_TIMEOUT_MS)
    {
        reset_outdoor_sensor_state_locked();
        outdoor_timed_out = true;
    }

    if (wind_data_available && (now - wind_last_update_ms) > WIND_SENSOR_TIMEOUT_MS)
    {
        reset_wind_sensor_state_locked();
        wind_timed_out = true;
    }

    xSemaphoreGive(state_mutex);

    if (indoor_timed_out)
    {
        ESP_LOGW(TAG, "Indoor sensor data timed out");
    }
    if (outdoor_timed_out)
    {
        ESP_LOGW(TAG, "Outdoor sensor data timed out");
    }
    if (wind_timed_out)
    {
        ESP_LOGW(TAG, "Wind sensor data timed out");
    }
}

void system_state_set_status_explanation(const char *message)
{
    if (state_mutex == NULL)
    {
        system_state_set_status_explanation_locked(message);
        return;
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    system_state_set_status_explanation_locked(message);
    xSemaphoreGive(state_mutex);
}

void system_state_set_status_explanationf(const char *fmt, ...)
{
    char message[STATUS_EXPLANATION_MAX_LEN];
    va_list args;

    if (fmt == NULL || fmt[0] == '\0')
    {
        system_state_set_status_explanation(NULL);
        return;
    }

    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    system_state_set_status_explanation(message);
}

void system_state_get_status_explanation(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0)
    {
        return;
    }

    if (state_mutex == NULL)
    {
        snprintf(buf, buf_size, "%s", status_explanation);
        return;
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    snprintf(buf, buf_size, "%s", status_explanation);
    xSemaphoreGive(state_mutex);
}

void publish_state(bool w, bool f, bool d, bool a)
{
    if (w != last_window)
        mqtt_publish("ESP32/window", w ? "1" : "0", 1);

    if (f != last_fan)
        mqtt_publish("ESP32/fan", f ? "1" : "0", 1);

    if (d != last_door)
        mqtt_publish("ESP32/door", d ? "1" : "0", 1);

    if (a != last_absorber)
        mqtt_publish("ESP32/absorber", a ? "1" : "0", 1);

    last_window = w;
    last_fan = f;
    last_door = d;
    last_absorber = a;
}

uint16_t time_to_minutes(const char *t)
{
    uint8_t hh = (t[0] - '0') * 10 + (t[1] - '0');
    uint8_t mm = (t[3] - '0') * 10 + (t[4] - '0');

    return hh * 60 + mm;
}

static bool schedule_is_active(system_mode_t *out_mode)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    uint16_t now_min = timeinfo.tm_hour * 60 + timeinfo.tm_min;

    for (uint8_t i = 0; i < schedule_count; i++)
    {
        uint16_t start = time_to_minutes(schedule[i].start);
        uint16_t end = time_to_minutes(schedule[i].end);

        if ((start <= end && now_min >= start && now_min <= end) ||
            (start > end && (now_min >= start || now_min <= end)))
        {
            *out_mode = schedule[i].mode;
            return true;
        }
    }
    return false;
}

static void apply_auto_logic(system_mode_t mode)
{
    // ---------------- AUTO_BEST ----------------
    if (mode == MODE_AUTO_BEST)
    {
        bool humidity_diff_high_enough = false;
        bool outdoor_air_is_good = false;

        if (!indoor_data_available || !outdoor_data_available)
        {
            window = false;
            fan = false;
            absorber_used = false;
            system_state_set_status_explanation_locked("AUTO_BEST: waiting for indoor and outdoor sensor data.");
            return;
        }

        humidity_diff_high_enough = (indoor_humidity - outdoor_humidity) > HUMIDITY_DIFF_THRESHOLD;
        outdoor_air_is_good = outdoor_aq <= GOOD_AQ;

        window = humidity_diff_high_enough && outdoor_air_is_good;
        fan = (window && indoor_aq > GOOD_AQ);
        absorber_used = (indoor_humidity > HIGH_HUMIDITY || indoor_aq > GOOD_AQ);

        if (fan)
        {
            system_state_set_status_explanationf_locked(
                "AUTO_BEST: window open and fan on because indoor humidity %.1f is higher than outdoor humidity %.1f and indoor air quality %u is above the threshold.",
                indoor_humidity,
                outdoor_humidity,
                indoor_aq);
        }
        else if (window)
        {
            system_state_set_status_explanationf_locked(
                "AUTO_BEST: window opened because indoor humidity %.1f is higher than outdoor humidity %.1f and outdoor air quality %u is acceptable.",
                indoor_humidity,
                outdoor_humidity,
                outdoor_aq);
        }
        else if (!outdoor_air_is_good)
        {
            system_state_set_status_explanationf_locked(
                "AUTO_BEST: window kept closed because outdoor air quality %u is above the threshold %u.",
                outdoor_aq,
                GOOD_AQ);
        }
        else
        {
            system_state_set_status_explanationf_locked(
                "AUTO_BEST: window kept closed because humidity difference %.1f is below the threshold %.1f.",
                indoor_humidity - outdoor_humidity,
                HUMIDITY_DIFF_THRESHOLD);
        }

        if (absorber_used)
        {
            system_state_append_status_explanationf_locked(
                "Absorber enabled because indoor humidity %.1f or air quality %u crossed the threshold.",
                indoor_humidity,
                indoor_aq);
        }
    }

    // ---------------- AUTO_ECO ----------------
    else if (mode == MODE_AUTO_ECO)
    {
        if (!outdoor_data_available)
        {
            window = false;
            fan = false;
            absorber_used = false;
            system_state_set_status_explanation_locked("AUTO_ECO: waiting for outdoor sensor data.");
            return;
        }

        if (outdoor_aq <= GOOD_AQ)
        {
            window = true;

            if (wind_data_available && wind_speed > WIND_HIGH)
            {
                fan = false;
                system_state_set_status_explanationf_locked(
                    "AUTO_ECO: window open, but fan off because wind speed %.1f is above the threshold %.1f.",
                    wind_speed,
                    WIND_HIGH);
            }
            else
            {
                if (!indoor_data_available)
                {
                    fan = false;
                    system_state_set_status_explanation_locked(
                        "AUTO_ECO: window open, waiting for indoor sensor data before deciding on the fan.");
                }
                else
                {
                    fan = (indoor_humidity > HIGH_HUMIDITY || indoor_temp > 25.0f) ? true : false;

                    if (fan)
                    {
                        system_state_set_status_explanationf_locked(
                            "AUTO_ECO: window open and fan on because indoor humidity %.1f or temperature %.1f crossed the limit.",
                            indoor_humidity,
                            indoor_temp);
                    }
                    else
                    {
                        system_state_set_status_explanation_locked(
                            "AUTO_ECO: window open and fan off because indoor climate is within the configured limits.");
                    }
                }
            }
        }
        else
        {
            window = false;
            fan = false;
            system_state_set_status_explanationf_locked(
                "AUTO_ECO: window kept closed because outdoor air quality %u is above the threshold %u.",
                outdoor_aq,
                GOOD_AQ);
        }

        absorber_used = (indoor_data_available &&
                         (indoor_humidity > HIGH_HUMIDITY || indoor_aq > GOOD_AQ));

        if (absorber_used)
        {
            system_state_append_status_explanationf_locked(
                "Absorber enabled because indoor humidity %.1f or air quality %u crossed the threshold.",
                indoor_humidity,
                indoor_aq);
        }
    }
}

void system_auto_update()
{
    bool w, f, d, a;
    system_mode_t mode;

    xSemaphoreTake(state_mutex, portMAX_DELAY);

    mode = current_mode;

    // -------- MANUAL --------
    if (mode == MODE_MANUAL)
    {
        system_state_set_status_explanation_locked("Manual Mode, no Status explanation!");
        xSemaphoreGive(state_mutex);
        return;
    }

    // -------- SCHEDULE --------
    if (schedule_count > 0)
    {
        if (!schedule_is_active(&mode))
        {
            // outside schedule -> everything off
            window = false;
            fan = false;
            door = false;
            absorber_used = false;
            system_state_set_status_explanation_locked("Schedule inactive: all actuators are off until the next scheduled period.");
            // TODO: if the schedule is inactive, the window, fan, door and absorber should be able to be manually controlled without the schedule overriding them until the next schedule period starts. This allows the user to have manual control during off-schedule hours without losing the schedule benefits during on-schedule hours.
            // make sure it does not overlapp with the manual status explanation above, maybe add a flag to the status explanation to indicate that the schedule is inactive and manual override is active until the next schedule period starts.
            // this could also be done in the Front-End by showing a message that the schedule is currently inactive and manual override is active until the next schedule period starts.
            // without setting mode to manual, the schedule will automatically take over again when the next schedule period starts, which is the desired behavior.
            
            w = window;
            f = fan;
            d = door;
            a = absorber_used;

            xSemaphoreGive(state_mutex);
            publish_state(w, f, d, a);
            return;
        }

        current_mode = mode;
    }

    // -------- AUTO LOGIC --------
    apply_auto_logic(mode);

    // Fan cannot be on if window is closed
    if (fan && !window)
    {
        fan = false;
    }

    // ---------------- Door ----------------
    door = (wind_data_available && wind_speed > WIND_HIGH);

    if (door)
    {
        system_state_append_status_explanationf_locked(
            "Door opened because wind speed %.1f is above the threshold %.1f.",
            wind_speed,
            WIND_HIGH);
    }

    w = window;
    f = fan;
    d = door;
    a = absorber_used;

    xSemaphoreGive(state_mutex);

    publish_state(w, f, d, a);
}

// System task
void system_task(void *pvParameters)
{
    while (1)
    {
        system_state_refresh_sensor_timeouts();
        system_auto_update();

        // 1 sec delay
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
