#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include <Arduino.h>
#include "victron_ble.h"

void mqttSetup(const char* broker, uint16_t port, const char* user, const char* pass, const char* baseTopic);
bool mqttConnect();
void mqttLoop();
bool mqttIsConnected();
void mqttReconnectIfNeeded();
void mqttPublishSolar(const SolarDisplayData& data);
void mqttPublishShunt(const ShuntDisplayData& data);
void mqttPublishBatterySense(const BatterySenseDisplayData& data);
void mqttPublishGatewayStatus(unsigned long uptimeSec);

#endif
