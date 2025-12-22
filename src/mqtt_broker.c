#include <string.h>
#include "mqtt_broker.h"
#include "mosq_broker.h"
#include "system_state.h"

// static const char *TAG = "MQTT_BROKER";

// MQTT message callback function
void mqtt_message_cb(char *client, char *topic, char *data, int len, int qos, int retain)
{
    char payload[32]; // save messages here
    uint8_t copy_len = (uint8_t)len < sizeof(payload) - 1 ? len : sizeof(payload) - 1;
    memcpy(payload, data, copy_len);
    payload[copy_len] = '\0';

    // Values to be passed to global state
    float in_t = 0, in_h = 0;
    float out_t = 0, out_h = 0;
    uint8_t in_aq = 0;
    uint8_t out_aq = 0;

    // Indoor Sensor
    if (strcmp(topic, "ESP32/indoor") == 0)
    {
        char *p;
        if ((p = strstr(payload, "Temp:")) != NULL)
        {
            in_t = atof(p + 5);
        }
        if ((p = strstr(payload, "H=")) != NULL)
        {
            in_h = atof(p + 2);
        }
        if ((p = strstr(payload, "AQ:")) != NULL)
        {
            in_aq = atoi(p + 3);
        }
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        indoor_temp = in_t;
        indoor_humidity = in_h;
        indoor_aq = in_aq;
        xSemaphoreGive(state_mutex);
    }
    // Outdoor Sensor
    else if (strcmp(topic, "ESP32/outdoor") == 0)
    {
        char *p;
        if ((p = strstr(payload, "Temp:")) != NULL)
        {
            out_t = atof(p + 5);
        }
        if ((p = strstr(payload, "H=")) != NULL)
        {
            out_h = atof(p + 2);
        }
        if ((p = strstr(payload, "AQ:")) != NULL)
        {
            out_aq = atoi(p + 3);
        }
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        outdoor_temp = out_t;
        outdoor_humidity = out_h;
        outdoor_aq = out_aq;
        xSemaphoreGive(state_mutex);
    }
    // Wind Speed
    else if (strcmp(topic, "ESP32/wind") == 0)
    {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        wind_speed = atof(payload);
        xSemaphoreGive(state_mutex);
    }
}

// MQTT authentication callback function
int mqtt_auth_cb(const char *client_id, const char *username, const char *password, int password_len)
{
    // ESP_LOGI("MQTT_AUTH", "MQTT connect attempt: client_id=%s user=%s", client_id, username);

    // simple auth: user=esp32 , pass=1234
    if (username && strcmp(username, "esp32") == 0 &&
        password && strncmp(password, "1234", password_len) == 0)
    {
        return 0; // accept
    }
    return 1; // reject
}

void mqtt_broker_start(void *pvParameters)
{

    struct mosq_broker_config config = {
        .host = "0.0.0.0",
        .port = 1883,
        .handle_connect_cb = mqtt_auth_cb,
        .handle_message_cb = mqtt_message_cb // <-- set callback
    };

    // ESP_LOGI(TAG, "Starting MQTT Broker on port %d", config.port);

    mosq_broker_run(&config); // blocking
    mosq_broker_stop();
}