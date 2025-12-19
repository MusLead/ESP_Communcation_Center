#include "http_server.h"
#include "esp_http_server.h"
#include <string.h>
#include <stdbool.h>
#include "mqtt_broker.h"
#include "system_state.h"
#include "mqtt_pub_sub.h"

static const char *TAG = "HTTP_SERVER";

// ---------- GET HANDLERS ----------

static esp_err_t sensors_get_handler(httpd_req_t *req)
{
    char json_resp[256];

    xSemaphoreTake(state_mutex, portMAX_DELAY);

    snprintf(json_resp, sizeof(json_resp),
             "{"
             "\"indoor\": {\"Temp\": %.2f, \"H\": %.2f, \"AQ\": %u},"
             "\"outdoor\": {\"Temp\": %.2f, \"H\": %.2f, \"AQ\": %u},"
             "\"wind_speed\": %.1f"
             "}",
             indoor_temp, indoor_humidity, indoor_aq,
             outdoor_temp, outdoor_humidity, outdoor_aq,
             wind_speed);

    xSemaphoreGive(state_mutex);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_resp, strlen(json_resp));

    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char json_resp[256];

    xSemaphoreTake(state_mutex, portMAX_DELAY);

    snprintf(json_resp, sizeof(json_resp),
             "{"
             "\"mode\":%d,"
             "\"window\":%d,"
             "\"fan\":%d,"
             "\"door\":%d,"
             "\"absorber\":%d"
             "}",
             current_mode, window, fan, door, absorber_used);

    xSemaphoreGive(state_mutex);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_resp);

    return ESP_OK;
}

static esp_err_t schedule_get_handler(httpd_req_t *req)
{
    char resp[512];
    int offset = 0;

    xSemaphoreTake(state_mutex, portMAX_DELAY);

    offset += snprintf(resp + offset, sizeof(resp) - offset, "{ \"periods\":[");
    for (int i = 0; i < schedule_count; i++)
    {
        offset += snprintf(resp + offset, sizeof(resp) - offset,
                           "%s{"
                           "\"start\":\"%s\","
                           "\"end\":\"%s\","
                           "\"mode\":\"%d\""
                           "}",
                           i > 0 ? "," : "",
                           schedule[i].start,
                           schedule[i].end,
                           schedule[i].mode);
    }
    offset += snprintf(resp + offset, sizeof(resp) - offset, "]}");

    xSemaphoreGive(state_mutex);

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

    xSemaphoreTake(state_mutex, portMAX_DELAY);

    if (current_mode != MODE_MANUAL)
    {
        xSemaphoreGive(state_mutex);
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "only MANUAL allowed"), ESP_FAIL;
    }

    int new_window, new_fan, new_absorber, new_door;

    char *p = strstr(buf, "\"window\"");

    if (p == NULL || sscanf(p,
                            "\"window\" : %d , \"fan\" : %d , \"door\" : %d , \"absorber\" : %d",
                            &new_window, &new_fan, &new_door, &new_absorber) != 4)
    {
        xSemaphoreGive(state_mutex);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body"), ESP_FAIL;
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

    mqtt_publish("ESP32/window", window ? "1" : "0", 1);
    mqtt_publish("ESP32/fan", fan ? "1" : "0", 1);
    mqtt_publish("ESP32/absorber", absorber_used ? "1" : "0", 1);
    mqtt_publish("ESP32/door", door ? "1" : "0", 1);

    xSemaphoreGive(state_mutex);
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

    xSemaphoreTake(state_mutex, portMAX_DELAY);

    schedule_count = 0;
    char *p = buf;

    while ((p = strstr(p, "\"start\"")) && schedule_count < MAX_PERIODS)
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
            strncpy(schedule[schedule_count].start, start, 6);
            strncpy(schedule[schedule_count].end, end, 6);
            schedule[schedule_count].mode = (system_mode_t)mode;
            schedule_count++;

            // ESP_LOGI("SCHEDULE", "Parsed Schedule %d: %s - %s Mode: %d",
            //         schedule_count - 1, start, end, mode);
        }

        // ESP_LOGI("SCHEDULE", "Current schedule count: %d", schedule_count);

        p = strchr(p + 1, '}'); // gehe zum Ende des Objekts
        if (p)
            p++; // weiter danach suchen
    }

    xSemaphoreGive(state_mutex);

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

//
//
// Start HTTP server
void http_server_start(void *pvParameters)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    esp_err_t ret = httpd_start(&server, &config);

    if (ret == ESP_OK)
    {
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
        ESP_LOGI(TAG, "HTTP API ready");
        //
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000)); //
        }
    }

    else
    {
        // ESP_LOGE(TAG, "Error starting server: %s", esp_err_to_name(ret));

        // Delete task if server failed to start
        vTaskDelete(NULL);
    }
}