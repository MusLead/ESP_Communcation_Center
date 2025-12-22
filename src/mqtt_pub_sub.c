#include "mqtt_pub_sub.h"
#include "mqtt_client.h"
#include "esp_log.h"

static esp_mqtt_client_handle_t global_client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    global_client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI("MQTT", "Connected");
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI("MQTT", "Disconnected");
        break;

    default:
        break;
    }
}

// PUBLIC FUNCTIONS FOR PUBLISH / SUBSCRIBE
void mqtt_publish(const char *topic, const char *msg, int qos)
{
    if (global_client == NULL)
    {
        ESP_LOGE("MQTT_PUB_SUB", "Client not initialized");
        return;
    }

    ESP_LOGI("MQTT_PUB_SUB", "Publishing to topic %s msg %s", topic, msg);

    esp_mqtt_client_publish(global_client, topic, msg, strlen(msg), qos, 1);
}

// START THE MQTT_PUBSUB
void mqtt_pubsub_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_ADDR_URL,
        .credentials.username = "esp32",
        .credentials.authentication.password = "1234",

    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}