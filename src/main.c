//
// ----
// This implemntation starts the wifi and init the local time and the system state and after that pins two task in two cores:
// Core 0 for the mqtt_broker
// Core 1 for the http_server
// + starts the system state task
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "sntp_time.h"
#include "wifi.h"
#include "http_server.h"
#include "mqtt_broker.h"
#include "system_state.h"
#include "mqtt_pub_sub.h"

void app_main(void)
{
    // 1 System Info
    ESP_LOGI("MAIN", "System start");
    ESP_LOGI("MAIN", "Free heap: %lu", esp_get_free_heap_size());
    ESP_LOGI("MAIN", "IDF version: %s", esp_get_idf_version());
    //

    // 2 Always initialize the default NVS partition first
    nvs_flash_init();

    // 3 start WIFI connection
    connect_wifi();

    // 4 Init the local time
    init_time();

    // 5 Start the system state module to manage modes and actuators
    system_state_init();

    // 6 Start MQTT BROKER --> CORE 0
    xTaskCreatePinnedToCore(mqtt_broker_start, "MQTT BROKER TASK - CORE 0", 8192, NULL, 1, NULL, 0);

    // 7 Start MQTT PUBLISH / SUBSCRIBE
    mqtt_pubsub_start();

    // 8 Start HTTP Server on --> CORE 1
    xTaskCreatePinnedToCore(http_server_start, "HTTP SERVER TASK - CORE 1", 8192, NULL, 1, NULL, 1);

    // 9 Start the system state task for mode communication
    xTaskCreate(system_task, "SHVS_TASK", 4096, NULL, 3, NULL);
}
// ----
