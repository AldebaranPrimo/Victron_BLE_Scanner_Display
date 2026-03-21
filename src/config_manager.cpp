#include "config_manager.h"
#include <Preferences.h>

static Preferences prefs;

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
