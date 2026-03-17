#include "network_manager.h"
#include <stdbool.h>       
#include <time.h>          
#include "secrets.h"
#include "robot_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_sntp.h"
#include "esp_log.h"

static const char *TAG = "NET_MGR";

// Note: This function is static (private) so it doesn't need to be in the header
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Retrying WiFi...");
    }
}

void network_init(void) {
    // WiFi
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    // Time
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    setenv("TZ", TIME_ZONE_UK, 1);
    tzset();
}

// Now the compiler knows what "bool" and "false" are
bool network_is_night_time(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2016 - 1900)) return false; // Not synced yet

    if (SLEEP_HOUR_START > SLEEP_HOUR_END) {
        return (timeinfo.tm_hour >= SLEEP_HOUR_START || timeinfo.tm_hour < SLEEP_HOUR_END);
    } else {
        return (timeinfo.tm_hour >= SLEEP_HOUR_START && timeinfo.tm_hour < SLEEP_HOUR_END);
    }
}