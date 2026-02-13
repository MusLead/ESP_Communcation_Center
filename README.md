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


## Architecture

This communication center is part of a larger IoT ecosystem:
- **ESP-GET_READY**: Manages ESP microcontroller communication and REST API endpoints
- **ESP_Communcation_Center**: Central broker for message routing and data management (this repository)
- **Application/Front-End**: User interface for monitoring and controlling the system

## License

MIT License - See [LICENSE](LICENSE) for details


# Reference

HTTPS_SERVER: [text](https://randomnerdtutorials.com/esp32-esp8266-https-ssl-tls/) ; [text](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_https_server.html)

- Mosquitto component : [espressif](https://components.espressif.com/components/espressif/mosquitto/versions/2.0.20~4/readme)

- Http_server: [espressif](https://developer.espressif.com/blog/2025/06/basic_http_server/)

- Mqtt :
  - [espressif](https://docs.espressif.com/projects/esp-idf/en/v4.3.3/esp32/api-reference/protocols/mqtt.html)
  - [Medium -- introduction to mqtt](https://medium.com/@vaishalinagori112/introduction-to-mqtt-and-setting-up-a-lab-for-mqtt-pentesting-on-macos-f6018183c2b7)
  - [developer -- espressif](https://developer.espressif.com/blog/2025/05/esp-idf-mosquitto-port/)
