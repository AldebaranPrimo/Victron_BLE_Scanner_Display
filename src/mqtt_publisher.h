#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

// ============================================================================
// mqtt_publisher.h — MQTT JSON publishing (PubSubClient)
//
// N-device model: a single slot-driven publisher (mqttPublishDevice) replaces
// the legacy per-type functions. The MQTT topic is derived from the slot's name
// ("<baseTopic>/<name>/state"), and the JSON payload is selected by the slot's
// type. With the default config (mppt / smartshunt / battery_sense) the topics
// and payloads are byte-for-byte identical to the legacy firmware.
// ============================================================================

#include <Arduino.h>
#include "victron_ble.h"

void mqttSetup(const char* broker, uint16_t port, const char* user, const char* pass, const char* baseTopic);
bool mqttConnect();
void mqttLoop();
bool mqttIsConnected();
void mqttReconnectIfNeeded();

// Publishes one runtime slot to "<baseTopic>/<slot.name>/state". The payload
// schema is chosen by slot.type and is byte-for-byte identical to the legacy
// per-type payloads (back-compat). No-op if the slot is unused or has no valid
// data yet.
void mqttPublishDevice(const DeviceRuntimeState& slot);

void mqttPublishGatewayStatus(unsigned long uptimeSec);

#endif
