#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "wifi_manager.h"
#include "factory_reset.h"

static const char *TAG = "APP_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Sake IoT Booting ===");

    // Step 1: NVS + netif + event loop
    ESP_ERROR_CHECK(wifi_manager_init());

    // Step 2: Factory reset button (GPIO0, hold 5s)
    ESP_ERROR_CHECK(factory_reset_init());

    // Step 3: WiFi
    if (wifi_manager_is_configured()) {
        ESP_LOGI(TAG, "Credentials found — starting APSTA mode...");

        if (wifi_manager_connect() == ESP_OK) {
            ESP_LOGI(TAG, "Online! Dashboard → http://192.168.4.1");
        } else {
            // STA failed but AP+dashboard is still running
            // User can update credentials via http://192.168.4.1
            ESP_LOGW(TAG, "STA failed — AP still up, update credentials at http://192.168.4.1");
        }

    } else {
        // First boot — no credentials saved
        ESP_LOGI(TAG, "No credentials — open AP '%s' → http://192.168.4.1",
                 WIFI_MANAGER_SETUP_SSID);
        wifi_manager_start_portal();
    }

    // All work is in FreeRTOS tasks / HTTP server
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}