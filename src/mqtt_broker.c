#include <string.h>
#include "mqtt_broker.h"
#include "mosq_broker.h"
#include "system_state.h"

// static const char *TAG = "MQTT_BROKER";

// MQTT message callback function
void mqtt_message_cb(char *client, char *topic, char *data, int len, int qos, int retain)
{
    if (xSemaphoreTake(state_mutex, portMAX_DELAY) != pdTRUE)
        return;

    char payload[32]; // save messages here
    uint8_t copy_len = (uint8_t)len < sizeof(payload) - 1 ? len : sizeof(payload) - 1;
    memcpy(payload, data, copy_len);
    payload[copy_len] = '\0';

    // Indoor Sensor
    if (strcmp(topic, "ESP32/indoor") == 0)
    {
        char *p;
        if ((p = strstr(payload, "Temp:")) != NULL)
        {
            indoor_temp = atof(p + 5);
        }
        if ((p = strstr(payload, "H=")) != NULL)
        {

            indoor_humidity = atof(p + 2);
        }
        if ((p = strstr(payload, "AQ:")) != NULL)
        {
            indoor_aq = atoi(p + 3);
        }
    }
    // Outdoor Sensor
    else if (strcmp(topic, "ESP32/outdoor") == 0)
    {
        char *p;
        if ((p = strstr(payload, "Temp:")) != NULL)
        {
            outdoor_temp = atof(p + 5);
        }
        if ((p = strstr(payload, "H=")) != NULL)
        {
            outdoor_humidity = atof(p + 2);
        }
        if ((p = strstr(payload, "AQ:")) != NULL)
        {
            outdoor_aq = atoi(p + 3);
        }
    }
    // Wind Speed
    else if (strcmp(topic, "ESP32/wind") == 0)
    {
        wind_speed = atof(payload);
    }

    xSemaphoreGive(state_mutex);
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