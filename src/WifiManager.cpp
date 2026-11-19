// WifiManager.cpp
#include "WifiManager.h"

WifiManager::WifiManager(const char* defaultSsid,
                       const char* defaultPass,
                       const char* apSsid,
                       const char* apPass)
: _defaultSsid(defaultSsid),
  _defaultPass(defaultPass),
  _apSsid(apSsid),
  _apPass(apPass) {}

void WifiManager::begin(uint32_t connectTimeoutMs) {
  if (_nvs.begin(NAMESPACE, /*readOnly*/ false)) {
    _nvsInit = true;
    loadFromNvs();
  }

  if (_ssid.length() == 0) {
    _ssid = _defaultSsid ? _defaultSsid : "";
    _password = _defaultPass ? _defaultPass : "";
  }

  if (_ssid.length() == 0) {
    startApFallback();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid.c_str(), _password.c_str());
  Serial.printf("📶 Verbinde mit WLAN '%s'...\n", _ssid.c_str());

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < connectTimeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    _apMode = false;
    Serial.printf("✅ WLAN verbunden (IP: %s, RSSI: %d dBm)\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("⚠️ WLAN-Verbindung fehlgeschlagen -> starte Setup-AP");
    startApFallback();
  }
}

void WifiManager::handle() {
  // Platzhalter für spätere Auto-Reconnect-Überwachung im STA-Modus
}

void WifiManager::loadFromNvs() {
  if (!_nvsInit) return;
  _ssid = _nvs.getString("ssid", "");
  _password = _nvs.getString("pass", "");
}

void WifiManager::saveCredentials(const String& ssid, const String& password) {
  String cleanSsid = ssid;
  cleanSsid.trim();
  if (cleanSsid.length() > 63) cleanSsid = cleanSsid.substring(0, 63);

  _ssid = cleanSsid;
  if (password.length() > 0) {
    _password = password;
  }

  if (_nvsInit) {
    _nvs.putString("ssid", _ssid);
    _nvs.putString("pass", _password);
  }
}

void WifiManager::clearCredentials() {
  _ssid = _defaultSsid ? _defaultSsid : "";
  _password = _defaultPass ? _defaultPass : "";
  if (_nvsInit) {
    _nvs.remove("ssid");
    _nvs.remove("pass");
  }
}

void WifiManager::startApFallback() {
  _apMode = true;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(_apSsid, _apPass);
  Serial.printf("📡 Setup-AP aktiv: SSID='%s', Pass='%s', IP=%s\n",
                _apSsid, _apPass, WiFi.softAPIP().toString().c_str());
}

String WifiManager::ipString() const {
  if (_apMode) return WiFi.softAPIP().toString();
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  return "0.0.0.0";
}

int WifiManager::rssi() const {
  if (_apMode || WiFi.status() != WL_CONNECTED) return 0;
  return WiFi.RSSI();
}