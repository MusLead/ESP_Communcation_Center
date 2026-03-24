#pragma once

#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define STATUS_EXPLANATION_MAX_LEN 256

void system_status_init(SemaphoreHandle_t mutex);

void system_status_set_headline(const char *message);
void system_status_set_message(const char *headline, const char *message);
void system_status_set_explanation(const char *message);
void system_status_set_explanationf(const char *fmt, ...);
void system_status_get_headline(char *buf, size_t buf_size);
void system_status_get_explanation(char *buf, size_t buf_size);

/* Use the locked variants only when the caller already holds the shared state mutex. */
void system_status_set_headline_locked(const char *message);
void system_status_set_message_locked(const char *headline, const char *message);
void system_status_set_messagef_locked(const char *headline, const char *fmt, ...);
void system_status_set_explanation_locked(const char *message);
void system_status_append_explanationf_locked(const char *fmt, ...);
