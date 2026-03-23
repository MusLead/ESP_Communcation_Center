#pragma once

#include <stdbool.h>

void status_led_init(bool require_remote_connection);
void status_led_set_wifi_connected(bool connected);
void status_led_set_remote_connected(bool connected);
