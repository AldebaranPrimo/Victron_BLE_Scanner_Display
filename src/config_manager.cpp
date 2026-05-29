// ============================================================================
// config_manager.cpp — NVS-backed configuration load/save/validate
//
// Persists GatewayConfig to the ESP32 NVS namespace "victron" via Preferences.
// Device slots are stored under per-index keys dev{0..MAX_DEVICES-1}_{en,name,
// mac,key,type}. A "cfg_ver" key records the schema version (CONFIG_VERSION).
//
// Migration model (v1 -> v2): the legacy firmware stored 3 device slots and no
// version key. Reading 6 slots from such an NVS is safe by construction: every
// getX(key, default) returns the supplied default for the missing dev3..dev5
// keys (enabled -> false). So widening the array is a transparent, lossless
// upgrade. We do NOT rewrite NVS on load (load stays read-only); the v2 layout
// is committed lazily on the next configSave(), which writes cfg_ver=2. This
// avoids write-on-boot flash wear.
//
// A downgrade guard rejects configs written by a FUTURE firmware (storedVer >
// CONFIG_VERSION) so a newer-than-known layout is never silently misread —
// instead the device falls back to setup mode.
// ============================================================================

#include "config_manager.h"
#include <Preferences.h>

static Preferences prefs;

// ----------------------------------------------------------------------------
// normalizeDefaultNames — back-compat topic-name safety net (in-memory only)
//
// Ensures every ENABLED device that has no explicit name gets the canonical
// default topic segment, so legacy configs keep publishing to the exact same
// topics they always did:
//   first solar  -> "mppt"   (additional solars -> "mppt2", "mppt3", ...)
//   first shunt  -> "smartshunt"
//   first bsense -> "battery_sense"
// A user-chosen name is NEVER overwritten — we only fill empty names. The same
// logic runs in the portal save path (config_portal.cpp) so saved configs are
// self-consistent with what a freshly-loaded legacy config would produce.
// Declared in config_manager.h so the portal can reuse it.
// ----------------------------------------------------------------------------
void normalizeDefaultNames(GatewayConfig& cfg) {
    int solarCount = 0;
    for (int i = 0; i < MAX_DEVICES; i++) {
        DeviceConfig& d = cfg.devices[i];
        if (!d.enabled) continue;

        bool isSolar = (d.type == 0); // 0 = Solar (DEVICE_TYPE_SOLAR)
        if (d.name[0] == '\0') {
            if (isSolar) {
                if (solarCount == 0) {
                    strncpy(d.name, "mppt", sizeof(d.name) - 1);
                } else {
                    snprintf(d.name, sizeof(d.name), "mppt%d", solarCount + 1);
                }
            } else if (d.type == 1) { // 1 = Shunt
                strncpy(d.name, "smartshunt", sizeof(d.name) - 1);
            } else {                  // 2 = BatterySense (and any other)
                strncpy(d.name, "battery_sense", sizeof(d.name) - 1);
            }
        }
        if (isSolar) solarCount++;
    }
}

bool configLoad(GatewayConfig& cfg) {
    // First open read-write to ensure namespace exists, then close
    prefs.begin("victron", false);
    prefs.end();

    prefs.begin("victron", true); // read-only

    bool valid = prefs.getBool("config_ok", false);
    if (!valid) {
        prefs.end();
        memset(&cfg, 0, sizeof(cfg));
        cfg.mqttPort = 1883;
        cfg.mqttPublishInterval = 5;
        strncpy(cfg.mqttBaseTopic, "victron", sizeof(cfg.mqttBaseTopic));
        return false;
    }

    // Schema version. Absent key => legacy v1 (no "cfg_ver" was ever written).
    uint8_t storedVer = prefs.getUChar("cfg_ver", 1);

    // Downgrade guard: a future firmware wrote a newer schema than we know how
    // to read. Treat as invalid -> forces setup mode rather than misreading.
    if (storedVer > CONFIG_VERSION) {
        prefs.end();
        memset(&cfg, 0, sizeof(cfg));
        cfg.mqttPort = 1883;
        cfg.mqttPublishInterval = 5;
        strncpy(cfg.mqttBaseTopic, "victron", sizeof(cfg.mqttBaseTopic));
        Serial.printf("[CFG] Stored cfg_ver=%u newer than firmware v%u; ignoring config\n",
            storedVer, CONFIG_VERSION);
        return false;
    }

    // WiFi
    String s;
    s = prefs.getString("wifi_ssid", "");
    strncpy(cfg.wifiSsid, s.c_str(), sizeof(cfg.wifiSsid) - 1);
    s = prefs.getString("wifi_pass", "");
    strncpy(cfg.wifiPass, s.c_str(), sizeof(cfg.wifiPass) - 1);

    // MQTT
    s = prefs.getString("mqtt_broker", "");
    strncpy(cfg.mqttBroker, s.c_str(), sizeof(cfg.mqttBroker) - 1);
    cfg.mqttPort = prefs.getUShort("mqtt_port", 1883);
    s = prefs.getString("mqtt_user", "");
    strncpy(cfg.mqttUser, s.c_str(), sizeof(cfg.mqttUser) - 1);
    s = prefs.getString("mqtt_pass", "");
    strncpy(cfg.mqttPass, s.c_str(), sizeof(cfg.mqttPass) - 1);
    s = prefs.getString("mqtt_topic", "victron");
    strncpy(cfg.mqttBaseTopic, s.c_str(), sizeof(cfg.mqttBaseTopic) - 1);
    cfg.mqttPublishInterval = prefs.getUShort("mqtt_interval", 5);

    // Devices
    for (int i = 0; i < MAX_DEVICES; i++) {
        char key[16];
        snprintf(key, sizeof(key), "dev%d_en", i);
        cfg.devices[i].enabled = prefs.getBool(key, false);
        snprintf(key, sizeof(key), "dev%d_name", i);
        s = prefs.getString(key, "");
        strncpy(cfg.devices[i].name, s.c_str(), sizeof(cfg.devices[i].name) - 1);
        snprintf(key, sizeof(key), "dev%d_mac", i);
        s = prefs.getString(key, "");
        strncpy(cfg.devices[i].mac, s.c_str(), sizeof(cfg.devices[i].mac) - 1);
        snprintf(key, sizeof(key), "dev%d_key", i);
        s = prefs.getString(key, "");
        strncpy(cfg.devices[i].aesKey, s.c_str(), sizeof(cfg.devices[i].aesKey) - 1);
        snprintf(key, sizeof(key), "dev%d_type", i);
        cfg.devices[i].type = prefs.getUChar(key, 0);
    }

    prefs.end();

    // Back-compat safety net: fill any empty-named enabled slot with its
    // canonical default topic segment. In-memory only — load stays read-only.
    normalizeDefaultNames(cfg);

    return true;
}

bool configSave(const GatewayConfig& cfg) {
    prefs.begin("victron", false); // read-write

    prefs.putString("wifi_ssid", cfg.wifiSsid);
    prefs.putString("wifi_pass", cfg.wifiPass);
    prefs.putString("mqtt_broker", cfg.mqttBroker);
    prefs.putUShort("mqtt_port", cfg.mqttPort);
    prefs.putString("mqtt_user", cfg.mqttUser);
    prefs.putString("mqtt_pass", cfg.mqttPass);
    prefs.putString("mqtt_topic", cfg.mqttBaseTopic);
    prefs.putUShort("mqtt_interval", cfg.mqttPublishInterval);

    for (int i = 0; i < MAX_DEVICES; i++) {
        char key[16];
        snprintf(key, sizeof(key), "dev%d_en", i);
        prefs.putBool(key, cfg.devices[i].enabled);
        snprintf(key, sizeof(key), "dev%d_name", i);
        prefs.putString(key, cfg.devices[i].name);
        snprintf(key, sizeof(key), "dev%d_mac", i);
        prefs.putString(key, cfg.devices[i].mac);
        snprintf(key, sizeof(key), "dev%d_key", i);
        prefs.putString(key, cfg.devices[i].aesKey);
        snprintf(key, sizeof(key), "dev%d_type", i);
        prefs.putUChar(key, cfg.devices[i].type);
    }

    prefs.putBool("config_ok", true);
    prefs.putUChar("cfg_ver", CONFIG_VERSION); // commit the v2 schema marker
    prefs.end();
    return true;
}

bool configIsValid(const GatewayConfig& cfg) {
    if (strlen(cfg.wifiSsid) == 0) return false;
    if (strlen(cfg.mqttBroker) == 0) return false;

    bool anyDevice = false;
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (cfg.devices[i].enabled) {
            if (strlen(cfg.devices[i].mac) != 12) return false;
            if (strlen(cfg.devices[i].aesKey) != 32) return false;
            anyDevice = true;
        }
    }
    return anyDevice;
}

void configClear() {
    prefs.begin("victron", false);
    prefs.clear();
    prefs.end();
}
