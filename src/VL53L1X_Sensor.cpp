// VL53L1X_Sensor.cpp
#include "VL53L1X_Sensor.h"
#include <cmath>

bool VL53L1X_Sensor::begin(bool debugInConsole) {
    dbg = debugInConsole;

    // NVS öffnen und gespeicherten Offset (Nullpunkt) laden
    Preferences nvs;
    if (nvs.begin(NAMESPACE, /*readOnly*/ true)) {
        offsetMM = nvs.getInt(KEY_OFF, 0);
        nvs.end();
        Serial.printf("📍 Gespeicherter Nullpunkt-Offset geladen: %d mm (%.1f cm)\n", offsetMM, offsetMM / 10.0f);
    }

    return sensor.begin() == 0;
}

int VL53L1X_Sensor::readDistanceMM() {
    sensor.startRanging();
    while (!sensor.checkForDataReady()) { delay(1); }
    int distance = sensor.getDistance();   // mm
    sensor.clearInterrupt();
    sensor.stopRanging();

    int corrected = distance - offsetMM;

    if (dbg) {
        static uint32_t last = 0;
        uint32_t now = millis();
        if (now - last > 500) { // Rate limit
            Serial.printf("Distance raw=%d mm, off=%d mm, corr=%d mm\n",
                          distance, offsetMM, corrected);
            last = now;
        }
    }
    return corrected;
}

float VL53L1X_Sensor::readDistanceCM() {
    return static_cast<float>(readDistanceMM()) / 10.0f;
}

void VL53L1X_Sensor::setZeroCurrent(float currentCm) {
    // Da currentCm = (raw - offsetMM) / 10.0f ist,
    // führt offsetMM += currentCm * 10 dazu, dass der neue Abstand genau 0.0 cm ist.
    offsetMM += static_cast<int>(lroundf(currentCm * 10.0f));
    saveOffsetToNvs();
}

void VL53L1X_Sensor::setOffset(float offsetValueCm) {
    offsetMM = static_cast<int>(lroundf(offsetValueCm * 10.0f));
    saveOffsetToNvs();
}

float VL53L1X_Sensor::getOffset() const {
    return offsetMM / 10.0f;  // in cm
}

void VL53L1X_Sensor::resetOffset() {
    offsetMM = 0;
    saveOffsetToNvs();
}

void VL53L1X_Sensor::saveOffsetToNvs() {
    Preferences nvs;
    if (nvs.begin(NAMESPACE, /*readOnly*/ false)) {
        nvs.putInt(KEY_OFF, offsetMM);
        nvs.end();
        Serial.printf("💾 Nullpunkt-Offset dauerhaft in NVS gespeichert: %d mm (%.1f cm)\n",
                      offsetMM, offsetMM / 10.0f);
    } else {
        Serial.println("❌ Fehler beim Öffnen von NVS für Nullpunkt!");
    }
}
