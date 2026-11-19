// PositionManager.cpp
#include "PositionManager.h"

PositionManager::PositionManager(int pinUp, int pinDown, bool activeHigh, float hysteresis_cm)
: _pinUp(pinUp), _pinDown(pinDown), _activeHigh(activeHigh), _hyst(hysteresis_cm) {}

void PositionManager::begin() {
  pinMode(_pinUp, OUTPUT);
  pinMode(_pinDown, OUTPUT);
  driveNone();
#if PM_DEBUG
  debugPins("begin");
#endif

  if (_nvs.begin(NAMESPACE, /*readOnly*/ false)) {
    _nvsInit = true;
    loadPresetsFromFlash();
  }
}

void PositionManager::debugPins(const char* tag) {
#if PM_DEBUG
  int rlUp = digitalRead(_pinUp);
  int rlDn = digitalRead(_pinDown);
  Serial.printf("[PM:%s] upPin=%d(%d) dnPin=%d(%d) activeHigh=%d _upOn=%d _dnOn=%d\n",
    tag, _pinUp, rlUp, _pinDown, rlDn, (int)_activeHigh, (int)_upOn, (int)_dnOn);
#endif
}

void PositionManager::emit(Event::Type t, uint8_t user, uint8_t preset, float tgt) {
  if (_cb) { Event e; e.type=t; e.user=user; e.preset=preset; e.target=tgt; _cb(e); }
}

void PositionManager::driveUp(bool on) {
  if (on) { _dnOn = false; digitalWrite(_pinDown, _activeHigh ? LOW : HIGH); }
  _upOn = on;
  digitalWrite(_pinUp, _activeHigh ? (_upOn?HIGH:LOW) : (_upOn?LOW:HIGH));
#if PM_DEBUG
  debugPins(on ? "up_on" : "up_off");
#endif
}

void PositionManager::driveDown(bool on) {
  if (on) { _upOn = false; digitalWrite(_pinUp, _activeHigh ? LOW : HIGH); }
  _dnOn = on;
  digitalWrite(_pinDown, _activeHigh ? (_dnOn?HIGH:LOW) : (_dnOn?LOW:HIGH));
#if PM_DEBUG
  debugPins(on ? "dn_on" : "dn_off");
#endif
}

void PositionManager::driveNone() {
  _upOn = _dnOn = false;
  digitalWrite(_pinUp,   _activeHigh ? LOW : HIGH);
  digitalWrite(_pinDown, _activeHigh ? LOW : HIGH);
#if PM_DEBUG
  debugPins("none");
#endif
  _mode = Mode::Idle;
}

void PositionManager::loadPresetsFromFlash() {
  if (!_nvsInit) return;
  for (uint8_t u = 0; u < NUM_USERS; ++u) {
    for (uint8_t p = 0; p < NUM_PRESETS; ++p) {
      char keyVal[8], keyName[8];
      snprintf(keyVal,  sizeof(keyVal),  "p%u%u", u, p);
      snprintf(keyName, sizeof(keyName), "n%u%u", u, p);
      _presets[u][p]     = _nvs.getFloat(keyVal, _presets[u][p]);
      _presetNames[u][p] = _nvs.getString(keyName, _presetNames[u][p]);
    }
  }
}

void PositionManager::savePresetToFlash(uint8_t user, uint8_t preset, float cm, const String& name) {
  if (!_nvsInit) return;
  char keyVal[8], keyName[8];
  snprintf(keyVal,  sizeof(keyVal),  "p%u%u", user, preset);
  snprintf(keyName, sizeof(keyName), "n%u%u", user, preset);
  _nvs.putFloat(keyVal, cm);
  _nvs.putString(keyName, name);
}

void PositionManager::setPreset(uint8_t user, uint8_t preset, float cm, const String& name) {
  if (user < NUM_USERS && preset < NUM_PRESETS) {
    _presets[user][preset] = cm;
    if (name.length() > 0) {
      String clean = name;
      clean.trim();
      if (clean.length() > 24) clean = clean.substring(0, 24);
      if (clean.length() > 0) _presetNames[user][preset] = clean;
    }
    savePresetToFlash(user, preset, _presets[user][preset], _presetNames[user][preset]);
    emit(Event::Type::PresetSaved, user, preset, cm);
  }
}

float PositionManager::getPreset(uint8_t user, uint8_t preset) const {
  if (user < NUM_USERS && preset < NUM_PRESETS) return _presets[user][preset];
  return NAN;
}

String PositionManager::getPresetName(uint8_t user, uint8_t preset) const {
  if (user < NUM_USERS && preset < NUM_PRESETS) return _presetNames[user][preset];
  return "";
}

void PositionManager::commandGoTo(uint8_t user, uint8_t preset) {
  float tgt = getPreset(user, preset);
  if (isnan(tgt)) return;
  _tUser     = user;
  _tPreset   = preset;
  _targetCm  = tgt;
  _hasTarget = true;
}

void PositionManager::manualHold(bool upHeld, bool downHeld) {
  _holdUp   = upHeld;
  _holdDown = downHeld;
  if (_holdUp && _holdDown) {
    driveNone();
    _mode = Mode::Stopped;
    emit(Event::Type::Stopped);
  } else if (_holdUp) {
    _hasTarget = false;
    _targetCm  = NAN;
    driveDown(false);
    driveUp(true);
    _mode = Mode::ManualUp;
  } else if (_holdDown) {
    _hasTarget = false;
    _targetCm  = NAN;
    driveUp(false);
    driveDown(true);
    _mode = Mode::ManualDown;
  } else {
    if (_mode == Mode::ManualUp || _mode == Mode::ManualDown) {
      driveNone();
    }
  }
}

void PositionManager::stop() {
  driveNone();
  _hasTarget = false;
  _targetCm  = NAN;
  _mode      = Mode::Stopped;
  emit(Event::Type::Stopped);
}

void PositionManager::update(float current_cm) {
  if (_mode == Mode::ManualUp || _mode == Mode::ManualDown) return;

  if (!_hasTarget) {
    if (_mode != Mode::Idle) driveNone();
    return;
  }

  float diff = _targetCm - current_cm;

  if (fabsf(diff) <= _hyst) {
    driveNone();
    _hasTarget = false;
    emit(Event::Type::MoveFinished, _tUser, _tPreset, _targetCm);
    _targetCm = NAN;
    return;
  }

  if (diff > 0) {
    if (_mode != Mode::PresetUp) {
      driveDown(false);
      driveUp(true);
      _mode = Mode::PresetUp;
      emit(Event::Type::MoveStarted, _tUser, _tPreset, _targetCm);
    }
    if (current_cm >= _targetCm) {
      driveNone();
      _hasTarget = false;
      emit(Event::Type::MoveFinished, _tUser, _tPreset, _targetCm);
      _targetCm = NAN;
    }
  } else {
    if (_mode != Mode::PresetDown) {
      driveUp(false);
      driveDown(true);
      _mode = Mode::PresetDown;
      emit(Event::Type::MoveStarted, _tUser, _tPreset, _targetCm);
    }
    if (current_cm <= _targetCm) {
      driveNone();
      _hasTarget = false;
      emit(Event::Type::MoveFinished, _tUser, _tPreset, _targetCm);
      _targetCm = NAN;
    }
  }
}