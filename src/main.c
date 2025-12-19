//
// ----
// This implemntation starts the wifi and pins two task in two cores:
// Core 0 for the mqtt_broker
// Core 1 for the http_server
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
    // Display free heap and IDF version
    ESP_LOGI("MAIN", "[APP] Startup..");
    ESP_LOGI("MAIN", "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI("MAIN", "[APP] IDF version: %s", esp_get_idf_version());
    //

    // Always initialize the default NVS partition first
    nvs_flash_init();

    // start WIFI connection
    connect_wifi();

    // Start the system state module to manage modes and actuators
    init_time();

    // Start MQTT PUBLISH / SUBSCRIBE
    mqtt_pubsub_start();

    // Start the system state module to manage modes and actuators
    system_state_init();

    // Start MQTT BROKER --> CORE 0
    xTaskCreatePinnedToCore(mqtt_broker_start, "MQTT BROKER TASK - CORE 0", 4096, NULL, 1, NULL, 0);

    // Start HTTP Server on --> CORE 1
    xTaskCreatePinnedToCore(http_server_start, "HTTP SERVER TASK - CORE 1", 4096, NULL, 1, NULL, 1);

    // Start the system state task for mode communication
    xTaskCreate(system_task, "SHVS_TASK", 4096, NULL, 5, NULL);
}
// ----