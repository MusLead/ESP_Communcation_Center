#pragma once
#include "esp_netif.h"

void connect_wifi(void);

esp_ip4_addr_t wifi_get_ip();
