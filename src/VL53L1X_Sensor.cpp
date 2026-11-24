// VL53L1X_Sensor.cpp
#include "VL53L1X_Sensor.h"
#include <cmath>

bool VL53L1X_Sensor::begin(bool debugInConsole) {
    dbg = debugInConsole;

    // NVS öffnen und gespeicherten Offset laden
    Preferences nvs;
    if (nvs.begin(NAMESPACE, /*readOnly*/ true)) {
        offsetMM = nvs.getInt(KEY_OFF, 0);
        nvs.end();
        Serial.printf("📍 Gespeicherter Sensor-Offset geladen: %d mm (%.1f cm)\n", offsetMM, offsetMM / 10.0f);
    }

    if (sensor.begin() != 0) {
        Serial.println("❌ VL53L1X init fehlgeschlagen!");
        return false;
    }

    // Kontinuierliche Messung starten
    sensor.startRanging();

    // Erste Messung abwarten (mit max. 200ms Timeout)
    uint32_t start = millis();
    while (!sensor.checkForDataReady() && (millis() - start < 200)) {
        delay(5);
    }
    if (sensor.checkForDataReady()) {
        _lastRawMM = sensor.getDistance();
        sensor.clearInterrupt();
    }

    Serial.printf("✅ VL53L1X Sensor initialisiert (Rohwert: %d mm, mit Offset: %.1f cm)\n",
                  _lastRawMM, (_lastRawMM + offsetMM) / 10.0f);
    return true;
}

int VL53L1X_Sensor::readDistanceMM() {
    // Non-blocking: nur lesen wenn neue Daten vom Sensor bereit sind
    if (sensor.checkForDataReady()) {
        int distance = sensor.getDistance(); // mm
        sensor.clearInterrupt();
        if (distance > 0) {
            _lastRawMM = distance;
        }

        if (dbg) {
            static uint32_t last = 0;
            uint32_t now = millis();
            if (now - last > 500) {
                Serial.printf("Distance raw=%d mm, off=%d mm, total=%d mm\n",
                              _lastRawMM, offsetMM, _lastRawMM + offsetMM);
                last = now;
            }
        }
    }

    // Offset (Totbereich / Montageabstand) wird ZUR Sensor-Distanz ADDIERT
    return _lastRawMM + offsetMM;
}

float VL53L1X_Sensor::readDistanceCM() {
    return static_cast<float>(readDistanceMM()) / 10.0f;
}

void VL53L1X_Sensor::setOffset(float offsetValueCm) {
    offsetMM = static_cast<int>(lroundf(offsetValueCm * 10.0f));
    saveOffsetToNvs();
}

float VL53L1X_Sensor::getOffset() const {
    return offsetMM / 10.0f;  // in cm
}

void VL53L1X_Sensor::saveOffsetToNvs() {
    Preferences nvs;
    if (nvs.begin(NAMESPACE, /*readOnly*/ false)) {
        nvs.putInt(KEY_OFF, offsetMM);
        nvs.end();
        Serial.printf("💾 Sensor-Offset dauerhaft in NVS gespeichert: %d mm (%.1f cm)\n",
                      offsetMM, offsetMM / 10.0f);
    } else {
        Serial.println("❌ Fehler beim Öffnen von NVS für Sensor-Offset!");
    }
}
