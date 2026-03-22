#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* -------------------- CONFIG CONSTANTS -------------------- */

#define HUMIDITY_DIFF_THRESHOLD 10.0f
#define HIGH_HUMIDITY 20.0f
#define GOOD_AQ 20
#define WIND_HIGH 5.0f
#define MAX_PERIODS 8
#define INDOOR_SENSOR_TIMEOUT_MS 1000
#define OUTDOOR_SENSOR_TIMEOUT_MS 1000
#define WIND_SENSOR_TIMEOUT_MS 5000

/* -------------------- MODES -------------------- */

typedef enum
{
    MODE_AUTO_BEST = 0,
    MODE_AUTO_ECO = 1,
    MODE_MANUAL = 2
} system_mode_t;

/* -------------------- SCHEDULE -------------------- */

typedef struct
{
    char start[7]; // "HH:MM"
    char end[7];
    system_mode_t mode;
} schedule_period_t;

/* -------------------- GLOBAL STATE -------------------- */

// schedule
extern schedule_period_t schedule[MAX_PERIODS]; // MAYBE MAKE A NODE LIST   -> [] -> [] -> Null
extern uint8_t schedule_count;

// actuators
extern bool window;        // false = closed, true = open
extern bool door;          // false = closed, true = open
extern bool fan;           // false = off, true = on
extern bool absorber_used; // false = off, true = on

// mode
extern system_mode_t current_mode;

// indoor sensors
extern float indoor_humidity;
extern float indoor_temp;
extern uint8_t indoor_aq;
extern bool indoor_data_available;

// outdoor sensors
extern float outdoor_humidity;
extern float outdoor_temp;
extern uint8_t outdoor_aq;
extern bool outdoor_data_available;

extern float wind_speed;
extern bool wind_data_available;

// mutex
extern SemaphoreHandle_t state_mutex;

/* -------------------- API -------------------- */

void publish_state(bool w, bool f, bool d, bool a);
void system_state_init(void);
void system_state_update_indoor_sensor(float temp, float humidity, uint8_t aq);
void system_state_update_outdoor_sensor(float temp, float humidity, uint8_t aq);
void system_state_update_wind_speed(float speed);
void system_state_refresh_sensor_timeouts(void);
void system_auto_update(void);
void system_task(void *pvParameters);
