#include <Arduino.h>
#include <WiFi.h>

#include "WiFiManager.h"
#include "OTAHandler.h"
#include "Display.h"
#include "VL53L1X_Sensor.h"
#include "PositionManager.h"

// WLAN
const char *ssid = "TurkishAirlines_";
const char *password = "123Polizei";

// Motor-GPIOs (Relais/MOSFET)
constexpr int PIN_UP = 25;
constexpr int PIN_DOWN = 26;
// Relais-Polarität: true = HIGH schaltet EIN, false = LOW schaltet EIN
constexpr bool RELAYS_ACTIVE_HIGH = false;

int anchor = 345;

// Objekte
VL53L1X_Sensor distanceSensor;
Display display;
PositionManager posMgr(PIN_UP, PIN_DOWN, RELAYS_ACTIVE_HIGH, /*hysteresis_cm*/ 0.3f);

static bool hadTarget = false;

void setup()
{
    Serial.begin(115200);
    Serial.println("\n🚀 Starte Tischsteuerung...");

    // WLAN (STA -> Fallback AP)
    bool ok = connectToWiFi(ssid, password, 20);
    if (!ok)
    {
        Serial.println("⚠️ Fallback: Starte AP...");
        startAccessPoint("Tischsteuerung_AP", "12345678",
                         IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    }

    // OTA
    setupOTA("Tischsteuerung");

    // Display
    display.init(false);
    display.calibrateTouch();
    display.setPersons({"Beco", "Aybala"});
    display.drawTable();

    // Sensor
    if (!distanceSensor.begin(false))
    { // Debug off
        Serial.println("❌ VL53L1X init fehlgeschlagen!");
        while (1)
            delay(100);
    }

    // Positions-Manager
    posMgr.begin();

    // Presets beim Start direkt in UI eintragen
    for (int u = 0; u < 2; u++)
    {
        for (int p = 0; p < 3; p++)
        {
            float val = posMgr.getPreset(u, p);
            display.setPresetValue(u, p, val);
        }
    }

    // Events zentral loggen + UI updaten
    posMgr.setCallback([](const PositionManager::Event &e)
                       {
    switch (e.type) {
      case PositionManager::Event::Type::PresetSaved:
        Serial.printf("💾 Preset gespeichert: user=%u preset=%u -> %.1f cm\n", e.user, e.preset, e.target);
        display.setPresetValue(e.user, e.preset, e.target);   // UI-Beschriftung im Button aktualisieren
        break;
      case PositionManager::Event::Type::MoveStarted:
        Serial.printf("▶️  Fahrt gestartet zu %.1f cm\n", e.target);
        break;
      case PositionManager::Event::Type::MoveFinished:
        Serial.printf("✅ Ziel erreicht (%.1f cm)\n", e.target);
        break;
      case PositionManager::Event::Type::Stopped:
        Serial.println("⛔️ Stop");
        
        break;
    } });
}

void loop()
{
    handleOTA();

    // Höhe filtern (leichtes EMA)
    static bool filtInit = false;
    static float cmFilt = 0.0f;
    const float cmRaw = distanceSensor.readDistanceMM() / 10.0f;

    if (!filtInit)
    {
        cmFilt = cmRaw;
        filtInit = true;
    }
    else
    {
        cmFilt = 0.25f * cmRaw + 0.75f * cmFilt;
    }

    display.updateHeight(cmFilt);

    // Touch-Events (Tap)
    Display::UiEvent ev;
    if (display.pollTouch(ev))
    {
        switch (ev.kind)
        {
        case Display::EventKind::SaveToggle:
            break;

        case Display::EventKind::Stop:
            posMgr.stop();
            display.clearActivePresets();
            display.setSaveArmed(false);
            break;

        case Display::EventKind::Preset:
            if (display.isSaveArmed())
            {
                posMgr.setPreset(ev.user, ev.preset, cmFilt);
                display.setSaveArmed(false);
            }
            else
            {
                posMgr.commandGoTo(ev.user, ev.preset);
                display.clearActivePresets();
                display.setActivePreset(ev.user, ev.preset, true); // Sticky Highlight
            }
            break;

        case Display::EventKind::ZeroOffset:
            distanceSensor.setOffset(cmFilt); // aktuelle Höhe in cm speichern
            break;

        default:
            break;
        }
    }

    // Manuell fahren: nur solange gehalten
    posMgr.manualHold(display.upHeld(), display.downHeld());

    // Fahrlogik (inkl. Hysterese & Sicherheits-Stopp)
    posMgr.update(cmFilt);

    // Sticky-Highlight aus, wenn Ziel erreicht und wieder Idle
    if (posMgr.hasTarget())
    {
        hadTarget = true;
    }
    else if (hadTarget && posMgr.mode() == PositionManager::Mode::Idle)
    {
        display.clearActivePresets();
        hadTarget = false;
    }

    display.setHeaderAnchorX(anchor);

    delay(60);
}