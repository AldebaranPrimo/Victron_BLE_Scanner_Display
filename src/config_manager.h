#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

#define MAX_DEVICES 3

struct DeviceConfig {
    bool enabled;
    char name[16];
    char mac[13];       // 12 hex chars + null
    char aesKey[33];    // 32 hex chars + null
    uint8_t type;       // 0=Solar, 1=Shunt, 2=BatterySense
};

struct GatewayConfig {
    char wifiSsid[33];
    char wifiPass[65];
    char mqttBroker[65];
    uint16_t mqttPort;
    char mqttUser[33];
    char mqttPass[33];
    char mqttBaseTopic[33];
    uint16_t mqttPublishInterval; // seconds
    DeviceConfig devices[MAX_DEVICES];
};

bool configLoad(GatewayConfig& cfg);
bool configSave(const GatewayConfig& cfg);
bool configIsValid(const GatewayConfig& cfg);
void configClear();

#endif
