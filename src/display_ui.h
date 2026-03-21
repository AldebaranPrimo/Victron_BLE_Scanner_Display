#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>
#include "victron_ble.h"

void displayInit();
void displayBootMessage(const char* line1, const char* line2);
void displayConfigMode(const char* ssid, const char* ip);
void displayNormalUpdate(bool wifiOk, bool mqttOk);
void displayHandleButtons(int& displayPage, int& displayRotation);

#endif
