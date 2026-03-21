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

static void publish(const char* subtopic, const char* payload) {
    char topic[96];
    snprintf(topic, sizeof(topic), "%s/%s", _baseTopic, subtopic);
    mqtt.publish(topic, payload, true); // retained
}

void mqttPublishSolar(const SolarDisplayData& d) {
    if (!d.valid) return;

    const char* stateText = "unknown";
    const char* states[] = {"off", "low_power", "fault", "bulk", "absorption", "float", "storage", "equalize"};
    if (d.chargeState <= 7) stateText = states[d.chargeState];

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"device_state\":%d,\"device_state_text\":\"%s\",\"charger_error\":%d,"
        "\"battery_voltage\":%.2f,\"battery_current\":%.1f,"
        "\"yield_today\":%.0f,\"pv_power\":%d,\"load_current\":%.1f,\"rssi\":%d}",
        d.chargeState, stateText, d.errorCode,
        d.batteryVoltage, d.batteryCurrent,
        d.todayYield, d.inputPower, d.loadCurrent, d.rssi);

    publish("mppt/state", buf);
}

void mqttPublishShunt(const ShuntDisplayData& d) {
    if (!d.valid) return;

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"soc\":%.1f,\"battery_voltage\":%.2f,\"battery_current\":%.2f,"
        "\"time_to_go_min\":%d,\"consumed_ah\":%.1f,\"alarm_reason\":0,\"rssi\":%d}",
        d.soc, d.batteryVoltage, d.batteryCurrent,
        d.ttg, d.consumedAh, d.rssi);

    publish("smartshunt/state", buf);
}

void mqttPublishBatterySense(const BatterySenseDisplayData& d) {
    if (!d.valid) return;

    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"battery_voltage\":%.2f,\"temperature\":%.1f,\"rssi\":%d}",
        d.batteryVoltage, d.temperature, d.rssi);

    publish("battery_sense/state", buf);
}

void mqttPublishGatewayStatus(unsigned long uptimeSec) {
    char buf[192];
    snprintf(buf, sizeof(buf),
        "{\"wifi_rssi\":%d,\"uptime_sec\":%lu,\"free_heap\":%u,\"min_free_heap\":%u}",
        WiFi.RSSI(), uptimeSec, ESP.getFreeHeap(), ESP.getMinFreeHeap());

    publish("gateway/status", buf);
}
