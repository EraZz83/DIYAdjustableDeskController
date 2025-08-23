// VL53L1X_Sensor.cpp
#include "VL53L1X_Sensor.h"

bool VL53L1X_Sensor::begin(bool debugInConsole) {
    Wire.begin();
    dbg = debugInConsole;

    // NVS öffnen und Offset laden
    if (_nvs.begin(NAMESPACE, /*readOnly*/ false)) {
        _nvsInit = true;
        offsetMM = _nvs.getInt(KEY_OFF, 0);
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

void VL53L1X_Sensor::setOffset(float offsetValueCm) {
    // cm -> mm, runden
    offsetMM = (int)lroundf(offsetValueCm * 10.0f);
    if (_nvsInit) _nvs.putInt(KEY_OFF, offsetMM);
}

float VL53L1X_Sensor::getOffset() const {
    return offsetMM / 10.0f;  // in cm
}
