// PositionManager.h
#pragma once
#include <Arduino.h>
#include <Preferences.h>

#ifndef PM_DEBUG
#define PM_DEBUG 1
#endif

class PositionManager {
public:
  enum class Mode { Idle, PresetUp, PresetDown, ManualUp, ManualDown, Stopped };

  struct Event {
    enum class Type { PresetSaved, MoveStarted, MoveFinished, Stopped } type;
    uint8_t user{0}, preset{0};
    float   target{NAN};
  };
  using EventCB = void (*)(const Event&);

  // pinUp/pinDown: GPIOs für Richtung. activeHigh: true => HIGH=ein, false => LOW=ein
  PositionManager(int pinUp, int pinDown, bool activeHigh, float hysteresis_cm);

  void begin();
  void update(float current_cm);

  // Presets (2 Users x 3 Presets)
  void  setPreset(uint8_t user, uint8_t preset, float cm);
  float getPreset(uint8_t user, uint8_t preset) const;

  // Kommandos
  void commandGoTo(uint8_t user, uint8_t preset); // fahre zu gespeicherter Höhe
  void manualHold(bool upHeld, bool downHeld);    // manuell nur während gehalten
  void stop();                                    // Not-Stopp

  // Status
  Mode  mode() const { return _mode; }
  bool  hasTarget() const { return _hasTarget; }
  float targetCm() const { return _targetCm; }

  // Events (für Logging/Callbacks)
  void setCallback(EventCB cb) { _cb = cb; }

private:
  // Relaissteuerung
  void driveUp(bool on);
  void driveDown(bool on);
  void driveNone();
  void debugPins(const char* tag);
  void emit(Event::Type t, uint8_t user=0, uint8_t preset=0, float tgt=NAN);

  int   _pinUp, _pinDown;
  bool  _activeHigh;
  float _hyst;

  float _presets[2][3] = { {180.0f,170.0f,160.0f}, {180.0f,170.0f,160.0f} };

  // Zielverwaltung
  bool  _hasTarget = false;
  float _targetCm  = NAN;
  uint8_t _tUser = 0, _tPreset = 0;

  // Modus
  Mode  _mode = Mode::Idle;

  // Relais-Statecache (logischer “an/aus”)
  bool _upOn  = false;
  bool _dnOn  = false;

  // Manuell-Flags (vom UI)
  bool _holdUp = false;
  bool _holdDown = false;

  EventCB _cb = nullptr;

  // Persistenz (ESP32 NVS)
  Preferences _nvs;
  bool _nvsInit = false;
  static constexpr const char* NAMESPACE = "desk";

  void loadPresetsFromFlash();
  void savePresetToFlash(uint8_t user, uint8_t preset, float cm);
};
