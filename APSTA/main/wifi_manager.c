#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"
// NAPT: uses esp_netif_napt_enable() — available in ESP-IDF v5.x
// Requires CONFIG_LWIP_IP_FORWARD=y + CONFIG_LWIP_IPV4_NAPT=y (sdkconfig.defaults)
#include <string.h>
#include <stdio.h>
#include "led_config.h" 
static const char *TAG = "WIFI_MANAGER";

static EventGroupHandle_t wifi_event_group;
static esp_netif_t *ap_netif  = NULL;
static esp_netif_t *sta_netif = NULL;
static httpd_handle_t server  = NULL;
static int retry_count        = 0;

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define MAX_RETRY           8

// ═════════════════════════════════════════════════════════════════════════════
//  Single Page HTML
// ═════════════════════════════════════════════════════════════════════════════

static const char *single_page =
"<!DOCTYPE html><html lang='en'><head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Sake IoT</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:Arial,sans-serif;background:#0d1117;min-height:100vh;color:#e6edf3;padding:20px}"
"h1{font-size:1.5rem;color:#58a6ff;margin-bottom:4px}"
".sub{color:#8b949e;font-size:.82rem;margin-bottom:24px}"
".card{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:22px;max-width:520px;margin:0 auto 20px}"
".card h2{font-size:.85rem;text-transform:uppercase;letter-spacing:.8px;color:#8b949e;margin-bottom:16px}"
".stats{display:flex;gap:12px;flex-wrap:wrap;max-width:520px;margin:0 auto 20px}"
".stat{flex:1;min-width:130px;background:#161b22;border:1px solid #30363d;border-radius:10px;padding:14px 16px}"
".stat .val{font-size:1.25rem;font-weight:bold;color:#58a6ff}"
".stat .lbl{font-size:.72rem;color:#8b949e;margin-top:3px}"
"label{display:block;font-size:.8rem;color:#8b949e;margin-bottom:4px;margin-top:12px}"
"input{width:100%;padding:10px 12px;background:#0d1117;border:1px solid #30363d;border-radius:7px;color:#e6edf3;font-size:.9rem;outline:none}"
"input:focus{border-color:#58a6ff}"
".btn{width:100%;padding:11px;margin-top:18px;background:#238636;color:#fff;border:none;border-radius:7px;font-size:.95rem;font-weight:bold;cursor:pointer}"
".btn:hover{background:#2ea043}"
".btn-warn{background:#b62324}.btn-warn:hover{background:#da3633}"
"table{width:100%;border-collapse:collapse}"
"th,td{padding:9px 10px;text-align:left;font-size:.83rem;border-bottom:1px solid #21262d}"
"th{color:#8b949e;font-weight:normal}"
"td{color:#c9d1d9}"
"tr:last-child td{border:none}"
"code{background:#0d1117;padding:2px 6px;border-radius:4px;font-size:.8rem}"
".dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px}"
".green{background:#3fb950}.red{background:#f85149}.yellow{background:#d29922}"
"</style></head><body>"

"<div style='max-width:520px;margin:0 auto'>"
"<h1>&#127982; Sake IoT Gateway</h1>"
"<p class='sub'>ESP32 Wi-Fi Router Dashboard</p>"
"</div>"

"<div class='stats'>"
"<div class='stat'><div class='val' id='sta-status'><span class='dot yellow'></span>...</div><div class='lbl'>Upstream WiFi</div></div>"
"<div class='stat'><div class='val' id='sta-ip'>--</div><div class='lbl'>STA IP</div></div>"
"<div class='stat'><div class='val' id='dev-count'>--</div><div class='lbl'>Devices on AP</div></div>"
"</div>"

"<div class='card'>"
"<h2>&#128225; Connected Devices <span style='float:right;cursor:pointer;color:#58a6ff;font-size:.8rem' onclick='loadDevices()'>&#8635; Refresh</span></h2>"
"<table><thead><tr><th>#</th><th>MAC Address</th><th>RSSI</th></tr></thead>"
"<tbody id='dev-table'><tr><td colspan='3' style='color:#8b949e'>Loading...</td></tr></tbody>"
"</table></div>"

"<div class='card'>"
"<h2>&#9881; WiFi Setup</h2>"
"<p id='setup-note' style='font-size:.8rem;color:#8b949e;margin-bottom:8px'>Enter upstream WiFi credentials.</p>"
"<form id='wifi-form'>"
"<label>WiFi Name (SSID)</label>"
"<input type='text' id='f-ssid' placeholder='Enter SSID' required>"
"<label>Password</label>"
"<input type='password' id='f-pass' placeholder='Enter password' required>"
"<button class='btn' type='submit'>Save &amp; Restart</button>"
"</form>"
"<div id='save-msg' style='margin-top:12px;font-size:.85rem;color:#3fb950'></div>"
"</div>"

"<div class='card'>"
"<h2>&#128268; This Device AP (Hardcoded)</h2>"
"<table>"
"<tr><th>SSID</th><td><code>" WIFI_MANAGER_AP_SSID "</code></td></tr>"
"<tr><th>Password</th><td><code>" WIFI_MANAGER_AP_PASSWORD "</code></td></tr>"
"<tr><th>AP IP</th><td><code id='ap-ip'>192.168.4.1</code></td></tr>"
"<tr><th>NAT</th><td><code id='nat-status'>Enabled &#9989;</code></td></tr>"
"</table></div>"

"<div class='card'>"
"<h2>&#9888; Danger Zone</h2>"
"<button class='btn btn-warn' onclick='factoryReset()'>Factory Reset (Clear WiFi)</button>"
"</div>"

"<script>"
"async function loadStatus(){"
"try{"
"const r=await fetch('/status');const d=await r.json();"
"const ss=document.getElementById('sta-status');"
"ss.innerHTML=d.sta_connected?\"<span class='dot green'></span>Connected\":\"<span class='dot red'></span>Disconnected\";"
"document.getElementById('sta-ip').textContent=d.sta_ip||'--';"
"document.getElementById('ap-ip').textContent=d.ap_ip||'192.168.4.1';"
"document.getElementById('setup-note').textContent=d.sta_connected?"
"'WiFi connected. Update credentials below if needed.':'No upstream WiFi. Enter credentials.';"
"}catch(e){console.error(e);}}"

"async function loadDevices(){"
"try{"
"const r=await fetch('/devices');const d=await r.json();"
"document.getElementById('dev-count').textContent=d.count;"
"const tb=document.getElementById('dev-table');"
"tb.innerHTML=d.count===0?"
"\"<tr><td colspan='3' style='color:#8b949e'>No clients</td></tr>\":"
"d.clients.map((c,i)=>`<tr><td>${i+1}</td><td><code>${c.mac}</code></td><td>${c.rssi} dBm</td></tr>`).join('');"
"}catch(e){console.error(e);}}"

"document.getElementById('wifi-form').addEventListener('submit',async function(e){"
"e.preventDefault();"
"const msg=document.getElementById('save-msg');"
"msg.textContent='Saving...';"
"try{"
"const r=await fetch('/save',{method:'POST',"
"headers:{'Content-Type':'application/x-www-form-urlencoded'},"
"body:'ssid='+encodeURIComponent(document.getElementById('f-ssid').value)"
"+'&password='+encodeURIComponent(document.getElementById('f-pass').value)});"
"msg.textContent=r.ok?'Saved! Restarting...':'Error saving.';"
"msg.style.color=r.ok?'#3fb950':'#f85149';"
"}catch(e){msg.textContent='Error: '+e;msg.style.color='#f85149';}});"

"async function factoryReset(){"
"if(!confirm('Clear WiFi and restart?'))return;"
"await fetch('/reset',{method:'POST'});"
"alert('Restarting...');}"

"loadStatus();loadDevices();"
"setInterval(loadStatus,5000);setInterval(loadDevices,5000);"
"</script></body></html>";

static const char *success_page =
"<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Saved</title>"
"<style>body{background:#0d1117;color:#e6edf3;font-family:Arial;"
"display:flex;align-items:center;justify-content:center;height:100vh;margin:0;text-align:center}"
"h1{color:#3fb950;font-size:2rem}p{color:#8b949e;margin-top:12px}"
"</style></head><body><div><h1>&#9989; Saved!</h1><p>Credentials stored. Restarting...</p></div></body></html>";

// ═════════════════════════════════════════════════════════════════════════════
//  Event Handler
//  ✅ NAPT is enabled HERE — inside IP_EVENT_STA_GOT_IP, after getting IP
// ═════════════════════════════════════════════════════════════════════════════
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        led_blink_start(); 
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_count < MAX_RETRY) {
            esp_wifi_connect();
            retry_count++;
            led_blink_start(); // ← retry তেও blink চলতে থাকে
            ESP_LOGI(TAG, "Retry %d/%d ...", retry_count, MAX_RETRY);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            led_blink_start();
            ESP_LOGE(TAG, "STA: failed to connect.");
        }

    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        retry_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
         led_blink_stop();  // ← কানেক্টেড! blink বন্ধ
        led_on();          // ← steady ON
        // ── NAPT Enable ───────────────────────────────────────────────────
        // ap_netif এ NAPT enable করলে ESP_Network এর সব client ইন্টারনেট পাবে
        if (ap_netif) {
            esp_err_t napt_err = esp_netif_napt_enable(ap_netif);
            if (napt_err == ESP_OK) {
                ESP_LOGI(TAG, "NAPT enabled — AP clients now have internet access");
            } else {
                ESP_LOGE(TAG, "NAPT enable failed: %s", esp_err_to_name(napt_err));
            }
        }
        // ─────────────────────────────────────────────────────────────────

    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *ev = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "AP: client joined MAC=%02x:%02x:%02x:%02x:%02x:%02x",
                 ev->mac[0], ev->mac[1], ev->mac[2], ev->mac[3], ev->mac[4], ev->mac[5]);

    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *ev = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "AP: client left  MAC=%02x:%02x:%02x:%02x:%02x:%02x",
                 ev->mac[0], ev->mac[1], ev->mac[2], ev->mac[3], ev->mac[4], ev->mac[5]);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  HTTP Handlers
// ═════════════════════════════════════════════════════════════════════════════

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, single_page, strlen(single_page));
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char sta_ip_str[16] = "";
    char ap_ip_str[16]  = "192.168.4.1";
    bool sta_connected  = false;

    if (sta_netif) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(sta_netif, &ip) == ESP_OK && ip.ip.addr != 0) {
            snprintf(sta_ip_str, sizeof(sta_ip_str), IPSTR, IP2STR(&ip.ip));
            sta_connected = true;
        }
    }
    if (ap_netif) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(ap_netif, &ip) == ESP_OK) {
            snprintf(ap_ip_str, sizeof(ap_ip_str), IPSTR, IP2STR(&ip.ip));
        }
    }

    char buf[200];
    snprintf(buf, sizeof(buf),
             "{\"sta_connected\":%s,\"sta_ip\":\"%s\",\"ap_ip\":\"%s\",\"configured\":%s}",
             sta_connected ? "true" : "false",
             sta_ip_str, ap_ip_str,
             wifi_manager_is_configured() ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

static esp_err_t devices_handler(httpd_req_t *req)
{
    wifi_sta_list_t sta_list = {0};
    esp_wifi_ap_get_sta_list(&sta_list);

    char buf[800];
    int  pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "{\"count\":%d,\"clients\":[", sta_list.num);

    for (int i = 0; i < sta_list.num && pos < (int)sizeof(buf) - 70; i++) {
        uint8_t *m = sta_list.sta[i].mac;
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "%s{\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"rssi\":%d}",
                        i ? "," : "",
                        m[0], m[1], m[2], m[3], m[4], m[5],
                        sta_list.sta[i].rssi);
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, buf, pos);
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    char content[256];
    int  ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) { httpd_resp_send_500(req); return ESP_FAIL; }
    content[ret] = '\0';

    char ssid[33]     = {0};
    char password[65] = {0};

    char *ssid_ptr = strstr(content, "ssid=");
    char *pass_ptr = strstr(content, "password=");

    if (ssid_ptr && pass_ptr) {
        ssid_ptr += 5;
        char *ssid_end = strchr(ssid_ptr, '&');
        if (ssid_end) {
            int len = (int)(ssid_end - ssid_ptr);
            if (len > 32) len = 32;
            strncpy(ssid, ssid_ptr, len);
        }
        pass_ptr += 9;
        char *pass_end = strchr(pass_ptr, '&');
        int   plen     = pass_end ? (int)(pass_end - pass_ptr) : (int)strlen(pass_ptr);
        if (plen > 64) plen = 64;
        strncpy(password, pass_ptr, plen);

        for (int i = 0; ssid[i];     i++) if (ssid[i]     == '+') ssid[i]     = ' ';
        for (int i = 0; password[i]; i++) if (password[i] == '+') password[i] = ' ';

        ESP_LOGI(TAG, "Saving SSID: %s", ssid);
        wifi_manager_save_credentials(ssid, password);

        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, success_page, strlen(success_page));
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    } else {
        httpd_resp_send_500(req);
    }
    return ESP_OK;
}

static esp_err_t reset_handler(httpd_req_t *req)
{
    ESP_LOGW(TAG, "Factory reset via web!");
    wifi_manager_clear_credentials();
    httpd_resp_sendstr(req, "OK");
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return ESP_OK;
}

static esp_err_t start_webserver(void)
{
    httpd_config_t config   = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    const httpd_uri_t uris[] = {
        { .uri="/",        .method=HTTP_GET,  .handler=root_handler    },
        { .uri="/status",  .method=HTTP_GET,  .handler=status_handler  },
        { .uri="/devices", .method=HTTP_GET,  .handler=devices_handler },
        { .uri="/save",    .method=HTTP_POST, .handler=save_handler    },
        { .uri="/reset",   .method=HTTP_POST, .handler=reset_handler   },
    };

    for (int i = 0; i < (int)(sizeof(uris)/sizeof(uris[0])); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }

    ESP_LOGI(TAG, "HTTP server started: /, /status, /devices, /save, /reset");
    return ESP_OK;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Public API
// ═════════════════════════════════════════════════════════════════════════════

esp_err_t wifi_manager_init(void)
{
    wifi_event_group = xEventGroupCreate();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    return ESP_OK;
}

bool wifi_manager_is_configured(void)
{
    nvs_handle_t handle;
    uint8_t configured = 0;
    if (nvs_open(WIFI_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u8(handle, WIFI_CONFIGURED_KEY, &configured);
        nvs_close(handle);
    }
    return configured == 1;
}

esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    nvs_set_str(handle, WIFI_SSID_KEY, ssid);
    nvs_set_str(handle, WIFI_PASS_KEY, password);
    nvs_set_u8(handle, WIFI_CONFIGURED_KEY, 1);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Credentials saved");
    return ESP_OK;
}

esp_err_t wifi_manager_clear_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    nvs_erase_all(handle);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Credentials cleared");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(void)
{
    char ssid[33]     = {0};
    char password[65] = {0};
    size_t len;

    nvs_handle_t handle;
    if (nvs_open(WIFI_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return ESP_FAIL;
    len = sizeof(ssid);     nvs_get_str(handle, WIFI_SSID_KEY, ssid, &len);
    len = sizeof(password); nvs_get_str(handle, WIFI_PASS_KEY, password, &len);
    nvs_close(handle);

    if (strlen(ssid) == 0) { ESP_LOGW(TAG, "No saved credentials"); return ESP_FAIL; }

    ESP_LOGI(TAG, "APSTA — STA:'%s'  AP:'%s'", ssid, WIFI_MANAGER_AP_SSID);

    ap_netif  = esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid           = WIFI_MANAGER_AP_SSID,
            .ssid_len       = strlen(WIFI_MANAGER_AP_SSID),
            .password       = WIFI_MANAGER_AP_PASSWORD,
            .channel        = WIFI_MANAGER_AP_CHANNEL,
            .authmode       = WIFI_AUTH_WPA2_PSK,
            .max_connection = WIFI_MANAGER_AP_MAX_CONN,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid,     ssid,     sizeof(sta_cfg.sta.ssid)     - 1);
    strncpy((char *)sta_cfg.sta.password, password, sizeof(sta_cfg.sta.password) - 1);
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));

    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    start_webserver();

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected. Dashboard: http://192.168.4.1");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "STA failed. AP+dashboard still running at http://192.168.4.1");
    return ESP_FAIL;
}

esp_err_t wifi_manager_start_portal(void)
{
    ESP_LOGI(TAG, "Starting setup portal: '%s'", WIFI_MANAGER_SETUP_SSID);

    ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid           = WIFI_MANAGER_SETUP_SSID,
            .ssid_len       = strlen(WIFI_MANAGER_SETUP_SSID),
            .password       = WIFI_MANAGER_SETUP_PASSWORD,
            .channel        = WIFI_MANAGER_AP_CHANNEL,
            .authmode       = WIFI_AUTH_OPEN,
            .max_connection = 2,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connect to '%s' → http://192.168.4.1", WIFI_MANAGER_SETUP_SSID);
    start_webserver();
    return ESP_OK;
}

esp_err_t wifi_manager_stop_portal(void)
{
    if (server) { httpd_stop(server); server = NULL; }
    return ESP_OK;
}