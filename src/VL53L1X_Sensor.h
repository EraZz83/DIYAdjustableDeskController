// VL53L1X_Sensor.h
#pragma once
#include <Wire.h>
#include <Preferences.h>
#include "SparkFun_VL53L1X.h"

class VL53L1X_Sensor {
public:
    bool begin(bool debugInConsole = true);
    int   readDistanceMM();                // korrigierte Distanz (Rohwert + Offset) in mm (non-blocking)
    float readDistanceCM();               // korrigierte Distanz in cm
    void  setOffset(float offsetValueCm); // Offset (z.B. Totbereich / Montagehöhe) in cm setzen & in NVS speichern
    float getOffset() const;              // Offset in cm zurückgeben

private:
    SFEVL53L1X sensor;
    bool dbg = false;

    // Letzter Rohwert in Millimetern für non-blocking Abfrage
    int _lastRawMM = 0;

    // Offset intern in Millimetern (wird zum Rohwert addiert)
    int offsetMM = 0;

    static constexpr const char* NAMESPACE = "desk";
    static constexpr const char* KEY_OFF   = "offset_mm";

    void saveOffsetToNvs();
};
