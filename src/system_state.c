#include "system_state.h"

schedule_period_t schedule[MAX_PERIODS];
int schedule_count = 1;

// MQTT needed data
bool window = false;
bool door = false;
bool fan = false;
bool absorber_used = false;
system_mode_t current_mode = MODE_AUTO_BEST;

// indoor
float indoor_humidity = 0.0;
float indoor_temp = 0.0;
unsigned int indoor_aq = 0;

// outdoor
float outdoor_humidity = 0.0;
float outdoor_temp = 0.0;
unsigned int outdoor_aq = 0;

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
    if (current_mode == MODE_MANUAL)
    {
        return; // do nothing in MANUAL mode
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);

    // ---------------- AUTO_BEST ----------------
    if (current_mode == MODE_AUTO_BEST)
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
    else if (current_mode == MODE_AUTO_ECO)
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

    // ---------------- Ventilation Safety ----------------
    // Fan cannot be on if window is closed
    if (fan && !window)
    {
        fan = false;
    }

    // ---------------- Door ----------------
    door = (wind_speed > WIND_HIGH);

    xSemaphoreGive(state_mutex);
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
