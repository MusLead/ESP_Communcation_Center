#include <stdio.h>
#include "esp_log.h"
#include "https_server.h"
#include "esp_err.h"
#include <esp_http_server.h>
#include <esp_https_server.h>
#include "mqtt_broker.h"
#include "mqtt_pub_sub.h"

static const char *TAG = "HTTPS_SERVER";

/* TLS certs from flash */
extern const uint8_t server_cert_pem_start[] asm("_binary_server_https_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_server_https_crt_end");
extern const uint8_t server_key_pem_start[] asm("_binary_server_https_key_start");
extern const uint8_t server_key_pem_end[] asm("_binary_server_https_key_end");

/* HTTPS GET Handler */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    char json_resp[64];
    snprintf(json_resp, sizeof(json_resp), "{ %s }", latest_temp);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_resp, HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Sent JSON: %s", json_resp);

    return ESP_OK;
}

/* HTTPS POST Handler ... But requires the mqtt_pub_sub.h file for publishing the message */
static esp_err_t light_post_handler(httpd_req_t *req)
{
    char value = 0;

    if (req->content_len != 1)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload must be 1 byte");
        return ESP_FAIL;
    }

    int received = httpd_req_recv(req, &value, 1);

    if (received != 1 || (value != '0' && value != '1' && value != '2'))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Send '0', '1' or '2'");
        return ESP_FAIL;
    }

    mqtt_publish("v2/light/values", &value, 1);

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

/* HTTPS TASK */
void https_server_start(void *pvParameters)
{
    httpd_handle_t server = NULL;

    /* HTTPS config (based on HTTP default) */
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();

    config.servercert = server_cert_pem_start;
    config.servercert_len = server_cert_pem_end - server_cert_pem_start;

    config.prvtkey_pem = server_key_pem_start;
    config.prvtkey_len = server_key_pem_end - server_key_pem_start;

    esp_err_t ret = httpd_ssl_start(&server, &config);

    // Start MQTT --> to publsh any client_post messages
    mqtt_pubsub_start();

    if (ret == ESP_OK)
    {
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_get_handler};
        httpd_uri_t light_uri = {
            .uri = "/lights",
            .method = HTTP_POST,
            .handler = light_post_handler};

        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &light_uri);

        ESP_LOGI(TAG, "HTTPS server started");

        /* Keep task alive */
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTPS server start failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
    }
}
