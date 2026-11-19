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
  _loadPresetsFromNvs();
}

void PositionManager::update(float currentHeightCm) {
  _currentHeightCm = currentHeightCm;

  if (std::isnan(_currentHeightCm)) {
    stop();
    return;
  }

  if (_mode == Mode::PresetUp) {
    if (_currentHeightCm >= (_targetCm - (_hysteresisCm / 2.0f))) {
      stop();
    } else {
      _setRelays(true, false);
    }
  } else if (_mode == Mode::PresetDown) {
    if (_currentHeightCm <= (_targetCm + (_hysteresisCm / 2.0f))) {
      stop();
    } else {
      _setRelays(false, true);
    }
  }
}

void PositionManager::commandGoTo(uint8_t preset) {
  if (preset >= NUM_PRESETS) return;
  const float target = _presetHeights[preset];

  if (std::isnan(target) || target <= 0.0f) return;

  if (std::fabs(_currentHeightCm - target) <= (_hysteresisCm / 2.0f)) {
    stop();
    return;
  }

  _targetCm = target;
  if (_targetCm > _currentHeightCm) {
    _mode = Mode::PresetUp;
  } else {
    _mode = Mode::PresetDown;
  }
}

void PositionManager::manualHold(bool up, bool down) {
  if (up && down) {
    stop();
    return;
  }

  if (up) {
    _mode = Mode::ManualUp;
    _setRelays(true, false);
  } else if (down) {
    _mode = Mode::ManualDown;
    _setRelays(false, true);
  } else {
    stop();
  }
}

void PositionManager::stop() {
  _mode = Mode::Idle;
  _targetCm = NAN;
  _setRelays(false, false);
}

float PositionManager::getPreset(uint8_t preset) const {
  if (preset >= NUM_PRESETS) return 0.0f;
  return _presetHeights[preset];
}

String PositionManager::getPresetName(uint8_t preset) const {
  if (preset >= NUM_PRESETS) return "Error";
  if (_presetNames[preset].length() == 0) {
    return "Slot " + String(preset + 1);
  }
  return _presetNames[preset];
}

void PositionManager::setPreset(uint8_t preset, float cm, const String& name) {
  if (preset >= NUM_PRESETS) return;

  _presetHeights[preset] = cm;
  if (name.length() > 0) {
    _presetNames[preset] = name;
  }
  _savePresetToNvs(preset);
}

void PositionManager::_loadPresetsFromNvs() {
  if (!_nvs.begin("presets", false)) return;

  for (uint8_t i = 0; i < NUM_PRESETS; ++i) {
    const String keyCm = "h_" + String(i);
    const String keyName = "n_" + String(i);

    if (!_nvs.isKey(keyCm.c_str())) {
      _presetHeights[i] = (i == 0) ? 112.0f : (i == 1) ? 74.0f : 65.0f;
      _presetNames[i] = "";
      _savePresetToNvs(i);
    } else {
      _presetHeights[i] = _nvs.getFloat(keyCm.c_str());
      _presetNames[i] = _nvs.getString(keyName.c_str(), "");
    }
  }
  _nvs.end();
}

void PositionManager::_savePresetToNvs(uint8_t preset) {
  if (preset >= NUM_PRESETS) return;
  if (!_nvs.begin("presets", false)) return;

  _nvs.putFloat(("h_" + String(preset)).c_str(), _presetHeights[preset]);
  _nvs.putString(("n_" + String(preset)).c_str(), _presetNames[preset]);
  _nvs.end();
}

void PositionManager::_setRelays(bool up, bool down) {
  if (_activeHigh) {
    digitalWrite(_pinUp, up ? HIGH : LOW);
    digitalWrite(_pinDown, down ? HIGH : LOW);
  } else {
    digitalWrite(_pinUp, up ? LOW : HIGH);
    digitalWrite(_pinDown, down ? LOW : HIGH);
  }
}