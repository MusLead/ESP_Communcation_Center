#include "http_server.h"
#include "esp_http_server.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "mqtt_broker.h"
#include "system_state.h"

static const char *TAG = "HTTP_SERVER";

static void format_json_float(char *buf, size_t buf_size, bool available, float value, int decimals)
{
    if (!available)
    {
        snprintf(buf, buf_size, "\"--\"");
        return;
    }

    snprintf(buf, buf_size, "%.*f", decimals, value);
}

static void format_json_uint(char *buf, size_t buf_size, bool available, uint32_t value)
{
    if (!available)
    {
        snprintf(buf, buf_size, "\"--\"");
        return;
    }

    snprintf(buf, buf_size, "%" PRIu32, value);
}

static void escape_json_string(char *dest, size_t dest_size, const char *src)
{
    size_t j = 0;

    if (dest_size == 0)
    {
        return;
    }

    if (src == NULL)
    {
        dest[0] = '\0';
        return;
    }

    for (size_t i = 0; src[i] != '\0' && j + 1 < dest_size; ++i)
    {
        if ((src[i] == '\\' || src[i] == '"') && j + 2 < dest_size)
        {
            dest[j++] = '\\';
        }

        if ((unsigned char)src[i] < 0x20)
        {
            continue;
        }

        dest[j++] = src[i];
    }

    dest[j] = '\0';
}

// ---------- GET HANDLERS ----------

static esp_err_t sensors_get_handler(httpd_req_t *req)
{
    float in_t = 0, in_h = 0;
    float out_t = 0, out_h = 0, wi_speed = 0;
    uint8_t in_aq = 0;
    uint8_t out_aq = 0;
    bool indoor_available = false;
    bool outdoor_available = false;
    bool wind_available = false;
    bool indoor_connected = false;
    bool outdoor_connected = false;
    char indoor_temp_json[16];
    char indoor_humidity_json[16];
    char indoor_aq_json[16];
    char outdoor_temp_json[16];
    char outdoor_humidity_json[16];
    char outdoor_aq_json[16];
    char wind_speed_json[16];

    system_state_refresh_sensor_timeouts();

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    in_t = indoor_temp;
    in_h = indoor_humidity;
    in_aq = indoor_aq;
    out_t = outdoor_temp;
    out_h = outdoor_humidity;
    out_aq = outdoor_aq;
    wi_speed = wind_speed;
    indoor_available = indoor_data_available;
    outdoor_available = outdoor_data_available;
    wind_available = wind_data_available;
    xSemaphoreGive(state_mutex);

    indoor_connected = indoor_available;
    outdoor_connected = outdoor_available || wind_available;

    format_json_float(indoor_temp_json, sizeof(indoor_temp_json), indoor_available, in_t, 2);
    format_json_float(indoor_humidity_json, sizeof(indoor_humidity_json), indoor_available, in_h, 2);
    format_json_uint(indoor_aq_json, sizeof(indoor_aq_json), indoor_available, in_aq);
    format_json_float(outdoor_temp_json, sizeof(outdoor_temp_json), outdoor_available, out_t, 2);
    format_json_float(outdoor_humidity_json, sizeof(outdoor_humidity_json), outdoor_available, out_h, 2);
    format_json_uint(outdoor_aq_json, sizeof(outdoor_aq_json), outdoor_available, out_aq);
    format_json_float(wind_speed_json, sizeof(wind_speed_json), wind_available, wi_speed, 1);

    char json_resp[256];

    snprintf(json_resp, sizeof(json_resp),
             "{"
             "\"connections\": {\"indoor\": %s, \"outdoor\": %s},"
             "\"indoor\": {\"Temp\": %s, \"H\": %s, \"AQ\": %s},"
             "\"outdoor\": {\"Temp\": %s, \"H\": %s, \"AQ\": %s},"
             "\"wind_speed\": %s"
             "}",
             indoor_connected ? "true" : "false",
             outdoor_connected ? "true" : "false",
             indoor_temp_json, indoor_humidity_json, indoor_aq_json,
             outdoor_temp_json, outdoor_humidity_json, outdoor_aq_json,
             wind_speed_json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_resp, strlen(json_resp));

    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    int mode;
    bool w, f, d, a;
    bool schedule_configured;
    bool schedule_active;
    bool schedule_inactive_manual_override;
    bool manual_control_allowed;
    char why[STATUS_EXPLANATION_MAX_LEN];
    char why_json[256];

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    mode = current_mode;
    w = window;
    f = fan;
    d = door;
    a = absorber_used;
    system_state_get_control_flags_locked(&schedule_configured,
                                          &schedule_active,
                                          &schedule_inactive_manual_override,
                                          &manual_control_allowed);
    xSemaphoreGive(state_mutex);

    system_state_get_status_explanation(why, sizeof(why));
    escape_json_string(why_json, sizeof(why_json), why);

    // The explanation is stored in shared system state.
    // Update it from the control logic with:
    // system_state_set_status_explanation("...");
    // or system_state_set_status_explanationf("...", ...);

    char json_resp[512];
    snprintf(json_resp, sizeof(json_resp),
             "{"
             "\"mode\":%d,"
             "\"window\":%d,"
             "\"fan\":%d,"
             "\"door\":%d,"
             "\"absorber\":%d,"
             "\"scheduleConfigured\":%s,"
             "\"scheduleActive\":%s,"
             "\"scheduleInactiveManualOverride\":%s,"
             "\"manualControlAllowed\":%s,"
             "\"why\":\"%s\""
             "}",
             mode,
             w,
             f,
             d,
             a,
             schedule_configured ? "true" : "false",
             schedule_active ? "true" : "false",
             schedule_inactive_manual_override ? "true" : "false",
             manual_control_allowed ? "true" : "false",
             why_json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_resp, strlen(json_resp));
    return ESP_OK;
}

static esp_err_t schedule_get_handler(httpd_req_t *req)
{

    schedule_period_t local[MAX_PERIODS];
    uint8_t local_count;

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    local_count = schedule_count;
    memcpy(local, schedule, sizeof(schedule_period_t) * local_count);
    xSemaphoreGive(state_mutex);

    char resp[512];
    int offset = 0;

    offset += snprintf(resp + offset, sizeof(resp) - offset, "{ \"periods\":[");
    for (int i = 0; i < local_count; i++)
    {
        offset += snprintf(resp + offset, sizeof(resp) - offset,
                           "%s{"
                           "\"start\":\"%s\","
                           "\"end\":\"%s\","
                           "\"mode\":\"%d\""
                           "}",
                           i > 0 ? "," : "",
                           local[i].start,
                           local[i].end,
                           local[i].mode);
    }

    offset += snprintf(resp + offset, sizeof(resp) - offset, "]}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

// ---------- POST HANDLERS ----------

static esp_err_t mode_post_handler(httpd_req_t *req)
{
    char buf[32];

    if (req->content_len <= 0 || req->content_len >= sizeof(buf))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "len"), ESP_FAIL;

    if (httpd_req_recv(req, buf, req->content_len) != req->content_len)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "recv"), ESP_FAIL;

    buf[req->content_len] = 0;

    int new_mode = 0;

    // ESP_LOGI("HTTP_SERVER", "Received mode change request: %s", buf);

    char *p = strstr(buf, "\"mode\"");

    if (p == NULL || sscanf(p, "\"mode\" : %d", &new_mode) != 1)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json"), ESP_FAIL;
    }

    // ESP_LOGI("HTTP_SERVER", "Requested new mode: %d", new_mode);

    if (new_mode < 0 || new_mode > 2)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid mode"), ESP_FAIL;

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    current_mode = (system_mode_t)new_mode;

    // IMPORTANT FIX:
    // Reset schedule count when switching mode

    schedule_count = 0;
    // ESP_LOGI("HTTP_SERVER_MODE", "Current schedule count: %d", schedule_count);

    xSemaphoreGive(state_mutex);

    if (new_mode == MODE_MANUAL)
    {
        system_state_set_status_explanation("Manual Mode, no Status explanation!");
    }
    else
    {
        system_state_set_status_explanation("Automatic mode active. Waiting for rule evaluation.");
    }

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

static esp_err_t actuators_post_handler(httpd_req_t *req)
{
    char buf[128];

    if (req->content_len <= 0 || req->content_len >= sizeof(buf))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "len"), ESP_FAIL;

    if (httpd_req_recv(req, buf, req->content_len) != req->content_len)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "recv"), ESP_FAIL;

    buf[req->content_len] = 0;

    int new_window, new_fan, new_absorber, new_door;

    char *p = strstr(buf, "\"window\"");

    if (p == NULL || sscanf(p,
                            "\"window\" : %d , \"fan\" : %d , \"door\" : %d , \"absorber\" : %d",
                            &new_window, &new_fan, &new_door, &new_absorber) != 4)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body"), ESP_FAIL;
    }

    bool w, f, d, a;
    bool schedule_inactive_manual_override = false;
    bool manual_control_allowed = false;
    bool is_manual_mode = false;

    xSemaphoreTake(state_mutex, portMAX_DELAY);

    system_state_get_control_flags_locked(NULL,
                                          NULL,
                                          &schedule_inactive_manual_override,
                                          &manual_control_allowed);

    if (!manual_control_allowed)
    {
        xSemaphoreGive(state_mutex);
        return httpd_resp_send_err(req,
                                   HTTPD_403_FORBIDDEN,
                                   "manual control is only allowed in MANUAL mode or while the schedule is inactive"),
               ESP_FAIL;
    }

    // Business rule: fan needs window open
    if (new_fan && !new_window)
    {
        xSemaphoreGive(state_mutex);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "fan requires window open"),
               ESP_FAIL;
    }

    // Apply state
    window = new_window;
    fan = new_fan;
    absorber_used = new_absorber;
    door = new_door;

    w = window;
    f = fan;
    d = door;
    a = absorber_used;
    is_manual_mode = current_mode == MODE_MANUAL;

    xSemaphoreGive(state_mutex);

    if (is_manual_mode)
    {
        system_state_set_status_explanation("Manual Mode, no Status explanation!");
    }
    else if (schedule_inactive_manual_override)
    {
        system_state_set_status_explanation(
            "Schedule inactive: manual override is active until the next schedule period starts.");
    }

    ESP_LOGI("HTTP_SERVER", "Publishing actuator states via MQTT");

    publish_state(w, f, d, a);

    ESP_LOGI("HTTP_SERVER", "DONE Publishing");

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

static esp_err_t schedule_post_handler(httpd_req_t *req)
{
    char buf[512];

    if (req->content_len <= 0 || req->content_len >= sizeof(buf))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "len"), ESP_FAIL;

    if (httpd_req_recv(req, buf, req->content_len) != req->content_len)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "recv"), ESP_FAIL;

    buf[req->content_len] = 0;

    schedule_period_t local[MAX_PERIODS];
    uint8_t local_count = 0;
    char *p = buf;

    while ((p = strstr(p, "\"start\"")) && local_count < MAX_PERIODS)
    {
        char start[6], end[6];
        int mode;

        int ret = sscanf(p,
                         "\"start\" : \"%5[^\"]\" , \"end\" : \"%5[^\"]\" , \"mode\" : %d",
                         start, end, &mode);

        // ESP_LOGI("SCHEDULE", "Parsing Schedule: ret=%d start=%s end=%s mode=%d",
        //          ret, start, end, mode);

        if (ret == 3)
        {
            strncpy(local[local_count].start, start, 6);
            strncpy(local[local_count].end, end, 6);
            local[local_count].mode = (system_mode_t)mode;
            local_count++;

            // ESP_LOGI("SCHEDULE", "Parsed Schedule %d: %s - %s Mode: %d",
            //         schedule_count - 1, start, end, mode);
        }

        // ESP_LOGI("SCHEDULE", "Current schedule count: %d", schedule_count);

        p = strchr(p + 1, '}'); // go at the end of the Objekt
        if (p)
        {
            p++;
        }
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    memcpy(schedule, local, sizeof(schedule_period_t) * local_count);
    schedule_count = local_count;
    xSemaphoreGive(state_mutex);

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// Start HTTP server
void http_server_start(void *pvParameters)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.keep_alive_enable = true;
    config.keep_alive_idle = 5;
    config.keep_alive_interval = 5;
    config.keep_alive_count = 3;

    if (httpd_start(&server, &config) != ESP_OK)
    {
        vTaskDelete(NULL);
        return;
    }

    // Add other any URI path here
    //
    // GET SENSOR --> /api/v1/sensors
    httpd_uri_t sensors_uri = {
        .uri = "/api/v1/sensors",
        .method = HTTP_GET,
        .handler = sensors_get_handler};

    // GET STATUS --> /api/v1/status
    httpd_uri_t status_uri = {
        .uri = "/api/v1/status",
        .method = HTTP_GET,
        .handler = status_get_handler};

    // POST MODE --> /api/v1/mode
    httpd_uri_t mode_uri = {
        .uri = "/api/v1/mode",
        .method = HTTP_POST,
        .handler = mode_post_handler};

    // POST ACTUATORS --> /api/v1/actuators
    httpd_uri_t actuators_uri = {
        .uri = "/api/v1/actuators",
        .method = HTTP_POST,
        .handler = actuators_post_handler};

    // GET SCHEDULE --> /api/v1/schedule
    httpd_uri_t schedule_get_uri = {
        .uri = "/api/v1/schedule",
        .method = HTTP_GET,
        .handler = schedule_get_handler};

    // POST SCHEDULE --> /api/v1/schedule
    httpd_uri_t schedule_post_uri = {
        .uri = "/api/v1/schedule",
        .method = HTTP_POST,
        .handler = schedule_post_handler};

    //
    //
    // Register URI handlers
    httpd_register_uri_handler(server, &sensors_uri);
    httpd_register_uri_handler(server, &status_uri);
    httpd_register_uri_handler(server, &mode_uri);
    httpd_register_uri_handler(server, &actuators_uri);
    httpd_register_uri_handler(server, &schedule_get_uri);
    httpd_register_uri_handler(server, &schedule_post_uri);

    //
    // Server running
    ESP_LOGI(TAG, "HTTP SERER RUNNING");
    //
    vTaskDelete(NULL);
}
