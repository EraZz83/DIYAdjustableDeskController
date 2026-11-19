// WebApiServer.cpp
#include "WebApiServer.h"
#include <LittleFS.h>
#include <math.h>

namespace {
String modeToStr(PositionManager::Mode m) {
  switch (m) {
    case PositionManager::Mode::Idle:       return "Idle";
    case PositionManager::Mode::PresetUp:   return "PresetUp";
    case PositionManager::Mode::PresetDown: return "PresetDown";
    case PositionManager::Mode::ManualUp:   return "ManualUp";
    case PositionManager::Mode::ManualDown: return "ManualDown";
    case PositionManager::Mode::Stopped:    return "Stopped";
    default: return "Unknown";
  }
}

String escapeJson(const String& raw) {
  String out;
  out.reserve(raw.length());
  for (size_t i = 0; i < raw.length(); ++i) {
    char c = raw[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c >= 0x20) {
      out += c;
    }
  }
  return out;
}

String extractJsonString(const String& body, const char* key) {
  String pat = String("\"") + key + "\"";
  int idx = body.indexOf(pat);
  if (idx < 0) return "";
  int colon = body.indexOf(':', idx + pat.length());
  if (colon < 0) return "";
  int q1 = body.indexOf('"', colon + 1);
  if (q1 < 0) return "";
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return "";
  return body.substring(q1 + 1, q2);
}

int extractJsonInt(const String& body, const char* key, int fallback = -1) {
  String pat = String("\"") + key + "\"";
  int idx = body.indexOf(pat);
  if (idx < 0) return fallback;
  int colon = body.indexOf(':', idx + pat.length());
  if (colon < 0) return fallback;
  int start = colon + 1;
  while (start < (int)body.length() && (body[start] == ' ' || body[start] == '\t')) {
    ++start;
  }
  if (start >= (int)body.length()) return fallback;
  return body.substring(start).toInt();
}
} // namespace

WebApiServer::WebApiServer(PositionManager& posMgr, VL53L1X_Sensor& sensor, uint16_t port)
: _server(port), _posMgr(posMgr), _sensor(sensor) {}

void WebApiServer::begin() {
  if (!LittleFS.begin(true)) {
    Serial.println("❌ LittleFS mount fehlgeschlagen!");
  }
  registerRoutes();
  _server.begin();
}

void WebApiServer::handle() {
  _server.handleClient();
  if ((_manualUp || _manualDown) && (millis() - _lastManualMs > MANUAL_TIMEOUT_MS)) {
    _manualUp = false;
    _manualDown = false;
  }
}

bool WebApiServer::manualUpRequested() const {
  return _manualUp && (millis() - _lastManualMs <= MANUAL_TIMEOUT_MS);
}

bool WebApiServer::manualDownRequested() const {
  return _manualDown && (millis() - _lastManualMs <= MANUAL_TIMEOUT_MS);
}

void WebApiServer::registerRoutes() {
  _server.serveStatic("/", LittleFS, "/index.html");
  _server.serveStatic("/style.css", LittleFS, "/style.css");
  _server.serveStatic("/script.js", LittleFS, "/script.js");

  _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  _server.on("/api/presets/goto", HTTP_POST, [this]() { handleGoto(); });
  _server.on("/api/presets/save", HTTP_POST, [this]() { handleSave(); });
  _server.on("/api/move", HTTP_POST, [this]() { handleMove(); });
  _server.on("/api/stop", HTTP_POST, [this]() { handleStop(); });
  _server.on("/api/zero", HTTP_POST, [this]() { handleZero(); });
}

void WebApiServer::sendJson(int code, const String& payload) {
  _server.send(code, "application/json; charset=utf-8", payload);
}

void WebApiServer::handleStatus() {
  const float tgt = _posMgr.targetCm();
  const String tgtStr = isnan(tgt) ? "null" : String(tgt, 1);

  String json = "{";
  json += "\"heightCm\":" + String(_currentCm, 1) + ",";
  json += "\"offsetCm\":" + String(_sensor.getOffset(), 1) + ",";
  json += "\"targetCm\":" + tgtStr + ",";
  json += "\"mode\":\"" + modeToStr(_posMgr.mode()) + "\",";
  json += "\"presets\":[";

  bool first = true;
  for (uint8_t u = 0; u < PositionManager::NUM_USERS; ++u) {
    for (uint8_t p = 0; p < PositionManager::NUM_PRESETS; ++p) {
      if (!first) json += ",";
      first = false;
      json += "{\"user\":" + String(u) +
              ",\"preset\":" + String(p) +
              ",\"name\":\"" + escapeJson(_posMgr.getPresetName(u, p)) + "\"" +
              ",\"cm\":" + String(_posMgr.getPreset(u, p), 1) + "}";
    }
  }
  json += "]}";
  sendJson(200, json);
}

void WebApiServer::handleGoto() {
  const String& body = _server.arg("plain");
  int u = extractJsonInt(body, "user", -1);
  int p = extractJsonInt(body, "preset", -1);
  if (u < 0 || u >= (int)PositionManager::NUM_USERS || p < 0 || p >= (int)PositionManager::NUM_PRESETS) {
    sendJson(400, "{\"error\":\"invalid user or preset\"}");
    return;
  }
  _manualUp = false;
  _manualDown = false;
  _posMgr.commandGoTo((uint8_t)u, (uint8_t)p);
  sendJson(200, "{\"ok\":true}");
}

void WebApiServer::handleSave() {
  const String& body = _server.arg("plain");
  int u = extractJsonInt(body, "user", -1);
  int p = extractJsonInt(body, "preset", -1);
  String name = extractJsonString(body, "name");
  if (u < 0 || u >= (int)PositionManager::NUM_USERS || p < 0 || p >= (int)PositionManager::NUM_PRESETS) {
    sendJson(400, "{\"error\":\"invalid user or preset\"}");
    return;
  }
  _posMgr.setPreset((uint8_t)u, (uint8_t)p, _currentCm, name);
  sendJson(200, "{\"ok\":true}");
}

void WebApiServer::handleMove() {
  const String& body = _server.arg("plain");
  String dir = extractJsonString(body, "direction");
  if (dir == "up") {
    _manualUp = true;
    _manualDown = false;
    _lastManualMs = millis();
  } else if (dir == "down") {
    _manualDown = true;
    _manualUp = false;
    _lastManualMs = millis();
  } else {
    _manualUp = false;
    _manualDown = false;
    _posMgr.stop();
  }
  sendJson(200, "{\"ok\":true}");
}

void WebApiServer::handleStop() {
  _manualUp = false;
  _manualDown = false;
  _posMgr.stop();
  sendJson(200, "{\"ok\":true}");
}

void WebApiServer::handleZero() {
  _sensor.setOffset(_currentCm);
  sendJson(200, "{\"ok\":true}");
}