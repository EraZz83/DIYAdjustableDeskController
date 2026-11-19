// WebApiServer.h
#pragma once
#include <WebServer.h>
#include "PositionManager.h"
#include "VL53L1X_Sensor.h"
#include "WifiManager.h"

class WebApiServer {
public:
  WebApiServer(PositionManager& posMgr,
               VL53L1X_Sensor& sensor,
               WifiManager& wifiMgr,
               uint16_t port = 80);

  void begin();
  void handle();

  void setCurrentHeight(float heightCm) { _currentCm = heightCm; }
  bool manualUpRequested() const;
  bool manualDownRequested() const;

private:
  WebServer _server;
  PositionManager& _posMgr;
  VL53L1X_Sensor& _sensor;
  WifiManager& _wifiMgr;

  float _currentCm = 0.0f;
  bool _manualUp = false;
  bool _manualDown = false;
  uint32_t _lastManualMs = 0;
  static constexpr uint32_t MANUAL_TIMEOUT_MS = 600;

  void registerRoutes();
  void handleStatus();
  void handleGoto();
  void handleSave();
  void handleDelete();
  void handleMove();
  void handleStop();
  void handleZero();
  void handleWifiGet();
  void handleWifiSave();

  void sendJson(int code, const String& payload);
};
