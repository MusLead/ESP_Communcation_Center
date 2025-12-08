#include "mqtt_pub_sub.h"
#include "esp_log.h"
#include "mqtt_client.h"

// MQTT BROKER --> HIVEMQ
// #define MQTT_ADDR_URL "mqtts://9729ca70c6144f3ca3a29dee8cf330ce.s1.eu.hivemq.cloud:8883"
#define MQTT_ADDR_URL "mqtts://192.168.178.109:8883"

// OLD CA crt
// extern const uint8_t certs_isrg_root_x1_pem_start[] asm("_binary_isrg_root_x1_pem_start");
// extern const uint8_t certs_isrg_root_x1_pem_end[] asm("_binary_isrg_root_x1_pem_end");
// Added the required CAs for the mqtt_broker
// Required CAs:
// client.crt
// client.key
// ca.crt
extern const uint8_t _binary_client_crt_start[] asm("_binary_client_crt_start");
extern const uint8_t _binary_client_crt_end[] asm("_binary_client_crt_end");

extern const uint8_t _binary_client_key_start[] asm("_binary_client_key_start");
extern const uint8_t _binary_client_key_end[] asm("_binary_client_key_end");

extern const uint8_t _binary_ca_crt_start[] asm("_binary_ca_crt_start");
extern const uint8_t _binary_ca_crt_end[] asm("_binary_ca_crt_end");

static const char *TAG = "MQTT_PUBSUB";

static esp_mqtt_client_handle_t global_client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    global_client = event->client; // Make client accessible in our functions

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to MQTT Broker");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected from MQTT Broker");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("\nTOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

// PUBLIC FUNCTIONS FOR PUBLISH / SUBSCRIBE
void mqtt_publish(const char *topic, const char *msg, int qos)
{
    if (global_client == NULL)
        return;
    esp_mqtt_client_publish(global_client, topic, msg, 0, qos, 0);
}

void mqtt_subscribe(const char *topic, int qos)
{
    if (global_client == NULL)
        return;
    int msg_id = esp_mqtt_client_subscribe(global_client, topic, qos);
    ESP_LOGI(TAG, "Subscribe OK, msg_id=%d", msg_id);
}

// START THE MQTT_PUBSUB
void mqtt_pubsub_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {

        .broker.address.uri = MQTT_ADDR_URL,
        // .broker.verification.certificate = (const char *)certs_isrg_root_x1_pem_start,
        // .credentials.username = "ESP32",
        // .credentials.authentication.password = "Test1234"
        .broker.verification.certificate = (const char *)_binary_ca_crt_start,
        .broker.verification.certificate_len = _binary_ca_crt_end - _binary_ca_crt_start,
        .credentials.authentication.certificate = (const char *)_binary_client_crt_start,
        .credentials.authentication.certificate_len = _binary_client_crt_end - _binary_client_crt_start,
        .credentials.authentication.key = (const char *)_binary_client_key_start,
        .credentials.authentication.key_len = _binary_client_key_end - _binary_client_key_start,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}