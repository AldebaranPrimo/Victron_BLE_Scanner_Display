// ============================================================================
// mqtt_publisher.cpp — MQTT JSON publishing
//
// Topic-by-name model: mqttPublishDevice() builds the topic from the slot name
// ("<base>/<name>/state") and the JSON payload from the slot type. The format
// strings and field order are unchanged from the legacy per-type publishers, so
// downstream Home Assistant / Node-RED mappings stay stable. gateway/status is
// published unchanged via the existing private publish() helper.
// ============================================================================

#include "mqtt_publisher.h"
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient espClient;
static PubSubClient mqtt(espClient);

static char _baseTopic[33] = "victron";
static char _user[33] = "";
static char _pass[33] = "";
static unsigned long _lastReconnectAttempt = 0;

void mqttSetup(const char* broker, uint16_t port, const char* user, const char* pass, const char* baseTopic) {
    mqtt.setServer(broker, port);
    // 512B buffer: largest payload (solar JSON) is ~200B and the longest topic
    // is ~55B, so this leaves comfortable headroom for PubSubClient's framing.
    mqtt.setBufferSize(512);
    strncpy(_baseTopic, baseTopic, sizeof(_baseTopic) - 1);
    strncpy(_user, user, sizeof(_user) - 1);
    strncpy(_pass, pass, sizeof(_pass) - 1);
}

bool mqttConnect() {
    Serial.println("[MQTT] Connecting...");
    bool ok;
    if (strlen(_user) > 0) {
        ok = mqtt.connect("victron-ble-gw", _user, _pass);
    } else {
        ok = mqtt.connect("victron-ble-gw");
    }
    if (ok) {
        Serial.println("[MQTT] Connected");
    } else {
        Serial.printf("[MQTT] Failed, rc=%d\n", mqtt.state());
    }
    return ok;
}

void mqttLoop() {
    mqtt.loop();
}

bool mqttIsConnected() {
    return mqtt.connected();
}

void mqttReconnectIfNeeded() {
    if (mqtt.connected()) return;

    unsigned long now = millis();
    if (now - _lastReconnectAttempt < 5000) return;
    _lastReconnectAttempt = now;

    mqttConnect();
}

// Publishes to a fixed sub-topic "<base>/<subtopic>". Used for gateway/status,
// whose sub-topic is constant (not derived from a device name).
static void publish(const char* subtopic, const char* payload) {
    char topic[96];
    snprintf(topic, sizeof(topic), "%s/%s", _baseTopic, subtopic);
    mqtt.publish(topic, payload, true); // retained
}

// Publishes to the per-device topic "<base>/<name>/state". `name` is sanitized
// to [a-z0-9_] (<=15 chars) by the config portal, so it is safe as a topic
// segment. Buffer math: base(<=32) + '/' + name(<=15) + "/state"(6) + NUL ~= 55.
static void publishState(const char* name, const char* payload) {
    char topic[96];
    snprintf(topic, sizeof(topic), "%s/%s/state", _baseTopic, name);
    mqtt.publish(topic, payload, true); // retained
}

void mqttPublishDevice(const DeviceRuntimeState& slot) {
    if (!slot.inUse) return;

    char buf[256];
    switch (slot.type) {
        case DEVICE_TYPE_SOLAR: {
            const SolarDisplayData& d = slot.data.solar;
            if (!d.valid) return;
            const char* states[] = {"off", "low_power", "fault", "bulk",
                                    "absorption", "float", "storage", "equalize"};
            const char* st = (d.chargeState <= 7) ? states[d.chargeState] : "unknown";
            snprintf(buf, sizeof(buf),
                "{\"device_state\":%d,\"device_state_text\":\"%s\",\"charger_error\":%d,"
                "\"battery_voltage\":%.2f,\"battery_current\":%.1f,"
                "\"yield_today\":%.0f,\"pv_power\":%d,\"load_current\":%.1f,\"rssi\":%d}",
                d.chargeState, st, d.errorCode, d.batteryVoltage, d.batteryCurrent,
                d.todayYield, d.inputPower, d.loadCurrent, d.rssi);
            break;
        }
        case DEVICE_TYPE_SHUNT: {
            const ShuntDisplayData& d = slot.data.shunt;
            if (!d.valid) return;
            // consumed_ah preserved as 0.0 (never parsed) for payload back-compat.
            snprintf(buf, sizeof(buf),
                "{\"soc\":%.1f,\"battery_voltage\":%.2f,\"battery_current\":%.2f,"
                "\"time_to_go_min\":%d,\"consumed_ah\":%.1f,\"alarm_reason\":0,\"rssi\":%d}",
                d.soc, d.batteryVoltage, d.batteryCurrent, d.ttg, d.consumedAh, d.rssi);
            break;
        }
        case DEVICE_TYPE_BSENSE: {
            const BatterySenseDisplayData& d = slot.data.bsense;
            if (!d.valid) return;
            snprintf(buf, sizeof(buf),
                "{\"battery_voltage\":%.2f,\"temperature\":%.1f,\"rssi\":%d}",
                d.batteryVoltage, d.temperature, d.rssi);
            break;
        }
        default:
            return;
    }

    publishState(slot.name, buf);
}

void mqttPublishGatewayStatus(unsigned long uptimeSec) {
    char buf[192];
    snprintf(buf, sizeof(buf),
        "{\"wifi_rssi\":%d,\"uptime_sec\":%lu,\"free_heap\":%u,\"min_free_heap\":%u}",
        WiFi.RSSI(), uptimeSec, ESP.getFreeHeap(), ESP.getMinFreeHeap());

    publish("gateway/status", buf);
}
