#pragma once
#define DEBUG_

#include <IPAddress.h>

bool connectToWiFi(const char *ssid, const char *password, int maxTries);
void startAccessPoint(const char *ap_ssid, const char *ap_password,
                      IPAddress ip, IPAddress gateway, IPAddress subnet);
