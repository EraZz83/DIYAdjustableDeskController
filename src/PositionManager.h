// src/PositionManager.h
#pragma once
#include <Arduino.h>
#include <Preferences.h>

class PositionManager {
public:
  enum class Mode {
    Idle,
    PresetUp,
    PresetDown,
    ManualUp,
    ManualDown,
    Stopped
  };

  static constexpr uint8_t NUM_PRESETS = 3;

  PositionManager(int pinRelayUp,
                  int pinRelayDown,
                  bool activeHigh = true,
                  float hysteresisCm = 0.5f);

  void begin();
  void update(float currentHeightCm);

  void commandGoTo(uint8_t preset);
  void manualHold(bool up, bool down);
  void stop();

  Mode mode() const { return _mode; }
  float targetCm() const { return _targetCm; }
  float hysteresisCm() const { return _hysteresisCm; }

  float getPreset(uint8_t preset) const;
  String getPresetName(uint8_t preset) const;
  void setPreset(uint8_t preset, float cm, const String& name = "");

private:
  int _pinUp;
  int _pinDown;
  bool _activeHigh;
  float _hysteresisCm;

  Mode _mode;
  float _targetCm;
  float _currentHeightCm;

  float _presetHeights[NUM_PRESETS];
  String _presetNames[NUM_PRESETS];

  Preferences _nvs;
  void _loadPresetsFromNvs();
  void _savePresetToNvs(uint8_t preset);
  void _setRelays(bool up, bool down);
};