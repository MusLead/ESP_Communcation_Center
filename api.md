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

## MQTT Communication Flow In This Repository

For the sensor-to-HTTP path, this project does **not** use a traditional MQTT client subscription such as `esp_mqtt_client_subscribe()` inside the HTTP server.

Instead, the communication center ESP32 runs the MQTT broker itself and registers a broker message callback that receives published sensor data.

### Startup sequence and function locations

1. `app_main()` starts the shared state, MQTT broker, MQTT client, HTTP server, and system task.
   * File: `src/main.c`
   * Functions:
     * `system_state_init()`
     * `mqtt_broker_start()`
     * `mqtt_pubsub_start()`
     * `http_server_start()`

2. When `mqtt_broker_start()` is called, the broker callback is configured before the broker starts running.
   * File: `src/mqtt_broker.c`
   * Function: `mqtt_broker_start(void *pvParameters)`
   * Important line of logic:
     * `.handle_message_cb = mqtt_message_cb`
   * After this setup, `mosq_broker_run(&config)` starts the broker loop. From that point on, every MQTT publish received by the broker can be forwarded to `mqtt_message_cb()`.

3. The sensor nodes publish their measured values to MQTT topics.
   * Indoor BME680:
     * File: `../ESP_Sensors_Actuators/ESP_Indoor_Sensors_Actuators/src/bme680_sensor.c`
     * Function: `bme680_read_task()`
     * Publish call: `mqtt_publish("ESP32/indoor", msg, 1)`
   * Outdoor BME680:
     * File: `../ESP_Sensors_Actuators/ESP_Outdoor_Sensors_Actuators/src/bme680_sensor.c`
     * Function: `bme680_read_task()`
     * Publish call: `mqtt_publish("ESP32/outdoor", msg, 1)`
   * Wind sensor:
     * File: `../ESP_Sensors_Actuators/ESP_Outdoor_Sensors_Actuators/src/anemometer.c`
     * Function: `anemometer_task()`
     * Publish call: `mqtt_publish("ESP32/wind", msg, 1)`

4. The broker callback `mqtt_message_cb()` collects the published sensor data.
   * File: `src/mqtt_broker.c`
   * Function: `mqtt_message_cb(char *client, char *topic, char *data, int len, int qos, int retain)`
   * Behavior:
     * Checks which topic was published, for example `ESP32/indoor`, `ESP32/outdoor`, or `ESP32/wind`
     * Parses the payload text with `strstr()`, `atof()`, and `atoi()`
     * Stores the latest values in shared global state:
       * `indoor_temp`
       * `indoor_humidity`
       * `indoor_aq`
       * `outdoor_temp`
       * `outdoor_humidity`
       * `outdoor_aq`
       * `wind_speed`
   * These shared variables are declared in `include/system_state.h` and defined in `src/system_state.c`.

5. The HTTP server returns the latest cached values from shared state.
   * File: `src/http_server.c`
   * Function: `sensors_get_handler(httpd_req_t *req)`
   * Behavior:
     * Locks `state_mutex`
     * Copies the latest sensor values from shared memory
     * Builds the JSON response
     * Returns that JSON through `GET /api/v1/sensors`

### Important clarification

For sensor collection, the communication center does **not** subscribe in the traditional MQTT client sense.

It works like this:

* Sensor ESP32 publishes data
* Embedded MQTT broker receives the publish
* Broker invokes `mqtt_message_cb()`
* Callback updates shared state
* HTTP endpoint reads shared state and returns JSON

So the data flow is:

`sensor -> MQTT publish -> broker callback -> shared state -> HTTP response`

### Where traditional MQTT subscribe is still used

Traditional MQTT subscribe is still used in this repository for actuator control on the sensor/actuator boards:

* `ESP_Indoor_Sensors_Actuators/src/mqtt_pub_sub.c`
  * subscribes to `ESP32/window`
  * subscribes to `ESP32/door`
* `ESP_Outdoor_Sensors_Actuators/src/mqtt_pub_sub.c`
  * subscribes to `ESP32/fan`
  * subscribes to `ESP32/absorber`

In other words:

* Sensor data into the communication center: broker callback, not traditional client subscribe
* Actuator commands to the remote boards: traditional MQTT subscribe

### Transport note: MQTT over TCP

Yes. In the current implementation this is standard MQTT over TCP.

Why:

* The broker runs on port `1883`
* The configured URIs use the `mqtt://` scheme
* There is no `ws://`, `wss://`, or `mqtts://` transport configured in the current code

Relevant locations:

* `include/mqtt_pub_sub.h` in this module uses `mqtt://localhost:1883`
* The sensor nodes use `mqtt://192.168.0.130:1883`

That means the current implementation is plain MQTT over TCP, without TLS, and not MQTT over WebSocket.

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

All `GET` endpoints respond with `Content-Type: application/json`.

## GET Endpoints

| Method | URI               | Description                                                            |
| ------ | ----------------- | ---------------------------------------------------------------------- |
| GET    | `/api/v1/sensors` | Returns current sensor values (temperature, humidity, AQI, wind speed) |
| GET    | `/api/v1/status`  | Returns mode, actuator states, schedule state flags, and status explanation |
| GET    | `/api/v1/schedule` | Returns the currently configured schedule periods                      |

### `GET /api/v1/sensors`

Returns the latest snapshot of indoor sensors, outdoor sensors, and wind speed.

If a source stops publishing for longer than its timeout window, the communication center resets only that source buffer to `0` internally and returns `--` for the stale values in the HTTP response.

Current timeout behavior:

* Indoor BME680 data: `1000 ms`
* Outdoor BME680 data: `1000 ms`
* Wind data: `5000 ms`

Timeouts are tracked independently, so if the indoor ESP stops publishing, only the indoor values become `--`. Outdoor and wind values remain available until their own timeout expires.

Example response:

```json
{
  "connections": {
    "indoor": true,
    "outdoor": true
  },
  "indoor": {
    "Temp": 23.50,
    "H": 54.20,
    "AQ": 78
  },
  "outdoor": {
    "Temp": 18.40,
    "H": 61.00,
    "AQ": 42
  },
  "wind_speed": 7.3
}
```

Field format:

* Fresh values:
  * `indoor.Temp`, `indoor.H`, `outdoor.Temp`, `outdoor.H`: floating-point numbers
  * `indoor.AQ`, `outdoor.AQ`: integers
  * `wind_speed`: floating-point number
* Stale values:
  * the corresponding field is returned as the string `"--"`
* Connection flags:
  * `connections.indoor`: `true` if the indoor ESP has published within the timeout window
  * `connections.outdoor`: `true` if the outdoor ESP has published outdoor or wind data within the timeout window

Important:

* The JSON keys are exactly `Temp`, `H`, and `AQ` with capital letters.
* `wind_speed` is outside the `indoor` and `outdoor` objects.
* A timeout in one source does not clear the other source buffers.

### `GET /api/v1/status`

Returns the current system mode, actuator states, schedule-related control flags, and the current status explanation produced by the control logic.

Example response:

```json
{
  "mode": 2,
  "window": 1,
  "fan": 0,
  "door": 0,
  "absorber": 1,
  "scheduleConfigured": true,
  "scheduleActive": false,
  "scheduleHoldingState": true,
  "manualControlAllowed": true,
  "whyHeadline": "Take over for now",
  "why": "Schedule inactive: manual override is active until the next schedule period starts."
}
```

Field format:

* `mode`: integer
  * `0` = `MODE_AUTO_BEST`
  * `1` = `MODE_AUTO_ECO`
  * `2` = `MODE_MANUAL`
* `window`, `fan`, `door`, `absorber`: numeric flags
  * `1` = ON / OPEN / ACTIVE
  * `0` = OFF / CLOSED / INACTIVE
* `scheduleConfigured`: boolean
  * `true` if at least one schedule period is stored
  * `false` if no schedule is configured
* `scheduleActive`: boolean
  * `true` if the current time is inside an active schedule period
  * `false` if no configured period currently applies
* `scheduleHoldingState`: boolean
  * `true` if a schedule exists but is currently inactive, so the system is holding manual override until the next period starts
  * `false` otherwise
* `manualControlAllowed`: boolean
  * `true` if the frontend is allowed to manually control actuators
  * `false` if actuator control should stay locked to automatic logic
* `whyHeadline`: string
  * short status headline generated by the control logic
* `why`: string
  * detailed human-readable explanation for the current system state

### `GET /api/v1/schedule`

Returns all active schedule periods in a `periods` array.

Example response:

```json
{
  "periods": [
    {
      "start": "08:00",
      "end": "18:00",
      "mode": "0"
    },
    {
      "start": "18:00",
      "end": "23:00",
      "mode": "1"
    }
  ]
}
```

Field format:

* `start`, `end`: strings in `HH:MM` format
* `mode`: string containing the numeric mode value
  * `"0"` = `MODE_AUTO_BEST`
  * `"1"` = `MODE_AUTO_ECO`
  * `"2"` = `MODE_MANUAL`

Important:

* In the current implementation, `mode` is returned as a string in the schedule response.
* If no schedule is configured, the response is:

```json
{
  "periods": []
}
```

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
| POST   | `/api/v1/schedule` | `{ "start": "08:00", "end": "18:00", "mode": 0 }` | Set schedule           |

---
## System Logic Info

The main system logic is implemented in:

`src/system/system_state.c`

This module controls the system actuators:

* `window`
* `fan`
* `door`
* `absorber`

The logic is executed periodically inside `system_task()` (every **1 second**) using the function `system_auto_update()`.

---

# System Modes

## MODE_MANUAL

Manual mode disables all automatic decisions.

Behavior:

* The system **does not change any actuator state automatically**.
* `window`, `fan`, `door`, and `absorber` must be controlled externally (for example via **MQTT** or a UI).
* `system_auto_update()` exits immediately when this mode is active.

Use case:

* Manual testing
* External control systems

---

## MODE_AUTO_BEST

This mode prioritizes **best air quality and humidity reduction**.

### Window

The window opens when:

* `(indoor_humidity - outdoor_humidity) > HUMIDITY_DIFF_THRESHOLD`
* `outdoor_aq <= GOOD_AQ`

Meaning:

* Indoor air is more humid than outdoor air
* Outdoor air quality is good

### Fan

The fan turns on when:

* `window == true`
* `indoor_aq > GOOD_AQ`

Meaning:

* Ventilation is needed because indoor air quality is worse than the defined threshold.

### Absorber

The humidity absorber is activated when:

```
indoor_humidity > HIGH_HUMIDITY
```

### Door

The door state depends on wind speed:

```
door = (wind_speed > WIND_HIGH)
```

---

## MODE_AUTO_ECO

This mode focuses on **energy-efficient ventilation**.

### Window

The window opens only when:

```
outdoor_aq <= GOOD_AQ
```

If outdoor air quality is bad:

```
window = false
fan = false
```

### Fan

If the window is open and wind speed is acceptable:

Fan turns on when:

* `indoor_humidity > HIGH_HUMIDITY`
* OR `indoor_temp > 25°C`

If wind speed is high:

```
fan = false
```

### Absorber

Activated when:

```
indoor_humidity > HIGH_HUMIDITY
```

### Door

Door closes automatically when wind speed is too high:

```
door = (wind_speed > WIND_HIGH)
```

---

# Safety Rule

The system enforces an important constraint:

```
Fan cannot run if the window is closed
```

If this happens:

```
fan = false
```

---

# Schedule System

The system can use **time-based schedules**.

Each schedule entry contains:

* `start time`
* `end time`
* `mode`

Example:

```
08:00 - 18:00  → MODE_AUTO_ECO
18:00 - 23:00  → MODE_AUTO_BEST
```

Behavior:

* If the current time is inside a schedule → that mode becomes active.
* If the time is outside all schedules → all actuators are turned off.

```
window = false
fan = false
door = false
absorber = false
```

Schedules support **overnight ranges**, for example:

```
22:00 - 06:00
```

---

# Important Functions

Main functions inside `system_state.c`:

```
system_state_init()      // Initializes mutex
system_auto_update()     // Executes the main system logic
system_task()            // FreeRTOS task (runs every second)

apply_auto_logic()       // Implements AUTO_BEST and AUTO_ECO logic
schedule_is_active()     // Checks if a schedule is active
publish_state()          // Publishes state changes via MQTT
time_to_minutes()        // Converts HH:MM to minutes
```
---


# Notes

* All MQTT messages are transferred as **strings**.
* Actuator messages use simple numeric payloads (`0` and `1`).
* HTTP POST requests require JSON bodies.
* MQTT and HTTP can be used at the same time — they do not interfere.
