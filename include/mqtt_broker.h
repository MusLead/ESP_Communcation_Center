#pragma once

extern char latest_temp[32];
void mqtt_message_cb(char *client, char *topic, char *data, int len, int qos, int retain);
void mqtt_broker_start(void *pvParameters);
