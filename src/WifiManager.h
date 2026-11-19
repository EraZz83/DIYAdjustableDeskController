// WifiManager.h
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

class WifiManager {
public:
  WifiManager(const char* defaultSsid,
              const char* defaultPass,
              const char* apSsid = "Liftoff-Setup",
              const char* apPass = "liftoff123");

  void begin(uint32_t connectTimeoutMs = 12000);
  void handle();

  void saveCredentials(const String& ssid, const String& password);
  void clearCredentials();

  String ssid() const { return _ssid; }
  bool   hasPassword() const { return _password.length() > 0; }
  bool   isApMode() const { return _apMode; }
  bool   isConnected() const { return WiFi.status() == WL_CONNECTED; }
  String ipString() const;
  int    rssi() const;

private:
  void loadFromNvs();
  void startApFallback();

  const char* _defaultSsid;
  const char* _defaultPass;
  const char* _apSsid;
  const char* _apPass;

  String _ssid;
  String _password;
  bool   _apMode = false;

  Preferences _nvs;
  bool _nvsInit = false;
  static constexpr const char* NAMESPACE = "wifi";
};