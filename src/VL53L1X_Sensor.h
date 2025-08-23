// VL53L1X_Sensor.h
#pragma once
#include <Wire.h>
#include <Preferences.h>
#include "SparkFun_VL53L1X.h"

class VL53L1X_Sensor {
public:
    bool begin(bool debugInConsole = true);
    int  readDistanceMM();                // korrigierte Distanz in mm
    void setOffset(float offsetValueCm);  // Offset in cm übergeben (z.B. cmFilt)
    float getOffset() const;              // Offset in cm zurückgeben

private:
    SFEVL53L1X sensor;
    bool dbg = false;

    // Offset wird intern in Millimetern gehalten (für korrekte Subtraktion)
    int offsetMM = 0;

    // Persistenz (ESP32 NVS)
    Preferences _nvs;
    bool _nvsInit = false;
    static constexpr const char* NAMESPACE = "desk";
    static constexpr const char* KEY_OFF   = "offset_mm";
};
