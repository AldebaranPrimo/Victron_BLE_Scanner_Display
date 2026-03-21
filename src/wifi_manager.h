#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

bool wifiConnect(const char* ssid, const char* password, uint16_t timeoutMs = 10000);
bool wifiIsConnected();
void wifiReconnectIfNeeded();
int wifiRSSI();

#endif
