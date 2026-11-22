// main.cpp
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include "PositionManager.h"
#include "VL53L1X_Sensor.h"
#include "WebApiServer.h"
#include "WifiManager.h"

namespace {
constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;
constexpr int PIN_RELAY_UP = 12;
constexpr int PIN_RELAY_DOWN = 13;
constexpr bool RELAY_ACTIVE_HIGH = true;
constexpr float HYSTERESIS_CM = 0.4f;

constexpr const char* DEFAULT_WIFI_SSID = "TurkishAirlines_";
constexpr const char* DEFAULT_WIFI_PASS = "123Polizei";
constexpr const char* AP_SSID = "Liftoff-Setup";
constexpr const char* AP_PASS = "liftoff123";
constexpr const char* HOSTNAME = "liftoff";
} // namespace

WifiManager wifiMgr(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS, AP_SSID, AP_PASS);
VL53L1X_Sensor sensor;
PositionManager posMgr(PIN_RELAY_UP, PIN_RELAY_DOWN, RELAY_ACTIVE_HIGH, HYSTERESIS_CM);
WebApiServer apiServer(posMgr, sensor, wifiMgr, 80);

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(PIN_SDA, PIN_SCL);
  sensor.begin();
  posMgr.begin();

  wifiMgr.begin(12000);

  if (MDNS.begin(HOSTNAME)) {
    Serial.printf("🚀 mDNS aktiv: http://%s.local\n", HOSTNAME);
  } else {
    Serial.println("⚠️ mDNS Start fehlgeschlagen");
  }

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.begin();

  apiServer.begin();
}

void loop() {
  ArduinoOTA.handle();
  wifiMgr.handle();
  apiServer.handle();

  const float heightCm = sensor.readDistanceCM();
  apiServer.setCurrentHeight(heightCm);

  if (apiServer.manualUpRequested() || apiServer.manualDownRequested()) {
    posMgr.manualHold(apiServer.manualUpRequested(), apiServer.manualDownRequested());
  } else if (posMgr.mode() == PositionManager::Mode::ManualUp ||
             posMgr.mode() == PositionManager::Mode::ManualDown) {
    posMgr.manualHold(false, false);
  }

  posMgr.update(heightCm);
  delay(20);
}