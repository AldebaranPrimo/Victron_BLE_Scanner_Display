#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

// ============================================================================
// config_manager.h — Persistent gateway configuration (NVS / Preferences)
//
// Holds the whole runtime configuration of the gateway: WiFi credentials, MQTT
// broker settings, and a fixed-size array of Victron device slots. The config
// is serialized to the ESP32 NVS namespace "victron" (see config_manager.cpp).
//
// N-device model: every device slot is independent and addressed by its slot
// index. The per-device `name` is also the MQTT topic segment, so a slot named
// "mppt" publishes to "<base>/mppt/state". This is what lets several devices of
// the SAME type (e.g. multiple SmartSolar MPPTs) coexist, each on its own topic.
//
// NVS schema versioning: a `cfg_ver` key (see CONFIG_VERSION) drives forward
// migration. Legacy v1 configs (3-device layout, no version key) are read
// transparently — missing higher slots simply default to disabled. See the
// migration notes in config_manager.cpp.
// ============================================================================

#include <Arduino.h>

// Maximum number of independently-configurable Victron device slots.
// Raised from the legacy 3 to 6 to allow multiple MPPTs (e.g. mppt, mppt2,
// mppt3) plus smartshunt, battery_sense, and one spare slot.
#define MAX_DEVICES 6

// NVS configuration schema version. Bump whenever the on-disk layout changes
// in a way that requires migration logic.
//   v1 = legacy 3-device layout (implicit: NVS had NO "cfg_ver" key).
//   v2 = current 6-device layout + explicit "cfg_ver" key.
#define CONFIG_VERSION 2

// Firmware release version — shown on the boot banner and the setup screen.
// Bump on each significant release. 2.0.0 = multi-device refactor: N devices,
// per-instance victron/<name>/state topics, M5StickC Plus target, paged display.
#define FW_VERSION "2.0.0"

struct DeviceConfig {
    bool enabled;
    char name[16];      // friendly name AND MQTT topic segment ([a-z0-9_], <=15 chars)
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

// Fills any enabled device that has an empty name with its canonical back-compat
// topic segment (first solar -> "mppt", further solars -> "mppt2"..., shunt ->
// "smartshunt", bsense -> "battery_sense"). Never overwrites a user-set name.
// Called by configLoad() and by the portal save path so both produce identical
// names. See implementation notes in config_manager.cpp.
void normalizeDefaultNames(GatewayConfig& cfg);

#endif
