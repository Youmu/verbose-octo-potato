#include "time_sync.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "time.h"

static const char *TAG = "time_sync";

time_sync::time_sync() {
    // Initialize SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

bool time_sync::SyncTime() {
    // Wait for time to be synchronized
    time_t now = 0;
    struct tm timeinfo = { };
    int retry = 0;
    const int retry_count = 10;

    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    if (retry < retry_count) {
        time(&now);
        localtime_r(&now, &timeinfo);
        ESP_LOGI(TAG, "Time synchronized: %s", asctime(&timeinfo));
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to synchronize time");
        return false;
    }
}