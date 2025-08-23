#include <WiFi.h>
#include "WiFiManager.h"

bool connectToWiFi(const char *ssid, const char *password, int maxTries) {
  Serial.print("🔌 Verbinde mit Heimnetzwerk");
  WiFi.begin(ssid, password);
  int tries = 0;

  while (WiFi.status() != WL_CONNECTED && tries < maxTries) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ STA verbunden!");
    Serial.print("📡 IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n❌ Verbindung fehlgeschlagen!");
    return false;
  }
}


void startAccessPoint(const char *ap_ssid, const char *ap_password, IPAddress ip, IPAddress gateway, IPAddress subnet) {
  if (!WiFi.softAPConfig(ip, gateway, subnet)) {
    Serial.println("❌ softAPConfig fehlgeschlagen!");
  }

  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("\n✅ Access Point gestartet:");
  Serial.print("\n📶 IP: ");
  Serial.println(WiFi.softAPIP());
}