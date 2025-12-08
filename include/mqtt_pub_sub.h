#pragma once

void mqtt_pubsub_start(void);
void mqtt_publish(const char *topic, const char *msg, int qos);
void mqtt_subscribe(const char *topic, int qos);