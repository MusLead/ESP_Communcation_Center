#pragma once

// MQTT BROKER ADDRESS
#define MQTT_ADDR_URL "mqtt://localhost:1883"

void mqtt_pubsub_start(void);
void mqtt_publish(const char *topic, const char *msg, int qos);