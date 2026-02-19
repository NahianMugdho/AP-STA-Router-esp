#include <WiFi.h>  // use <WiFi.h> for ESP32

const char* hotspot_ssid     = "25";
const char* hotspot_password = "mugdho1234";

const char* ap_ssid     = "ESP_Network";
const char* ap_password = "esp12345";

void setup() {
  Serial.begin(115200);

  // Enable AP + STA mode simultaneously
  WiFi.mode(WIFI_AP_STA);

  // Connect to mobile hotspot
  WiFi.begin(hotspot_ssid, hotspot_password);
  Serial.print("Connecting to hotspot");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  // Start Access Point for other ESPs
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("AP started. IP: " + WiFi.softAPIP().toString());
}

void loop() {
  // Your main logic here
}