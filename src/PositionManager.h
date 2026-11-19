// PositionManager.h
#pragma once
#include <Arduino.h>
#include <Preferences.h>

#ifndef PM_DEBUG
#define PM_DEBUG 1
#endif

class PositionManager {
public:
  static constexpr uint8_t NUM_USERS   = 2;
  static constexpr uint8_t NUM_PRESETS = 3;

  enum class Mode { Idle, PresetUp, PresetDown, ManualUp, ManualDown, Stopped };

  struct Event {
    enum class Type { PresetSaved, MoveStarted, MoveFinished, Stopped } type;
    uint8_t user{0}, preset{0};
    float   target{NAN};
  };
  using EventCB = void (*)(const Event&);

  PositionManager(int pinUp, int pinDown, bool activeHigh, float hysteresis_cm);

  void begin();
  void update(float current_cm);

  void   setPreset(uint8_t user, uint8_t preset, float cm, const String& name = "");
  float  getPreset(uint8_t user, uint8_t preset) const;
  String getPresetName(uint8_t user, uint8_t preset) const;

  void commandGoTo(uint8_t user, uint8_t preset);
  void manualHold(bool upHeld, bool downHeld);
  void stop();

  Mode  mode() const { return _mode; }
  bool  hasTarget() const { return _hasTarget; }
  float targetCm() const { return _targetCm; }

  void setCallback(EventCB cb) { _cb = cb; }

private:
  void driveUp(bool on);
  void driveDown(bool on);
  void driveNone();
  void debugPins(const char* tag);
  void emit(Event::Type t, uint8_t user=0, uint8_t preset=0, float tgt=NAN);

  int   _pinUp, _pinDown;
  bool  _activeHigh;
  float _hyst;

  float  _presets[NUM_USERS][NUM_PRESETS]     = { {110.0f, 95.0f, 75.0f}, {110.0f, 95.0f, 75.0f} };
  String _presetNames[NUM_USERS][NUM_PRESETS] = { {"Stehen", "Sitzen", "Tief"}, {"Stehen", "Sitzen", "Tief"} };

  bool    _hasTarget = false;
  float   _targetCm  = NAN;
  uint8_t _tUser = 0, _tPreset = 0;

  Mode _mode     = Mode::Idle;
  bool _upOn     = false;
  bool _dnOn     = false;
  bool _holdUp   = false;
  bool _holdDown = false;

  EventCB _cb = nullptr;

  Preferences _nvs;
  bool _nvsInit = false;
  static constexpr const char* NAMESPACE = "desk";

  void loadPresetsFromFlash();
  void savePresetToFlash(uint8_t user, uint8_t preset, float cm, const String& name);
};