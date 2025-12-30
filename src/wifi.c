#include "wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"

static const char *TAG = "ESP32_WIFI";
static esp_ip4_addr_t esp_ip_addr;

esp_ip4_addr_t wifi_get_ip()
{
    return esp_ip_addr;
}


// wifi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // start connecting to WIFI
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();

        // connection lost ... try to reconnect
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();

        // Connected to WIFI and print the IP
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_ip_addr = event->ip_info.ip; // store IP
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&esp_ip_addr));
    }
}

void connect_wifi()
{
    // get ESP ready to connect to WIFI
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // ===== STATIC IP CONFIGURATION =====
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = esp_ip4addr_aton("192.168.0.220");   // ESP32 static IP
    ip_info.gw.addr = esp_ip4addr_aton("192.168.0.1");     // Router gateway
    ip_info.netmask.addr = esp_ip4addr_aton("255.255.255.0");

    // disable DHCP client so static IP is actually used
    esp_netif_dhcpc_stop(netif);
    esp_netif_set_ip_info(netif, &ip_info);
    // ===================================

    // Register WiFi event handlers
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    // WIFI Name + Password
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}
