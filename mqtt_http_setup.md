# ESP32 Network Framework

## Project Structure
```
include/
├── wifi.h          # WiFi connection manager
├── http_server.h   # HTTPS web server
└── mqtt_broker.h   # Secure MQTT broker

src/
├── wifi.c
├── http_server.c
└── mqtt_broker.c
```

## Quick Start

### 1. Copy Basic Setup
```c
#include "wifi.h"
#include "http_server.h"
#include "mqtt_broker.h"

void app_main(void)
{
    // Before starting the http and mqtt serve do:
    // TO MAKE FREE UP ANY ALLOCATED HEAP MEMORY AND TO PRINT THE EDF VERSION
    ESP_LOGI("MAIN", "[APP] Startup..");
    ESP_LOGI("MAIN", "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI("MAIN", "[APP] IDF version: %s", esp_get_idf_version());
    //

    // 1.
    // Initialize system storage
    nvs_flash_init();

    // 2.
    // Connect to WiFi
    connect_wifi();

    // 3.
    // Start network services on different cores
    xTaskCreatePinnedToCore(mqtt_broker_start, "MQTT Broker", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(http_server_start, "HTTP Server", 4096, NULL, 1, NULL, 1);
}
```

### 2. Configure WiFi
Edit `src/wifi.c`:
```c
// Enter your network credentials
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

### 3. Add Certificates
Create folder `main/certs/` and add:
- `server.crt` - Server certificate
- `server.key` - Private key  
- `ca.crt` - CA certificate

## Services Overview

### HTTP Server
**Port:** 80 (standard HTTP)
**Access:** `http://[ESP32-IP-ADDRESS]/`

**Response Format:**
```json
{ "temp": "23.5" }
```

## Accessing Services

### Option A: Web Browser
```
https://192.168.1.100/
```

### Option B: MQTT Client
```bash
# Using mosquitto client
mosquitto_pub -h 192.168.178.50 -p 8883 -t ESP32/values -m "HELLO this is a Test" \
--cafile ca.crt --cert client.crt --key client.key
```

### Option C: cURL
```bash
# HTTPS (self-signed certificate, use -k)
curl -k https://192.168.1.100/
```

### Server Ports
```c
// In mqtt_broker.c
.port = 8883  // MQTT TLS port

// In http_server.c
.server_port = 80  // HTTP port
```

## Important Global Variables

```c
// Global temperature value (used by MQTT/HTTP)
extern char latest_temp[32];
```

## Core Distribution

- **Core 0**: MQTT Broker (with TLS encryption)
- **Core 1**: HTTP Server (HTTPS processing)

**Advantages:**
- Better performance
- No blocking between services
- Stable connections

---

After flashing:
1. ESP32 connects to WiFi
2. HTTPS server starts on port 443
3. MQTT broker starts on port 8883
4. Data is available through both interfaces