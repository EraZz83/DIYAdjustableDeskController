// WebApiServer.cpp
#include "WebApiServer.h"
#include <LittleFS.h>
#include <cmath>

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
    } else if (static_cast<uint8_t>(c) >= 0x20) {
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
  while (start < static_cast<int>(body.length()) && (body[start] == ' ' || body[start] == '\t')) {
    ++start;
  }
  if (start >= static_cast<int>(body.length())) return fallback;
  return body.substring(start).toInt();
}

float extractJsonFloat(const String& body, const char* key, float fallback = NAN) {
  String pat = String("\"") + key + "\"";
  int idx = body.indexOf(pat);
  if (idx < 0) return fallback;
  int colon = body.indexOf(':', idx + pat.length());
  if (colon < 0) return fallback;
  int start = colon + 1;
  while (start < static_cast<int>(body.length()) && (body[start] == ' ' || body[start] == '\t')) {
    ++start;
  }
  if (start >= static_cast<int>(body.length())) return fallback;
  return body.substring(start).toFloat();
}
} // namespace

WebApiServer::WebApiServer(PositionManager& posMgr,
                         VL53L1X_Sensor& sensor,
                         WifiManager& wifiMgr,
                         uint16_t port)
: _server(port), _posMgr(posMgr), _sensor(sensor), _wifiMgr(wifiMgr) {}

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
  _server.on("/api/presets/add", HTTP_POST, [this]() { handleSave(); });
  _server.on("/api/presets/delete", HTTP_POST, [this]() { handleDelete(); });
  _server.on("/api/target", HTTP_POST, [this]() { handleTarget(); });
  _server.on("/api/offset", HTTP_POST, [this]() { handleOffset(); });
  _server.on("/api/limits", HTTP_POST, [this]() { handleLimitsSave(); });
  _server.on("/api/move", HTTP_POST, [this]() { handleMove(); });
  _server.on("/api/stop", HTTP_POST, [this]() { handleStop(); });
  _server.on("/api/wifi", HTTP_GET, [this]() { handleWifiGet(); });
  _server.on("/api/wifi/save", HTTP_POST, [this]() { handleWifiSave(); });
}

void WebApiServer::sendJson(int code, const String& payload) {
  _server.send(code, "application/json; charset=utf-8", payload);
}

void WebApiServer::handleStatus() {
  const float tgt = _posMgr.targetCm();
  const String tgtStr = std::isnan(tgt) ? "null" : String(tgt, 1);
  const bool isMoving = _posMgr.isMoving();

  String json = "{";
  json += "\"heightCm\":" + String(_currentCm, 1) + ",";
  json += "\"offsetCm\":" + String(_sensor.getOffset(), 1) + ",";
  json += "\"targetCm\":" + tgtStr + ",";
  json += "\"limitMin\":" + String(_posMgr.minLimitCm(), 1) + ",";
  json += "\"limitMax\":" + String(_posMgr.maxLimitCm(), 1) + ",";
  json += "\"mode\":\"" + modeToStr(_posMgr.mode()) + "\",";
  json += "\"moving\":" + String(isMoving ? "true" : "false") + ",";
  json += "\"presets\":[";

  const auto& presets = _posMgr.getPresets();
  for (size_t i = 0; i < presets.size(); ++i) {
    if (i > 0) json += ",";
    json += "{\"id\":" + String(presets[i].id) +
            ",\"name\":\"" + escapeJson(presets[i].name) + "\"" +
            ",\"cm\":" + String(presets[i].heightCm, 1) + "}";
  }
  json += "]}";
  sendJson(200, json);
}

void WebApiServer::handleGoto() {
  const String& body = _server.arg("plain");
  int id = extractJsonInt(body, "id", -1);
  if (id < 0) {
    id = extractJsonInt(body, "preset", -1);
  }
  if (id < 0) {
    sendJson(400, "{\"error\":\"id required\"}");
    return;
  }

  const auto* p = _posMgr.findPreset(static_cast<uint32_t>(id));
  if (p) {
    Serial.printf("🔘 [UI] Button gedrückt: ANFAHREN -> \"%s\" (Ziel: %.1f cm, Ist: %.1f cm)\n",
                  p->name.c_str(), p->heightCm, _currentCm);
  } else {
    Serial.printf("🔘 [UI] Button gedrückt: ANFAHREN -> Preset ID %d\n", id);
  }

  _manualUp = false;
  _manualDown = false;
  _posMgr.commandGoTo(static_cast<uint32_t>(id));
  sendJson(200, "{\"ok\":true}");
}

void WebApiServer::handleTarget() {
  const String& body = _server.arg("plain");
  float targetCm = extractJsonFloat(body, "targetCm", NAN);

  if (std::isnan(targetCm)) {
    sendJson(400, "{\"error\":\"targetCm required\"}");
    return;
  }

  Serial.printf("🎯 [UI] Sollwert eingegeben: %.1f cm (Fahre dorthin...)\n", targetCm);

  _manualUp = false;
  _manualDown = false;
  _posMgr.commandGoToHeight(targetCm);

  String resp = "{\"ok\":true,\"targetCm\":" + String(_posMgr.targetCm(), 1) + "}";
  sendJson(200, resp);
}

void WebApiServer::handleSave() {
  const String& body = _server.arg("plain");
  String name = extractJsonString(body, "name");
  float cm = extractJsonFloat(body, "cm", NAN);

  if (std::isnan(cm) || cm < 0.0f) {
    cm = _currentCm;
  }

  Serial.printf("💾 [UI] Button gedrückt: PRESET SPEICHERN -> Name: \"%s\", Höhe: %.1f cm\n",
                name.c_str(), cm);

  _posMgr.addPreset(name, cm);
  sendJson(200, "{\"ok\":true}");
}

void WebApiServer::handleDelete() {
  const String& body = _server.arg("plain");
  int id = extractJsonInt(body, "id", -1);
  if (id < 0) {
    sendJson(400, "{\"error\":\"id required\"}");
    return;
  }

  const auto* p = _posMgr.findPreset(static_cast<uint32_t>(id));
  if (p) {
    Serial.printf("🗑️ [UI] Button gedrückt: PRESET LÖSCHEN -> \"%s\" (ID: %d, %.1f cm)\n",
                  p->name.c_str(), id, p->heightCm);
  } else {
    Serial.printf("🗑️ [UI] Button gedrückt: PRESET LÖSCHEN -> ID: %d\n", id);
  }

  bool deleted = _posMgr.deletePreset(static_cast<uint32_t>(id));
  if (deleted) {
    sendJson(200, "{\"ok\":true}");
  } else {
    sendJson(404, "{\"error\":\"not found\"}");
  }
}

void WebApiServer::handleOffset() {
  const String& body = _server.arg("plain");
  float offsetCm = extractJsonFloat(body, "offsetCm", NAN);

  if (std::isnan(offsetCm)) {
    sendJson(400, "{\"error\":\"offsetCm required\"}");
    return;
  }

  _sensor.setOffset(offsetCm);
  Serial.printf("⚙️ [Offset] Sensor-Offset (Totbereich) auf %.1f cm gesetzt\n", _sensor.getOffset());

  String resp = "{\"ok\":true,\"offsetCm\":" + String(_sensor.getOffset(), 1) + "}";
  sendJson(200, resp);
}

void WebApiServer::handleLimitsSave() {
  const String& body = _server.arg("plain");
  float minCm = extractJsonFloat(body, "min", NAN);
  float maxCm = extractJsonFloat(body, "max", NAN);
  float offsetCm = extractJsonFloat(body, "offsetCm", NAN);

  if (!std::isnan(offsetCm)) {
    _sensor.setOffset(offsetCm);
    Serial.printf("⚙️ [Offset] Sensor-Offset (Totbereich) auf %.1f cm gesetzt\n", _sensor.getOffset());
  }

  if (!std::isnan(minCm) && !std::isnan(maxCm)) {
    if (minCm < maxCm) {
      _posMgr.setLimits(minCm, maxCm);
    } else {
      sendJson(400, "{\"error\":\"invalid limits (min must be less than max)\"}");
      return;
    }
  }

  String resp = "{\"ok\":true,\"min\":" + String(_posMgr.minLimitCm(), 1) +
                ",\"max\":" + String(_posMgr.maxLimitCm(), 1) +
                ",\"offsetCm\":" + String(_sensor.getOffset(), 1) + "}";
  sendJson(200, resp);
}

void WebApiServer::handleMove() {
  const String& body = _server.arg("plain");
  String dir = extractJsonString(body, "direction");

  if (dir == "up") {
    if (!_manualUp) {
      Serial.printf("🔘 [UI] Button gedrückt: HOCH (Start manuelle Aufwärtsfahrt bei %.1f cm)\n", _currentCm);
    }
    _manualUp = true;
    _manualDown = false;
    _lastManualMs = millis();
  } else if (dir == "down") {
    if (!_manualDown) {
      Serial.printf("🔘 [UI] Button gedrückt: RUNTER (Start manuelle Abwärtsfahrt bei %.1f cm)\n", _currentCm);
    }
    _manualDown = true;
    _manualUp = false;
    _lastManualMs = millis();
  } else {
    if (_manualUp || _manualDown) {
      Serial.printf("⏹️ [UI] Button losgelassen: Manuelle Fahrt beendet (Position: %.1f cm)\n", _currentCm);
    }
    _manualUp = false;
    _manualDown = false;
    _posMgr.stop();
  }
  sendJson(200, "{\"ok\":true}");
}

void WebApiServer::handleStop() {
  Serial.printf("🛑 [UI] Button gedrückt: STOPP (Position: %.1f cm)\n", _currentCm);
  _manualUp = false;
  _manualDown = false;
  _posMgr.stop();
  sendJson(200, "{\"ok\":true}");
}

void WebApiServer::handleWifiGet() {
  String json = "{";
  json += "\"ssid\":\"" + escapeJson(_wifiMgr.ssid()) + "\",";
  json += "\"hasPassword\":" + String(_wifiMgr.hasPassword() ? "true" : "false") + ",";
  json += "\"connected\":" + String(_wifiMgr.isConnected() ? "true" : "false") + ",";
  json += "\"apMode\":" + String(_wifiMgr.isApMode() ? "true" : "false") + ",";
  json += "\"ip\":\"" + escapeJson(_wifiMgr.ipString()) + "\",";
  json += "\"rssi\":" + String(_wifiMgr.rssi());
  json += "}";
  sendJson(200, json);
}

void WebApiServer::handleWifiSave() {
  const String& body = _server.arg("plain");
  String ssid = extractJsonString(body, "ssid");
  String password = extractJsonString(body, "password");

  ssid.trim();
  if (ssid.length() == 0) {
    sendJson(400, "{\"error\":\"ssid required\"}");
    return;
  }

  Serial.printf("📶 [UI] Button gedrückt: WLAN SPEICHERN -> Neue SSID: \"%s\" (ESP32 startet neu...)\n", ssid.c_str());

  _wifiMgr.saveCredentials(ssid, password);
  sendJson(200, "{\"ok\":true,\"restarting\":true}");
  delay(200);
  ESP.restart();
}
