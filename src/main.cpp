#include <Arduino.h>
#include <WiFi.h>

#include "WiFiManager.h"
#include "OTAHandler.h"
#include <ESPmDNS.h>
#include "VL53L1X_Sensor.h"
#include "PositionManager.h"
#include "WebApiServer.h"

// WLAN
const char *ssid = "TurkishAirlines_";
const char *password = "123Polizei";

// Motor-GPIOs (Relais/MOSFET)
constexpr int PIN_UP = 25;
constexpr int PIN_DOWN = 26;
constexpr bool RELAYS_ACTIVE_HIGH = false;

VL53L1X_Sensor distanceSensor;
PositionManager posMgr(PIN_UP, PIN_DOWN, RELAYS_ACTIVE_HIGH, /*hysteresis_cm*/ 0.3f);
WebApiServer apiServer(posMgr, distanceSensor, 80);

void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 Starte Tischsteuerung (Web-API)...");

  bool ok = connectToWiFi(ssid, password, 20);
  if (!ok) {
    Serial.println("⚠️ Fallback: Starte AP...");
    startAccessPoint("Tischsteuerung_AP", "12345678",
                     IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  }

  setupOTA("liftoff");

  if (MDNS.begin("liftoff")) {
    Serial.println("🚀 mDNS aktiv: http://liftoff.local");
  } else {
    Serial.println("⚠️ mDNS Start fehlgeschlagen");
  }

  if (!distanceSensor.begin(false)) {
    Serial.println("❌ VL53L1X init fehlgeschlagen!");
    while (1) delay(100);
  }

  posMgr.begin();

  posMgr.setCallback([](const PositionManager::Event &e) {
    switch (e.type) {
      case PositionManager::Event::Type::PresetSaved:
        Serial.printf("💾 Preset gespeichert: user=%u preset=%u -> %.1f cm\n", e.user, e.preset, e.target);
        break;
      case PositionManager::Event::Type::MoveStarted:
        Serial.printf("▶️  Fahrt gestartet zu %.1f cm\n", e.target);
        break;
      case PositionManager::Event::Type::MoveFinished:
        Serial.printf("✅ Ziel erreicht (%.1f cm)\n", e.target);
        break;
      case PositionManager::Event::Type::Stopped:
        Serial.println("⛔️ Stop");
        break;
    }
  });

  apiServer.begin();
}

void loop() {
  handleOTA();
  apiServer.handle();

  static bool filtInit = false;
  static float cmFilt = 0.0f;
  const float cmRaw = distanceSensor.readDistanceMM() / 10.0f;

  if (!filtInit) {
    cmFilt = cmRaw;
    filtInit = true;
  } else {
    cmFilt = 0.25f * cmRaw + 0.75f * cmFilt;
  }

  apiServer.setCurrentHeight(cmFilt);
  posMgr.manualHold(apiServer.manualUpRequested(), apiServer.manualDownRequested());
  posMgr.update(cmFilt);

  delay(60);
}