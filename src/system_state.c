#include "system_state.h"
#include <time.h>
#include <string.h>
#include "sntp_time.h"

static bool schedule_is_active(system_mode_t *out_mode);
static void apply_auto_logic(system_mode_t mode);

schedule_period_t schedule[MAX_PERIODS];
uint8_t schedule_count = 0;

// MQTT needed data
bool window = false;
bool door = false;
bool fan = false;
bool absorber_used = false;

// mode
system_mode_t current_mode = MODE_MANUAL;

// indoor
float indoor_humidity = 0.0;
float indoor_temp = 0.0;
uint8_t indoor_aq = 0;

// outdoor
float outdoor_humidity = 0.0;
float outdoor_temp = 0.0;
uint8_t outdoor_aq = 0;

float wind_speed = 0.0;

SemaphoreHandle_t state_mutex;

void system_state_init()
{
    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == NULL)
    {
        printf("ERROR: Failed to create STATE mutex\n");
    }
}

void system_auto_update()
{
    xSemaphoreTake(state_mutex, portMAX_DELAY);

    // -------- MANUAL --------
    if (current_mode == MODE_MANUAL)
    {
        xSemaphoreGive(state_mutex);
        return;
    }

    // -------- SCHEDULE --------
    system_mode_t scheduled_mode = current_mode;

    if (schedule_count > 0)
    {
        if (!schedule_is_active(&scheduled_mode))
        {
            // outside schedule → everything off
            window = false;
            fan = false;
            absorber_used = false;
            door = false;

            xSemaphoreGive(state_mutex);
            return;
        }
        // Fixed: Apply scheduled mode
        current_mode = scheduled_mode;

        // apply_auto_logic(scheduled_mode);
        // instead of
        // current_mode = scheduled_mode;
    }

    // -------- AUTO LOGIC --------
    apply_auto_logic(scheduled_mode);

    // Fan cannot be on if window is closed
    if (fan && !window)
    {
        fan = false;
    }

    // ---------------- Door ----------------
    door = (wind_speed > WIND_HIGH);

    xSemaphoreGive(state_mutex);
}

uint16_t time_to_minutes(const char *t)
{
    uint8_t hh = (t[0] - '0') * 10 + (t[1] - '0');
    uint8_t mm = (t[3] - '0') * 10 + (t[4] - '0');

    return (uint16_t)(hh * 60 + mm);
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
        // Windowlogik
        window = (indoor_humidity - outdoor_humidity > HUMIDITY_DIFF_THRESHOLD &&
                  outdoor_aq <= GOOD_AQ); // strcmp(outdoor_aq_level,"good")==0
        // Fanlogik
        fan = (window && indoor_aq > GOOD_AQ);

        // Absorberlogik
        absorber_used = (indoor_humidity > HIGH_HUMIDITY);
    }

    // ---------------- AUTO_ECO ----------------
    else if (mode == MODE_AUTO_ECO)
    {
        // window: open if outdoor AQ is good
        if (outdoor_aq <= GOOD_AQ)
        {
            window = true;

            if (wind_speed > WIND_HIGH)
            {
                fan = false;
            }
            else
            {
                fan = (indoor_humidity > HIGH_HUMIDITY || indoor_temp > 25.0) ? 1 : 0;
            }
        }
        else
        {
            // bad outdoor AQ -> close window and turn off fan
            window = false;
            fan = false;
        }

        absorber_used = (indoor_humidity > HIGH_HUMIDITY);
    }
}

// System task
void system_task(void *pvParameters)
{
    while (1)
    {
        //
        system_auto_update();

        // 1 sec delay
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}