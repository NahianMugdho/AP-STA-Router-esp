প্রথমবার (credentials নেই):
  → SSID: "Sake_IOT_Setup" (open) চালু হবে
  → http://192.168.4.1 এ গিয়ে WiFi দিলে restart হবে

পরের বার (credentials আছে):
  → APSTA mode চালু
  → STA: তোমার saved WiFi (internet)
  → AP:  "ESP_Network" / "esp12345"  ← অন্য ESP32 গুলো এখানে কানেক্ট করবে
  → Dashboard: http://192.168.4.1

  Single page dashboard এ যা দেখা যাবে:

✅ Upstream WiFi status + STA IP
📡 Connected devices (MAC + RSSI) — 5 সেকেন্ড পর পর auto refresh
⚙️ WiFi credentials update form (restart ছাড়াও চেঞ্জ করা যাবে)
🔌 AP info (hardcoded SSID/Password দেখাবে)
⚠️ Factory reset button

STA fail হলেও AP + dashboard চালু থাকবে, user সেখান থেকেই credentials ঠিক করতে পারবে।