// VL53L1X_Sensor.h
#pragma once
#include <Wire.h>
#include <Preferences.h>
#include "SparkFun_VL53L1X.h"

class VL53L1X_Sensor {
public:
    bool begin(bool debugInConsole = true);
    int   readDistanceMM();                // korrigierte Distanz in mm
    float readDistanceCM();               // korrigierte Distanz in cm
    void  setZeroCurrent(float currentCm);// Setzt aktuellen Messwert dauerhaft auf 0.0 cm
    void  setOffset(float offsetValueCm); // Offset in cm setzen
    float getOffset() const;              // Offset in cm zurückgeben
    void  resetOffset();                  // Offset auf 0 zurücksetzen

private:
    SFEVL53L1X sensor;
    bool dbg = false;

    // Offset intern in Millimetern
    int offsetMM = 0;

    static constexpr const char* NAMESPACE = "desk";
    static constexpr const char* KEY_OFF   = "offset_mm";

    void saveOffsetToNvs();
};
