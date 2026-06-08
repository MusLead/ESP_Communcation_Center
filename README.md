# ESP_Communcation_Center

This repository serves as the communication server/broker between ESP sensors/actuators and the client/front-end server for the Smart Home Ventilation IoT project.

## Overview

ESP_Communcation_Center acts as a central communication hub that:
- Receives data from ESP-based sensors and actuators
- Processes and routes messages between devices
- Provides an interface for the front-end server to interact with ESP devices
- Manages real-time communication and data synchronization

## Integration

This repository is designed to be used as a submodule in the [Smart-Home-Ventilation](https://github.com/MusLead/Smart-Home-Ventilation) project.

### Adding as a Submodule

To add this repository as a submodule to Smart-Home-Ventilation:

```bash
cd Smart-Home-Ventilation
git submodule add https://github.com/MusLead/ESP_Communcation_Center.git
git submodule update --init --recursive
```

### Updating the Submodule

To update this submodule to the latest version:

```bash
cd Smart-Home-Ventilation/ESP_Communcation_Center
git pull origin main
cd ..
git add ESP_Communcation_Center
git commit -m "Update ESP_Communcation_Center submodule"
```

## Quick Setup

### 1 Create WiFi credentials

Create a file named **`secret.ini`** in the project directory folder `config\` with the following content:

```
[wifi]
ssid = XXXX
password = XXXX
```

> ! Replace `XXXX` with your WiFi credentials.

### 2 Set MQTT Broker address

Open **`mqtt_pub_sub.h`** and set the IP address of your MQTT broker:

```c
#define MQTT_ADDR_URL "mqtt://192.168.X.X"
```

### 3 Startup order

! **Web server (SHVS_Front_End_Server), sensors, and actuators (ESP_Sensors_Actuators) must only be used after the following message appears in the monitor of the ESP_Communcation_Center project:**

```
MQTT Connected
```

Only after this message the system is fully operational.

## OLED Status Display

The communication center can use an optional I2C OLED display to show the current system status directly on the ESP32. The display code is implemented in `src/oled_status.c` and is started from `app_main()` through `oled_status_init()`.

### OLED Library

The OLED is integrated with the ESP-IDF LCD framework and the external SH1106 driver component:

```yaml
dependencies:
  tny-robotics/sh1106-esp-idf: "^1.0.0"
```

This dependency is declared in `src/idf_component.yml`. PlatformIO/ESP-IDF resolves it into `managed_components/tny-robotics__sh1106-esp-idf`.

The library provides the SH1106 panel driver function:

```c
esp_lcd_new_panel_sh1106(io_handle, &panel_config, &panel_handle);
```

This project also uses ESP-IDF's built-in I2C and LCD APIs:

- `driver/i2c_master.h` for creating the I2C master bus
- `esp_lcd_io_i2c.h` for LCD communication over I2C
- `esp_lcd_panel_ops.h` for panel reset, initialization, display on/off, and bitmap drawing
- `esp_lcd_panel_sh1106.h` from the TNY Robotics SH1106 component

### OLED Configuration

The OLED settings are configured through `platformio.ini` build flags:

```ini
-DOLED_STATUS_ENABLE=1
-DOLED_STATUS_SDA_GPIO=21
-DOLED_STATUS_SCL_GPIO=22
-DOLED_STATUS_I2C_ADDR=0x3C
```

Default values are also defined in `src/oled_status.c`, so the module still has fallback settings if a flag is missing:

- I2C port: `0`
- SDA GPIO: `21`
- SCL GPIO: `22`
- I2C address: `0x3C`
- Pixel clock: `400 kHz`
- Display size: `128 x 64`
- Update interval: `1000 ms`

### How The OLED Is Used

The OLED module runs as its own FreeRTOS task:

```c
xTaskCreate(oled_status_task, "oled_status_task", 4096, NULL, 1, NULL);
```

During startup, `oled_hw_init()`:

1. Creates the I2C master bus with `i2c_new_master_bus()`.
2. Creates the LCD I2C IO handle with `esp_lcd_new_panel_io_i2c()`.
3. Creates the SH1106 panel with `esp_lcd_new_panel_sh1106()`.
4. Resets and initializes the OLED panel.
5. Turns the OLED display on.

The module then refreshes the screen once per second. It reads WiFi and system-state data, formats the status lines, draws them into a local framebuffer, and sends the framebuffer to the OLED with:

```c
esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, OLED_STATUS_WIDTH, OLED_STATUS_HEIGHT, oled_buffer);
```

The text rendering is custom and minimal. `src/oled_status.c` contains a small bitmap font table, converts characters into pixels, writes them into `oled_buffer`, and finally transfers that buffer to the SH1106 display.

The displayed information includes:

- WiFi connection status
- WiFi SSID
- Communication center IP address
- Indoor ESP connection status and IP address
- Outdoor ESP connection status and IP address

If the OLED is not connected or initialization fails, the task logs a warning and exits. The rest of the communication center continues running normally.


## Architecture

This communication center is part of a larger IoT ecosystem:
- **ESP-GET_READY**: Manages ESP microcontroller communication and REST API endpoints
- **ESP_Communcation_Center**: Central broker for message routing and data management (this repository)
- **Application/Front-End**: User interface for monitoring and controlling the system

---
## Info 

Explanation for the Modes (Auto - Eco,Auto - Best, Manual) and each API functions can be found in the [api.md](/api.md) file.

## License

MIT License - See [LICENSE](LICENSE) for details


# Reference

HTTPS_SERVER: [ssl-tls](https://randomnerdtutorials.com/esp32-esp8266-https-ssl-tls/) ; [https-server](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_https_server.html)

- Mosquitto component : [espressif](https://components.espressif.com/components/espressif/mosquitto/versions/2.0.20~4/readme)

- Http_server: [espressif](https://developer.espressif.com/blog/2025/06/basic_http_server/)

- Mqtt :
  - [espressif](https://docs.espressif.com/projects/esp-idf/en/v4.3.3/esp32/api-reference/protocols/mqtt.html)
  - [Medium -- introduction to mqtt](https://medium.com/@vaishalinagori112/introduction-to-mqtt-and-setting-up-a-lab-for-mqtt-pentesting-on-macos-f6018183c2b7)
  - [developer -- espressif](https://developer.espressif.com/blog/2025/05/esp-idf-mosquitto-port/)

- OLED SH1106 component: [TNY Robotics sh1106-esp-idf](https://github.com/TNY-Robotics/sh1106-esp-idf)
- ESP-IDF LCD API: [Espressif LCD Peripheral API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/lcd/index.html)
