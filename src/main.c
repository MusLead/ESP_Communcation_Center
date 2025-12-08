//
// ----
// This implemntation starts the wifi and pins two task in two task:
// Core 0 for the mqtt_broker
// Core 1 for the http_server
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "wifi.h"
#include "esp_https_server.h"
#include "https_server.h"
#include "mqtt_broker.h"

void app_main(void)
{
    // TO FREE UP ANY ALLOCATED HEAP MEMORY AND TO PRINT THE EDF VERSION
    ESP_LOGI("MAIN", "[APP] Startup..");
    ESP_LOGI("MAIN", "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI("MAIN", "[APP] IDF version: %s", esp_get_idf_version());
    //

    // Always initialize the default NVS partition first
    nvs_flash_init();

    // start WIFI connection
    connect_wifi();

    // Start MQTT BROKER --> CORE 0
    xTaskCreatePinnedToCore(mqtt_broker_start, "MQTT BROKER TASK - CORE 0", 4096, NULL, 1, NULL, 0);

    // Start HTTP Server on --> CORE 1
    xTaskCreatePinnedToCore(https_server_start, "HTTP SERVER TASK - CORE 1", 4096, NULL, 1, NULL, 1);
}
// ----