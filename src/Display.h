#pragma once
#include <TFT_eSPI.h>
#include <vector>

class Display
{
public:
    enum class EventKind
    {
        None,
        Preset,
        SaveToggle,
        Stop,
        ZeroOffset
    };
    struct UiEvent
    {
        EventKind kind{EventKind::None};
        int user{-1};
        int preset{-1};
    };

    Display();

    void init(bool showZeroButton = true);
    void setPersons(const std::vector<String> &names);
    void drawTable();
    void updateHeight(float cm);

    void calibrateTouch();
    bool pollTouch(UiEvent &ev);

    bool upHeld() const { return _heldUp; }
    bool downHeld() const { return _heldDown; }

    void setSaveArmed(bool armed);
    bool isSaveArmed() const { return _saveArmed; }
    void setActivePreset(int user, int preset, bool active);
    void clearActivePresets();

    void setPresetValue(int user, int preset, float cm);
    void renamePerson(int idx, const String &newName);

    void setHeaderAnchorX(int x); // rechter Anker der Einheit im Header

private:
    TFT_eSPI tft;
    TFT_eSprite _hdr{&tft};
    int headerH = 80, pad = 5;

    uint16_t COL_BG = 0x1084, COL_PANEL = 0x1A26, COL_CARD = 0x2432, COL_BORDER = 0x2E3B;
    uint16_t COL_TEXT = 0xE71C, COL_SUB = 0x9CF3, COL_ACC = 0x55B5, COL_ACCHI = 0x348F, COL_SHADOW = 0x0008;
    uint16_t COL_DANGER = 0xB004;

    int16_t ts_minx = 200, ts_maxx = 3900, ts_miny = 300, ts_maxy = 3700;

    std::vector<String> persons;
    struct Btn
    {
        int x, y, w, h;
        String label;
        int user;
        int preset;
    };
    std::vector<Btn> buttons;
    std::vector<bool> _sticky;
    int lastHighlighted = -1;

    // >>> NEU: Werte pro Preset-Button (Index wie in buttons)
    std::vector<float> _presetValues; // NAN = kein Wert

    float lastCmShown = NAN;
    uint32_t lastCmDrawMs = 0;

    bool _saveArmed = false;
    int _btnSave = -1, _btnStop = -1, _btnUp = -1, _btnDown = -1, _btnZero = -1;

    bool _heldUp = false, _heldDown = false;

    bool getCalibratedTouch(int16_t &x, int16_t &y);
    int pushButton(int x, int y, int w, int h, const String &label, int user, int preset);
    void paintButton(int idx, uint16_t fill, uint16_t border, uint16_t txt);
    int hitTest(int16_t x, int16_t y) const;

    // >>> NEU: kleine Anzeige im Button unten
    void drawPresetValueInButton(int idx);

    int _hdrAnchorX = -1; // rechter Anker für "cm" im Sprite
    int _hdrGap = 5;     // Abstand zwischen Zahl und Einheit

    bool showZero;
};
