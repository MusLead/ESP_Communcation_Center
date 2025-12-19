#include "mqtt_pub_sub.h"
#include "mqtt_client.h"

// static const char *TAG = "MQTT_PUBSUB";

static esp_mqtt_client_handle_t global_client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    global_client = event->client; // Make client accessible in our functions

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        // ESP_LOGI(TAG, "Connected to MQTT Broker");
        break;
    case MQTT_EVENT_DISCONNECTED:
        // ESP_LOGI(TAG, "Disconnected from MQTT Broker");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        // ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        // ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        // ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        // ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        // ESP_LOGI(TAG, "Topic=%.*s", event->topic_len, event->topic);
        // ESP_LOGI(TAG, "Data=%.*s", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        // ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        // ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

// PUBLIC FUNCTIONS FOR PUBLISH / SUBSCRIBE
void mqtt_publish(const char *topic, const char *msg, int qos)
{
    if (global_client == NULL)
        return;
    esp_mqtt_client_publish(global_client, topic, msg, 0, qos, 1);
}

// START THE MQTT_PUBSUB
void mqtt_pubsub_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_ADDR_URL,
        .credentials.username = "esp32",
        .credentials.authentication.password = "1234"};

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}