// src/PositionManager.h
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <vector>

class PositionManager {
public:
  struct Preset {
    uint32_t id;
    String name;
    float heightCm;
  };

  enum class Mode {
    Idle,
    PresetUp,
    PresetDown,
    ManualUp,
    ManualDown,
    Stopped
  };

  PositionManager(int pinRelayUp,
                  int pinRelayDown,
                  bool activeHigh = true,
                  float hysteresisCm = 0.5f);

  void begin();
  void update(float currentHeightCm);

  void commandGoTo(uint32_t presetId);
  void commandGoToHeight(float targetCm);
  void manualHold(bool up, bool down);
  void stop();

  Mode mode() const { return _mode; }
  bool isMoving() const { return _mode == Mode::PresetUp || _mode == Mode::PresetDown; }
  float targetCm() const { return _targetCm; }
  float hysteresisCm() const { return _hysteresisCm; }

  const std::vector<Preset>& getPresets() const { return _presets; }
  bool addPreset(const String& name, float heightCm);
  bool deletePreset(uint32_t id);
  bool updatePreset(uint32_t id, const String& name, float heightCm);
  const Preset* findPreset(uint32_t id) const;

private:
  int _pinUp;
  int _pinDown;
  bool _activeHigh;
  float _hysteresisCm;

  Mode _mode;
  float _targetCm;
  float _currentHeightCm;

  std::vector<Preset> _presets;

  Preferences _nvs;
  void _loadPresetsFromNvs();
  void _savePresetsToNvs();
  void _setRelays(bool up, bool down);
};
