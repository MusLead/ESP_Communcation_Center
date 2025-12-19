#include "sntp_time.h"
#include "esp_log.h"

void init_time(void)
{
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "0.de.pool.ntp.org");
    esp_sntp_setservername(1, "1.de.pool.ntp.org");
    esp_sntp_setservername(2, "2.de.pool.ntp.org");
    esp_sntp_setservername(3, "3.de.pool.ntp.org");

    esp_sntp_set_sync_interval(15 * 1000); // 15s

    esp_sntp_init();

    // Berlin (einfach + DST)
    setenv("TZ", "CET-1CEST", 1);
    tzset();
}