#include "wifi_manager.h"
#include <WiFi.h>

static const char* _ssid = nullptr;
static const char* _pass = nullptr;
static unsigned long _lastReconnectAttempt = 0;
static unsigned long _reconnectInterval = 5000;

bool wifiConnect(const char* ssid, const char* password, uint16_t timeoutMs) {
    _ssid = ssid;
    _pass = password;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.printf("[WiFi] Connecting to %s", ssid);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        _reconnectInterval = 5000;
        return true;
    }

    Serial.println("[WiFi] Connection failed");
    return false;
}

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void wifiReconnectIfNeeded() {
    if (WiFi.status() == WL_CONNECTED) {
        _reconnectInterval = 5000;
        return;
    }

    unsigned long now = millis();
    if (now - _lastReconnectAttempt < _reconnectInterval) return;
    _lastReconnectAttempt = now;

    Serial.println("[WiFi] Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(_ssid, _pass);

    // Exponential backoff, max 30s
    if (_reconnectInterval < 30000) {
        _reconnectInterval = _reconnectInterval * 3 / 2;
    }
}

int wifiRSSI() {
    return WiFi.RSSI();
}
