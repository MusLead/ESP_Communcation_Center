# ESP32 Smart Home API

## Overview

This document describes the MQTT topics and HTTP endpoints used to read sensor values and control actuators (fan, servo/window, door).

---

# MQTT API

The ESP32 uses the following MQTT topics:

| Topic           | Payload Format                     | Description                                        |
| --------------- | ---------------------------------- | -------------------------------------------------- |
| `ESP32/indoor`  | `Temp:<float>,H=<float>%,AQ:<int>` | Indoor sensors: temperature, humidity, air quality |
| `ESP32/outdoor` | `Temp:<float>,H=<float>%,AQ:<int>` | Outdoor sensors                                    |
| `ESP32/fan`     | `0` or `1`                         | Fan state: `1` = ON, `0` = OFF                     |
| `ESP32/window` && `ESP32/door`  | `0` or `1`                         | Window servo: `1` = 90°, `0` = 0°                  |
| `ESP32/wind`    | `<float>`                          | Wind speed in km/h                                 |

---

## Examples

### **Subscribe to fan state**

```bash
mosquitto_sub -h <ESP32-IP> -t ESP32/fan
```

### **Turn fan ON**

```bash
mosquitto_pub -h 192.168.37.136 -p 1883 -u esp32 -P 1234 -t ESP32/indoor -m "1"
```

### **Close window**

```bash
mosquitto_pub -h <ESP32-IP> -p 1883 -u esp32 -P 1234 -t ESP32/window -m "0"
```

---

# HTTP API

## Sensor Data

| Method | URI               | Description                                                            |
| ------ | ----------------- | ---------------------------------------------------------------------- |
| GET    | `/api/v1/sensors` | Returns current sensor values (temperature, humidity, AQI, wind speed) |
| GET    | `/api/v1/status`  | Returns actuator states (mode /  window / fan / door / absorber)                            |

---

## Actuator Control

| Method | URI                 | Body                        | Description                 |                       |
| ------ | ------------------- | --------------------------- | --------------------------- | --------------------- |
| POST   | `/api/v1/mode`      | `{"2"}`           |  MODE_AUTO_BEST = 0,MODE_AUTO_ECO = 1,MODE_MANUAL = 2,               | Change operation mode |
| POST   | `/api/v1/actuators` | `{ "fan": 1, "window": 0, "absorber": 0 }` | Control window / fan / door / absorber |                       |

---

## Schedule

| Method | URI                | Body                                   | Description            |
| ------ | ------------------ | -------------------------------------- | ---------------------- |
| GET    | `/api/v1/schedule` | –                                      | Fetch current schedule |
| POST   | `/api/v1/schedule` | `{ "start": "08:00", "end": "18:00", "mode": 0 }` | Set schedule           |

---

# Notes

* All MQTT messages are transferred as **strings**.
* Actuator messages use simple numeric payloads (`0` and `1`).
* HTTP POST requests require JSON bodies.
* MQTT and HTTP can be used at the same time — they do not interfere.

