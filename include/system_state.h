#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* -------------------- CONFIG CONSTANTS -------------------- */

#define HUMIDITY_DIFF_THRESHOLD 10.0f
#define HIGH_HUMIDITY 70.0f
#define GOOD_AQ 50
#define WIND_HIGH 5.0f
#define MAX_PERIODS 8

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

// outdoor sensors
extern float outdoor_humidity;
extern float outdoor_temp;
extern uint8_t outdoor_aq;

extern float wind_speed;

// mutex
extern SemaphoreHandle_t state_mutex;

/* -------------------- API -------------------- */

void system_state_init(void);
void system_auto_update(void);
void system_task(void *pvParameters);
