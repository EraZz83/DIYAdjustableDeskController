#include "Display.h"
#include <math.h>

Display::Display() : tft() {}

void Display::init(bool showZeroButton)
{
    showZero = showZeroButton;

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(COL_BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);

    _hdr.setColorDepth(16);
    _hdr.createSprite(tft.width() - 2 * pad, headerH - pad / 2);

    // Standard: Einheit 10px vom rechten Header-Rand
    _hdrAnchorX = _hdr.width() - 10;
}

void Display::setHeaderAnchorX(int x)
{
    // Clamp in Spritebreite
    if (_hdr.width() == 0)
        return;
    if (x < 0)
        x = 0;
    if (x > _hdr.width() - 1)
        x = _hdr.width() - 1;
    _hdrAnchorX = x;
}

void Display::setPersons(const std::vector<String> &names) { persons = names; }

void Display::drawTable()
{
    tft.fillScreen(COL_BG);
    tft.fillRoundRect(pad, pad, tft.width() - 2 * pad, headerH - 10 * pad / 2, 10, COL_PANEL);

    buttons.clear();
    _sticky.clear();
    _presetValues.clear();
    lastCmShown = NAN;
    lastCmDrawMs = 0;
    _btnSave = _btnStop = _btnUp = _btnDown = _btnZero = -1; // << erweitert
    _heldUp = _heldDown = false;

    int rows = persons.size();
    if (rows < 2)
        rows = 2;

    const int presetBarH = 46;
    const int bottomBarH = 60;
    const int rowH = (tft.height() - headerH - 3 * pad - bottomBarH) / rows;

    const int left = 120;
    const int gap = 10;
    const int usableW = tft.width() - left - pad;

    //  Preset-Bar (schmal)
    const int bwPreset = (usableW - 2 * gap) / 3;

    // Bottom-Bar Breite (groß)
    const int bwBottom = (tft.width() - 2 * pad - 3 * gap) / 4;
    const int bhBottom = bottomBarH;

    // Preset-Zeilen (Presets jetzt groß)
    for (int r = 0; r < rows; r++)
    {
        int y = headerH + r * rowH;
        if (r > 0)
            tft.drawLine(pad, y, tft.width() - pad, y, COL_BORDER);

        tft.setTextColor(COL_SUB, COL_BG);
        tft.setTextDatum(ML_DATUM);
        String nm = (r < (int)persons.size()) ? persons[r] : String("User ") + String(r + 1);
        tft.drawString(nm, pad + 4, y + rowH / 2, 2);

        const int totalW = 3 * bwBottom + 2 * gap;
        int x = left + (usableW - totalW) / 2;
        const int by = y + (rowH - presetBarH) / 2;

        for (int i = 0; i < 3; i++)
        {
            int idx = pushButton(x, by, bwBottom, presetBarH, String(i + 1), r, i);
            _presetValues.push_back(NAN);
            drawPresetValueInButton(idx);
            x += bwBottom + gap;
        }
    }

    // Bottom-Bar
    const int yb = tft.height() - (pad + bottomBarH);
    int x = pad / 2;

    // SAVE
    _btnSave = pushButton(x, yb, bwBottom, bhBottom, "Save", -1, -1);

    if (showZero)
    {
        // ZERO OFFSET (kleiner Button OBEN DRÜBER, gleiche X, halbe Höhe)
        const int zeroH = (bhBottom - gap) / 2; // kompakt
        const int zeroY = yb - gap - zeroH;     // direkt über Save
        _btnZero = pushButton(x, zeroY, bwBottom / 2, zeroH, "Zero", -1, -1);
    }

    x += bwBottom + gap;

    // STOP (groß)
    _btnStop = pushButton(x, yb, bwBottom, bhBottom, "Stop", -1, -1);
    x += bwBottom + gap;

    // UP/DOWN (groß)
    _btnUp = pushButton(x, yb, bwPreset, bhBottom, "Up", -1, -1);
    x += bwPreset + gap;
    _btnDown = pushButton(x, yb, bwPreset, bhBottom, "Down", -1, -1);

    // Farben
    paintButton(_btnSave, COL_CARD, COL_BORDER, COL_TEXT);
    paintButton(_btnZero, COL_ACCHI, COL_BORDER, COL_TEXT); // Zero optisch hervorgehoben
    paintButton(_btnStop, COL_DANGER, COL_BORDER, COL_TEXT);
    paintButton(_btnUp, COL_CARD, COL_BORDER, COL_TEXT);
    paintButton(_btnDown, COL_CARD, COL_BORDER, COL_TEXT);

    _sticky.assign(buttons.size(), false);
}

void Display::updateHeight(float cm)
{
    const uint32_t now = millis();
    const uint32_t minGap = 120;
    const float eps = 0.1f;
    if (!isnan(lastCmShown))
    {
        if (fabsf(cm - lastCmShown) < eps && (now - lastCmDrawMs) < minGap)
            return;
    }
    lastCmShown = cm;
    lastCmDrawMs = now;

    // Strings
    char buf[16];
    dtostrf(cm, 0, 1, buf); // z.B. "123.4"
    const String sNum(buf);
    const String sUnit("cm");

    // Sprite-Init falls noch nicht existiert
    if (_hdr.width() == 0 || _hdr.height() == 0)
    {
        _hdr.setColorDepth(16);
        _hdr.createSprite(tft.width() - 2 * pad, headerH - pad / 2);
    }
    if (_hdrAnchorX < 0)
        _hdrAnchorX = _hdr.width() - 10;

    // --- Offscreen zeichnen ---
    _hdr.fillSprite(COL_PANEL);

    const int cy = _hdr.height() / 2;

    // Breite der Einheit ermitteln
    _hdr.setTextFont(4);
    int wUnit = _hdr.textWidth(sUnit);

    // Einheit: immer rechts am Anchor
    _hdr.setTextDatum(MR_DATUM);
    _hdr.setTextColor(COL_TEXT, COL_PANEL);
    int xUnitRight = _hdrAnchorX;
    _hdr.drawString(sUnit, xUnitRight, cy);

    // Wert: rechtsbündig direkt links neben der Einheit + Abstand
    _hdr.setTextFont(7);
    _hdr.setTextDatum(MR_DATUM);
    _hdr.setTextColor(COL_ACC, COL_PANEL);
    int xValRight = xUnitRight - wUnit - _hdrGap; // Zahl berücksichtigt Einheitbreite
    _hdr.drawString(sNum, xValRight, cy);

    // Sprite -> Display
    _hdr.pushSprite(pad, pad);
}

void Display::calibrateTouch()
{
    tft.fillScreen(COL_BG);
    tft.drawCircle(20, 20, 10, COL_ACC);
    uint16_t rx, ry;
    while (!tft.getTouch(&rx, &ry))
    {
        delay(10);
    }
    ts_minx = rx;
    ts_miny = ry;
    delay(300);

    tft.fillScreen(COL_BG);
    tft.drawCircle(tft.width() - 20, tft.height() - 20, 10, COL_ACC);
    while (!tft.getTouch(&rx, &ry))
    {
        delay(10);
    }
    ts_maxx = rx;
    ts_maxy = ry;
    delay(200);
}

bool Display::getCalibratedTouch(int16_t &x, int16_t &y)
{
    uint16_t rx, ry;
    if (tft.getTouch(&rx, &ry))
    {
        x = map(rx, ts_minx, ts_maxx, 0, tft.width());
        y = map(ry, ts_miny, ts_maxy, 0, tft.height());
        return true;
    }
    return false;
}

int Display::pushButton(int x, int y, int w, int h, const String &label, int user, int preset)
{
    tft.fillRoundRect(x + 1, y + 2, w, h, 8, COL_SHADOW);
    tft.fillRoundRect(x, y, w, h, 8, COL_CARD);
    tft.drawRoundRect(x, y, w, h, 8, COL_BORDER);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_CARD);
    tft.drawString(label, x + w / 2, y + h / 2 - 6, 2); // leicht höher, Platz für Wert unten

    buttons.push_back({x, y, w, h, label, user, preset});
    return (int)buttons.size() - 1;
}

void Display::paintButton(int idx, uint16_t fill, uint16_t border, uint16_t txt)
{
    if (idx < 0 || idx >= (int)buttons.size())
        return;
    const auto &b = buttons[idx];
    tft.fillRoundRect(b.x + 1, b.y + 2, b.w, b.h, 8, COL_SHADOW);
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, fill);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(txt, fill);
    tft.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2 - 6, 2);

    // Wert unten neu zeichnen, falls Preset-Button
    drawPresetValueInButton(idx); // <<< NEU
}

int Display::hitTest(int16_t x, int16_t y) const
{
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const auto &b = buttons[i];
        if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h)
            return (int)i;
    }
    return -1;
}

void Display::setSaveArmed(bool armed)
{
    _saveArmed = armed;
    if (_btnSave >= 0)
        paintButton(_btnSave, armed ? COL_ACCHI : COL_CARD, COL_BORDER, COL_TEXT);
}

bool Display::pollTouch(UiEvent &ev)
{
    ev = {};
    int16_t x, y;
    if (!getCalibratedTouch(x, y))
    {
        _heldUp = _heldDown = false;
        lastHighlighted = -1;
        return false;
    }

    int idx = hitTest(x, y);
    if (idx < 0)
    {
        _heldUp = _heldDown = false;
        return false;
    }

    _heldUp = (idx == _btnUp);
    _heldDown = (idx == _btnDown);

    if (idx != lastHighlighted)
    {
        lastHighlighted = idx;
        paintButton(idx, COL_ACCHI, COL_BORDER, COL_TEXT);
        delay(60);
        bool sticky = (_sticky.size() == buttons.size() ? _sticky[idx] : false);
        if (idx == _btnStop)
            paintButton(idx, COL_DANGER, COL_BORDER, COL_TEXT);
        else if (idx == _btnSave)
            paintButton(idx, _saveArmed ? COL_ACCHI : COL_CARD, COL_BORDER, COL_TEXT);
        else if (idx == _btnZero)
            paintButton(idx, COL_ACCHI, COL_BORDER, COL_TEXT); // << NEU
        else
            paintButton(idx, sticky ? COL_ACCHI : COL_CARD, COL_BORDER, COL_TEXT);
    }

    if (idx == _btnZero)
    {
        ev.kind = EventKind::ZeroOffset;
        return true;
    } // << NEU
    if (idx == _btnSave)
    {
        setSaveArmed(!_saveArmed);
        ev.kind = EventKind::SaveToggle;
        return true;
    }
    if (idx == _btnStop)
    {
        ev.kind = EventKind::Stop;
        return true;
    }

    const auto &b = buttons[idx];
    if (b.user >= 0 && b.preset >= 0)
    {
        ev.kind = EventKind::Preset;
        ev.user = b.user;
        ev.preset = b.preset;
        return true;
    }
    return false;
}

void Display::clearActivePresets()
{
    _sticky.assign(buttons.size(), false);
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const auto &b = buttons[i];
        bool isStop = ((int)i == _btnStop);
        bool isSave = ((int)i == _btnSave);
        uint16_t fill = isStop ? COL_DANGER : (_sticky[i] ? COL_ACCHI : COL_CARD);
        if (isSave)
            fill = _saveArmed ? COL_ACCHI : COL_CARD;
        paintButton((int)i, fill, COL_BORDER, COL_TEXT);
    }
}

void Display::setActivePreset(int user, int preset, bool active)
{
    if (_sticky.size() != buttons.size())
        _sticky.assign(buttons.size(), false);
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const auto &b = buttons[i];
        if (b.user == user && b.preset == preset)
        {
            _sticky[i] = active;
            uint16_t fill = active ? COL_ACCHI : COL_CARD;
            paintButton((int)i, fill, COL_BORDER, COL_TEXT);
            break;
        }
    }
}

// >>> NEU: öffentliches Setzen eines Preset-Werts
void Display::setPresetValue(int user, int preset, float cm)
{
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const auto &b = buttons[i];
        if (b.user == user && b.preset == preset)
        {
            if (_presetValues.size() != buttons.size())
                _presetValues.assign(buttons.size(), NAN);
            _presetValues[i] = cm;
            drawPresetValueInButton((int)i);
            break;
        }
    }
}

// >>> NEU: kleine Anzeige am unteren Rand des Buttons
void Display::drawPresetValueInButton(int idx)
{
    if (idx < 0 || idx >= (int)buttons.size())
        return;
    const auto &b = buttons[idx];
    if (b.user < 0 || b.preset < 0)
        return; // nur für echte Preset-Buttons

    if (_presetValues.size() != buttons.size())
        _presetValues.assign(buttons.size(), NAN);
    float v = _presetValues[idx];

    // Bereich unten als „Strip“
    int padIn = 6;
    int stripH = 18;
    int y0 = b.y + b.h - padIn - stripH;
    int x0 = b.x + padIn;
    int w = b.w - 2 * padIn;

    // Hintergrund passend zum Button (aktiv/nicht aktiv) erneut zeichnen:
    bool sticky = (_sticky.size() == buttons.size() ? _sticky[idx] : false);
    uint16_t fill = sticky ? COL_ACCHI : COL_CARD;
    tft.fillRect(x0, y0, w, stripH, fill);

    // Text
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_SUB, fill);
    String txt = isnan(v) ? String("--") : String(v, 1);
    tft.drawString(txt, x0 + w / 2, y0 + stripH / 2 + 1, 2);
}

void Display::renamePerson(int idx, const String &newName)
{
    if (idx < 0 || idx >= (int)persons.size())
        return;
    persons[idx] = newName;

    // Bereich des Namens neu zeichnen
    int rows = persons.size();
    if (rows < 2)
        rows = 2;
    int bottomBarH = 46;
    int rowH = (tft.height() - headerH - 3 * pad - bottomBarH) / rows;
    int y = headerH + pad + idx * rowH;

    tft.fillRect(pad, y, 110, rowH, COL_BG); // alten Namen übermalen
    tft.setTextColor(COL_SUB, COL_BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(newName, pad + 4, y + rowH / 2, 2);
}
