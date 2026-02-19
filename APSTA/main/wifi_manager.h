#pragma once

#include "esp_err.h"
#include "esp_wifi.h"

// ─────────────────────────────────────────────────────────────────────────────
//  AP Config  (hardcoded – অন্য ESP32 গুলো এই SSID/Password দিয়ে কানেক্ট করবে)
// ─────────────────────────────────────────────────────────────────────────────
#define WIFI_MANAGER_AP_SSID        "ESP_Network"
#define WIFI_MANAGER_AP_PASSWORD    "esp12345"      // min 8 chars for WPA2
#define WIFI_MANAGER_AP_CHANNEL     1
#define WIFI_MANAGER_AP_MAX_CONN    5

// ─────────────────────────────────────────────────────────────────────────────
//  Setup Portal SSID  (open – শুধু প্রথমবার WiFi configure করার জন্য)
// ─────────────────────────────────────────────────────────────────────────────
#define WIFI_MANAGER_SETUP_SSID     "Sake_IOT_Setup"
#define WIFI_MANAGER_SETUP_PASSWORD ""              // Open network

// ─────────────────────────────────────────────────────────────────────────────
//  NVS Storage Keys
// ─────────────────────────────────────────────────────────────────────────────
#define WIFI_NAMESPACE              "wifi_config"
#define WIFI_SSID_KEY               "ssid"
#define WIFI_PASS_KEY               "password"
#define WIFI_CONFIGURED_KEY         "configured"

// ─────────────────────────────────────────────────────────────────────────────
//  Timeouts
// ─────────────────────────────────────────────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_MS     30000           // 30 seconds

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

/** NVS + netif + event loop init — সবার আগে call করতে হবে */
esp_err_t wifi_manager_init(void);

/**
 * Saved credentials দিয়ে upstream WiFi তে connect করে APSTA mode চালু করে।
 * STA  → saved SSID (ইন্টারনেট পাওয়ার জন্য)
 * AP   → WIFI_MANAGER_AP_SSID  (অন্য ESP32 গুলো এখানে কানেক্ট করবে)
 */
esp_err_t wifi_manager_connect(void);

/** প্রথমবার boot এ — open AP + setup webpage চালু করে */
esp_err_t wifi_manager_start_portal(void);

/** HTTP server বন্ধ করে */
esp_err_t wifi_manager_stop_portal(void);

/** NVS তে credentials সেভ করে */
esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password);

/** Credentials মুছে দেয় (factory reset) */
esp_err_t wifi_manager_clear_credentials(void);

/** Credentials আছে কিনা চেক করে */
bool wifi_manager_is_configured(void);