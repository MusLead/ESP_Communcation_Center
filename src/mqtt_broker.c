#include "mqtt_broker.h"
#include "mosq_broker.h"
#include "esp_log.h"

static const char *TAG = "MQTT_BROKER";

// Added the required CAs for the mqtt_broker
// Required CAs:
// server.crt
// server.key
// ca.crt
extern const uint8_t _binary_server_crt_start[] asm("_binary_server_crt_start");
extern const uint8_t _binary_server_crt_end[] asm("_binary_server_crt_end");

extern const uint8_t _binary_server_key_start[] asm("_binary_server_key_start");
extern const uint8_t _binary_server_key_end[] asm("_binary_server_key_end");

extern const uint8_t _binary_ca_crt_start[] asm("_binary_ca_crt_start");
extern const uint8_t _binary_ca_crt_end[] asm("_binary_ca_crt_end");

// Define the variable here
char latest_temp[32] = "0";
char latest_light[2] = "0";

void mqtt_message_cb(char *client, char *topic, char *data, int len, int qos, int retain)
{
    if (strcmp(topic, "ESP32/values") == 0)
    {
        // Copy the message into our global variable
        int copy_len = len < sizeof(latest_temp) - 1 ? len : sizeof(latest_temp) - 1;
        memcpy(latest_temp, data, copy_len);
        latest_temp[copy_len] = '\0';
    }
    else if (strcmp(topic, "v2/light/values") == 0)
    {
        int copy_len = len < sizeof(latest_light) - 1 ? len : sizeof(latest_light) - 1;
        memcpy(latest_light, data, copy_len);
        latest_light[copy_len] = '\0';
    }
}

void mqtt_broker_start(void *pvParameters)
{
    esp_tls_cfg_server_t tls_cfg = {
        .cacert_buf = _binary_ca_crt_start,
        .cacert_bytes = _binary_ca_crt_end - _binary_ca_crt_start,
        .servercert_buf = _binary_server_crt_start,
        .servercert_bytes = _binary_server_crt_end - _binary_server_crt_start,
        .serverkey_buf = _binary_server_key_start,
        .serverkey_bytes = _binary_server_key_end - _binary_server_key_start,
    };

    struct mosq_broker_config config = {
        .host = "0.0.0.0",
        .port = 8883,
        .tls_cfg = &tls_cfg,
        .handle_message_cb = mqtt_message_cb // <-- set callback
    };

    ESP_LOGI(TAG, "Starting MQTT Broker on port %d", config.port);

    mosq_broker_run(&config); // blocking
    mosq_broker_stop();
}
