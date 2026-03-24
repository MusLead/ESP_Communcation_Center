#include "system_status.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static SemaphoreHandle_t status_mutex = NULL;
static char status_headline[STATUS_EXPLANATION_MAX_LEN] = "You are in control";
static char status_explanation[STATUS_EXPLANATION_MAX_LEN] = "Manual Mode, no Status explanation!";

static void system_status_copy_string(char *dest, size_t dest_size, const char *message, const char *fallback)
{
    const char *safe_message = message;

    if (safe_message == NULL || safe_message[0] == '\0')
    {
        safe_message = fallback;
    }

    snprintf(dest, dest_size, "%s", safe_message);
}

void system_status_init(SemaphoreHandle_t mutex)
{
    status_mutex = mutex;
}

void system_status_set_headline_locked(const char *message)
{
    system_status_copy_string(status_headline, sizeof(status_headline), message, "System update");
}

void system_status_set_explanation_locked(const char *message)
{
    system_status_copy_string(status_explanation,
                              sizeof(status_explanation),
                              message,
                              "No status explanation available.");
}

void system_status_set_message_locked(const char *headline, const char *message)
{
    system_status_set_headline_locked(headline);
    system_status_set_explanation_locked(message);
}

void system_status_set_messagef_locked(const char *headline, const char *fmt, ...)
{
    va_list args;

    system_status_set_headline_locked(headline);

    if (fmt == NULL || fmt[0] == '\0')
    {
        system_status_set_explanation_locked(NULL);
        return;
    }

    va_start(args, fmt);
    vsnprintf(status_explanation, sizeof(status_explanation), fmt, args);
    va_end(args);
}

void system_status_append_explanationf_locked(const char *fmt, ...)
{
    size_t len;
    va_list args;

    if (fmt == NULL || fmt[0] == '\0')
    {
        return;
    }

    len = strlen(status_explanation);

    if (len + 1 >= sizeof(status_explanation))
    {
        return;
    }

    status_explanation[len++] = ' ';
    status_explanation[len] = '\0';

    va_start(args, fmt);
    vsnprintf(status_explanation + len, sizeof(status_explanation) - len, fmt, args);
    va_end(args);
}

void system_status_set_headline(const char *message)
{
    if (status_mutex == NULL)
    {
        system_status_set_headline_locked(message);
        return;
    }

    xSemaphoreTake(status_mutex, portMAX_DELAY);
    system_status_set_headline_locked(message);
    xSemaphoreGive(status_mutex);
}

void system_status_set_message(const char *headline, const char *message)
{
    if (status_mutex == NULL)
    {
        system_status_set_message_locked(headline, message);
        return;
    }

    xSemaphoreTake(status_mutex, portMAX_DELAY);
    system_status_set_message_locked(headline, message);
    xSemaphoreGive(status_mutex);
}

void system_status_set_explanation(const char *message)
{
    if (status_mutex == NULL)
    {
        system_status_set_explanation_locked(message);
        return;
    }

    xSemaphoreTake(status_mutex, portMAX_DELAY);
    system_status_set_explanation_locked(message);
    xSemaphoreGive(status_mutex);
}

void system_status_set_explanationf(const char *fmt, ...)
{
    char message[STATUS_EXPLANATION_MAX_LEN];
    va_list args;

    if (fmt == NULL || fmt[0] == '\0')
    {
        system_status_set_explanation(NULL);
        return;
    }

    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    system_status_set_explanation(message);
}

void system_status_get_headline(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0)
    {
        return;
    }

    if (status_mutex == NULL)
    {
        snprintf(buf, buf_size, "%s", status_headline);
        return;
    }

    xSemaphoreTake(status_mutex, portMAX_DELAY);
    snprintf(buf, buf_size, "%s", status_headline);
    xSemaphoreGive(status_mutex);
}

void system_status_get_explanation(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0)
    {
        return;
    }

    if (status_mutex == NULL)
    {
        snprintf(buf, buf_size, "%s", status_explanation);
        return;
    }

    xSemaphoreTake(status_mutex, portMAX_DELAY);
    snprintf(buf, buf_size, "%s", status_explanation);
    xSemaphoreGive(status_mutex);
}
