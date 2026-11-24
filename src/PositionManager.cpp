// src/PositionManager.cpp
#include "PositionManager.h"
#include <cmath>

PositionManager::PositionManager(int pinRelayUp,
                                 int pinRelayDown,
                                 bool activeHigh,
                                 float hysteresisCm)
  : _pinUp(pinRelayUp),
    _pinDown(pinRelayDown),
    _activeHigh(activeHigh),
    _hysteresisCm(hysteresisCm),
    _mode(Mode::Idle),
    _targetCm(NAN),
    _currentHeightCm(0.0f) {}

void PositionManager::begin() {
  pinMode(_pinUp, OUTPUT);
  pinMode(_pinDown, OUTPUT);
  _setRelays(false, false);
  _loadLimitsFromNvs();
  _loadPresetsFromNvs();
}

void PositionManager::setLimits(float minCm, float maxCm) {
  if (minCm >= maxCm) {
    Serial.printf("❌ Ungültige Limiten: Min (%.1f) muss kleiner als Max (%.1f) sein!\n", minCm, maxCm);
    return;
  }
  _minLimitCm = minCm;
  _maxLimitCm = maxCm;
  _saveLimitsToNvs();
  Serial.printf("💾 Tisch-Limiten gespeichert: Min = %.1f cm, Max = %.1f cm\n", _minLimitCm, _maxLimitCm);
}

void PositionManager::_loadLimitsFromNvs() {
  Preferences nvs;
  if (nvs.begin("desk", true)) {
    _minLimitCm = nvs.getFloat("lim_min", 50.0f);
    _maxLimitCm = nvs.getFloat("lim_max", 130.0f);
    nvs.end();
    Serial.printf("📍 Geladene Tisch-Limiten: Min = %.1f cm, Max = %.1f cm\n", _minLimitCm, _maxLimitCm);
  }
}

void PositionManager::_saveLimitsToNvs() {
  Preferences nvs;
  if (nvs.begin("desk", false)) {
    nvs.putFloat("lim_min", _minLimitCm);
    nvs.putFloat("lim_max", _maxLimitCm);
    nvs.end();
  }
}

void PositionManager::update(float currentHeightCm) {
  _currentHeightCm = currentHeightCm;

  if (std::isnan(_currentHeightCm)) {
    stop();
    return;
  }

  // Sicherheitsstopp an den konfigurierten Grenzwerten
  if ((_mode == Mode::PresetUp || _mode == Mode::ManualUp) && (_currentHeightCm >= _maxLimitCm)) {
    Serial.printf("⛔ [Limit] Oberes Limit (%.1f cm) erreicht! Ist: %.1f cm -> Stopp\n", _maxLimitCm, _currentHeightCm);
    stop();
    return;
  }
  if ((_mode == Mode::PresetDown || _mode == Mode::ManualDown) && (_currentHeightCm <= _minLimitCm)) {
    Serial.printf("⛔ [Limit] Unteres Limit (%.1f cm) erreicht! Ist: %.1f cm -> Stopp\n", _minLimitCm, _currentHeightCm);
    stop();
    return;
  }

  if (_mode == Mode::PresetUp) {
    if (_currentHeightCm >= (_targetCm - (_hysteresisCm / 2.0f))) {
      Serial.printf("✅ [Fahrt] Zielhöhe erreicht: %.1f cm (Ist: %.1f cm)\n", _targetCm, _currentHeightCm);
      stop();
    } else {
      _setRelays(true, false);
    }
  } else if (_mode == Mode::PresetDown) {
    if (_currentHeightCm <= (_targetCm + (_hysteresisCm / 2.0f))) {
      Serial.printf("✅ [Fahrt] Zielhöhe erreicht: %.1f cm (Ist: %.1f cm)\n", _targetCm, _currentHeightCm);
      stop();
    } else {
      _setRelays(false, true);
    }
  }
}

void PositionManager::commandGoTo(uint32_t presetId) {
  for (const auto& p : _presets) {
    if (p.id == presetId) {
      commandGoToHeight(p.heightCm);
      return;
    }
  }
}

void PositionManager::commandGoToHeight(float target) {
  if (std::isnan(target) || target < 0.0f) return;

  // Begrenzung auf obere und untere Limite
  if (target > _maxLimitCm) {
    Serial.printf("⚠️ Sollwert %.1f cm überschreitet oberes Limit (%.1f cm) -> auf %.1f cm begrenzt\n",
                  target, _maxLimitCm, _maxLimitCm);
    target = _maxLimitCm;
  } else if (target < _minLimitCm) {
    Serial.printf("⚠️ Sollwert %.1f cm unterschreitet unteres Limit (%.1f cm) -> auf %.1f cm begrenzt\n",
                  target, _minLimitCm, _minLimitCm);
    target = _minLimitCm;
  }

  if (std::fabs(_currentHeightCm - target) <= (_hysteresisCm / 2.0f)) {
    Serial.printf("ℹ️ [Fahrt] Zielhöhe %.1f cm bereits erreicht (Ist: %.1f cm)\n", target, _currentHeightCm);
    stop();
    return;
  }

  _targetCm = target;
  if (_targetCm > _currentHeightCm) {
    _mode = Mode::PresetUp;
    Serial.printf("🔼 [Fahrt] Starte Aufwärtsfahrt: %.1f cm -> %.1f cm (Limit: %.1f..%.1f cm)\n",
                  _currentHeightCm, _targetCm, _minLimitCm, _maxLimitCm);
  } else {
    _mode = Mode::PresetDown;
    Serial.printf("🔽 [Fahrt] Starte Abwärtsfahrt: %.1f cm -> %.1f cm (Limit: %.1f..%.1f cm)\n",
                  _currentHeightCm, _targetCm, _minLimitCm, _maxLimitCm);
  }
}

void PositionManager::manualHold(bool up, bool down) {
  if (up && down) {
    stop();
    return;
  }

  if (up) {
    if (_currentHeightCm >= _maxLimitCm) {
      Serial.printf("⛔ [Manuell] Oberes Limit (%.1f cm) erreicht, keine Aufwärtsfahrt möglich\n", _maxLimitCm);
      stop();
      return;
    }
    _mode = Mode::ManualUp;
    _setRelays(true, false);
  } else if (down) {
    if (_currentHeightCm <= _minLimitCm) {
      Serial.printf("⛔ [Manuell] Unteres Limit (%.1f cm) erreicht, keine Abwärtsfahrt möglich\n", _minLimitCm);
      stop();
      return;
    }
    _mode = Mode::ManualDown;
    _setRelays(false, true);
  } else {
    stop();
  }
}

void PositionManager::stop() {
  if (_mode != Mode::Idle) {
    Serial.println("🛑 [Fahrt] Gestoppt / Relais AUS");
  }
  _mode = Mode::Idle;
  _targetCm = NAN;
  _setRelays(false, false);
}

const PositionManager::Preset* PositionManager::findPreset(uint32_t id) const {
  for (const auto& p : _presets) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

bool PositionManager::addPreset(const String& name, float heightCm) {
  uint32_t newId = 1;
  for (const auto& p : _presets) {
    if (p.id >= newId) {
      newId = p.id + 1;
    }
  }

  String pName = name;
  pName.trim();
  if (pName.length() == 0) {
    pName = "Preset " + String(newId);
  }

  // Auch Preset-Werte auf die Limiten begrenzen
  if (heightCm > _maxLimitCm) heightCm = _maxLimitCm;
  if (heightCm < _minLimitCm) heightCm = _minLimitCm;

  _presets.push_back({newId, pName, heightCm});
  _savePresetsToNvs();
  return true;
}

bool PositionManager::deletePreset(uint32_t id) {
  for (auto it = _presets.begin(); it != _presets.end(); ++it) {
    if (it->id == id) {
      _presets.erase(it);
      _savePresetsToNvs();
      return true;
    }
  }
  return false;
}

bool PositionManager::updatePreset(uint32_t id, const String& name, float heightCm) {
  for (auto& p : _presets) {
    if (p.id == id) {
      if (name.length() > 0) p.name = name;
      if (heightCm > _maxLimitCm) heightCm = _maxLimitCm;
      if (heightCm < _minLimitCm) heightCm = _minLimitCm;
      p.heightCm = heightCm;
      _savePresetsToNvs();
      return true;
    }
  }
  return false;
}

void PositionManager::_loadPresetsFromNvs() {
  _presets.clear();
  if (!_nvs.begin("presets", false)) return;

  if (_nvs.isKey("count")) {
    uint32_t count = _nvs.getUInt("count", 0);
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t id = _nvs.getUInt(("id_" + String(i)).c_str(), i + 1);
      String name = _nvs.getString(("n_" + String(i)).c_str(), "Preset " + String(i + 1));
      float h = _nvs.getFloat(("h_" + String(i)).c_str(), 0.0f);
      _presets.push_back({id, name, h});
    }
    _nvs.end();
  } else {
    // Migration: Falls noch alte h_0, h_1, h_2 Schlüssel vorliegen
    float h0 = _nvs.getFloat("h_0", 112.0f);
    String n0 = _nvs.getString("n_0", "Stehen");
    if (n0.length() == 0) n0 = "Stehen";

    float h1 = _nvs.getFloat("h_1", 74.0f);
    String n1 = _nvs.getString("n_1", "Sitzen");
    if (n1.length() == 0) n1 = "Sitzen";

    float h2 = _nvs.getFloat("h_2", 65.0f);
    String n2 = _nvs.getString("n_2", "Niedrig");
    if (n2.length() == 0) n2 = "Niedrig";

    _presets.push_back({1, n0, h0});
    _presets.push_back({2, n1, h1});
    _presets.push_back({3, n2, h2});

    _nvs.end();
    _savePresetsToNvs();
  }
}

void PositionManager::_savePresetsToNvs() {
  if (!_nvs.begin("presets", false)) return;
  _nvs.clear();
  _nvs.putUInt("count", static_cast<uint32_t>(_presets.size()));
  for (size_t i = 0; i < _presets.size(); ++i) {
    _nvs.putUInt(("id_" + String(i)).c_str(), _presets[i].id);
    _nvs.putString(("n_" + String(i)).c_str(), _presets[i].name);
    _nvs.putFloat(("h_" + String(i)).c_str(), _presets[i].heightCm);
  }
  _nvs.end();
}

void PositionManager::_setRelays(bool up, bool down) {
  static bool lastUp = false;
  static bool lastDown = false;

  if (up != lastUp || down != lastDown) {
    lastUp = up;
    lastDown = down;
    const int upLevel = _activeHigh ? (up ? HIGH : LOW) : (up ? LOW : HIGH);
    const int downLevel = _activeHigh ? (down ? HIGH : LOW) : (down ? LOW : HIGH);
    Serial.printf("⚡ [Relais] UP(GPIO %d)=%s [Pin=%s] | DOWN(GPIO %d)=%s [Pin=%s] (Polarität: %s)\n",
                  _pinUp, up ? "EIN" : "AUS", upLevel == HIGH ? "HIGH" : "LOW",
                  _pinDown, down ? "EIN" : "AUS", downLevel == HIGH ? "HIGH" : "LOW",
                  _activeHigh ? "Active-HIGH" : "Active-LOW");
  }

  if (_activeHigh) {
    digitalWrite(_pinUp, up ? HIGH : LOW);
    digitalWrite(_pinDown, down ? HIGH : LOW);
  } else {
    digitalWrite(_pinUp, up ? LOW : HIGH);
    digitalWrite(_pinDown, down ? LOW : HIGH);
  }
}
