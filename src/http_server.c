
#include "http_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include "mqtt_broker.h"

static const char *TAG = "HTTP_SERVER";

// HTTP GET handler
static esp_err_t index_get_handler(httpd_req_t *req)
{
    char json_resp[64];
    snprintf(json_resp, sizeof(json_resp), "{ %s }", latest_temp);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_resp, strlen(json_resp));

    ESP_LOGI("HTTP", "Sent JSON: %s", json_resp);

    return ESP_OK;
}

// HTTP_POST ... but requires the mqtt_pub_sub.h file for publishing the message
/* static esp_err_t light_post_handler(httpd_req_t *req)
{
    char value;

    // Read exactly ONE byte
    int received = httpd_req_recv(req, &value, sizeof(value));

    if (received != 1 || (value != '0' && value != '1' && value != '2'))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Send 0, 1 or 2");
        return ESP_FAIL;
    }

    // Publish exactly what we received
    mqtt_publish("v2/light/values", &value, 1);

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
} */

// Start HTTP server
void http_server_start(void *pvParameters)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    esp_err_t ret = httpd_start(&server, &config);

    // Start MQTT --> to publsh any client_post messages
    // mqtt_pubsub_start();

    if (ret == ESP_OK)
    {
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_get_handler};

        //
        // Add other uri path .. for example /light
        /* httpd_uri_t light_uri = {
            .uri = "/light",
            .method = HTTP_POST,
            .handler = light_post_handler}; */
        httpd_register_uri_handler(server, &index_uri);
        //
        // Add more uri registers
        /* httpd_register_uri_handler(server, &light_uri); */
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000)); // ✔ Task muss laufen
        }
    }

    else
    {
        ESP_LOGE(TAG, "Error starting server: %s", esp_err_to_name(ret));

        // Task ordentlich löschen
        vTaskDelete(NULL);
    }
}