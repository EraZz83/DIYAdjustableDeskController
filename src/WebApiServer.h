// WebApiServer.h
#pragma once
#include <WebServer.h>
#include "PositionManager.h"
#include "VL53L1X_Sensor.h"

class WebApiServer {
public:
  WebApiServer(PositionManager& posMgr, VL53L1X_Sensor& sensor, uint16_t port = 80);

  void begin();
  void handle();

  void setCurrentHeight(float cm) { _currentCm = cm; }
  bool manualUpRequested() const;
  bool manualDownRequested() const;

private:
  WebServer _server;
  PositionManager& _posMgr;
  VL53L1X_Sensor& _sensor;

  float _currentCm = 0.0f;
  bool _manualUp = false;
  bool _manualDown = false;
  uint32_t _lastManualMs = 0;
  static constexpr uint32_t MANUAL_TIMEOUT_MS = 900;

  void registerRoutes();
  void handleStatus();
  void handleGoto();
  void handleSave();
  void handleMove();
  void handleStop();
  void handleZero();
  void sendJson(int code, const String& payload);
};