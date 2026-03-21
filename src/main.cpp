/*
  Victron BLE Gateway - M5StickC

  Reads BLE advertisements from Victron devices (MPPT, SmartShunt, BatterySense),
  decrypts and publishes data via MQTT.

  Configuration via WiFi AP web portal (hold Button B for 3s at boot).
*/

#include <Arduino.h>
#include "config_manager.h"
#include "config_portal.h"
#include "wifi_manager.h"
#include "mqtt_publisher.h"
#include "victron_ble.h"
#include "display_ui.h"

static GatewayConfig config;
static bool configMode = false;
static unsigned long lastPublish = 0;
static unsigned long bootTime = 0;
static int displayPage = 0;
static int displayRotation = 3;

// Wait for Button B long press during a visible window.
// Shows countdown on display. Returns true if button held for holdMs
// within a windowMs observation period.
static bool checkLongPress(uint8_t pin, uint16_t holdMs, uint16_t windowMs) {
    pinMode(pin, INPUT_PULLUP);
    unsigned long start = millis();
    unsigned long heldTime = 0;
    unsigned long lastCheck = start;
    int lastSec = -1;

    while ((millis() - start) < windowMs) {
        unsigned long now = millis();
        unsigned long dt = now - lastCheck;
        lastCheck = now;

        if (digitalRead(pin) == LOW) {
            heldTime += dt;
        } else {
            heldTime = 0; // reset if released
        }

        // Show countdown on serial
        int secLeft = (int)(windowMs - (now - start)) / 1000;
        if (secLeft != lastSec) {
            lastSec = secLeft;
            Serial.printf("  Setup window: %ds remaining%s\n",
                secLeft, (digitalRead(pin) == LOW) ? " [B pressed]" : "");
        }

        if (heldTime >= holdMs) {
            Serial.println("  Button B held - entering setup!");
            return true;
        }

        delay(50);
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n========================================");
    Serial.println("Victron BLE Gateway v1.0");
    Serial.println("========================================");

    // Init display
    displayInit();

    // Show boot instructions - 5 second window to press button
    displayBootMessage("VictronBLE GW", "Tieni B 3s = Setup");

    // 5-second window, need 3s continuous press within that window
    Serial.println("Hold Button B (side) for 3s to enter setup...");
    bool buttonHeld = checkLongPress(39, 3000, 5000);

    // Load config
    bool configValid = configLoad(config) && configIsValid(config);

    if (buttonHeld || !configValid) {
        // ===================== CONFIG MODE =====================
        configMode = true;

        if (buttonHeld) {
            Serial.println("[MODE] Setup requested via button");
        } else {
            Serial.println("[MODE] No valid config found, entering setup");
        }

        displayConfigMode("VictronBLE-Setup", "192.168.4.1");
        portalStart(config);

    } else {
        // ===================== NORMAL MODE =====================
        configMode = false;
        bootTime = millis();

        Serial.println("[MODE] Normal operation");
        Serial.printf("[CFG] WiFi: %s\n", config.wifiSsid);
        Serial.printf("[CFG] MQTT: %s:%d\n", config.mqttBroker, config.mqttPort);
        Serial.printf("[CFG] Publish interval: %ds\n", config.mqttPublishInterval);

        // Init BLE with loaded config
        bleInit(config);

        // Connect WiFi
        displayBootMessage("WiFi...", config.wifiSsid);
        wifiConnect(config.wifiSsid, config.wifiPass);

        // Setup MQTT
        mqttSetup(config.mqttBroker, config.mqttPort,
                  config.mqttUser, config.mqttPass, config.mqttBaseTopic);

        if (wifiIsConnected()) {
            mqttConnect();
        }

        Serial.printf("[HEAP] Free: %d bytes\n", ESP.getFreeHeap());
        Serial.println("[READY] Scanning for Victron devices...");
    }
}

void loop() {
    if (configMode) {
        // Config mode: just run the portal
        portalLoop();

        if (portalSaved()) {
            displayBootMessage("Salvato!", "Riavvio...");
            delay(2000);
            ESP.restart();
        }
        return;
    }

    // ===================== NORMAL MODE LOOP =====================

    // BLE scan (blocking for ~1 second)
    bleScan();

    // WiFi reconnect
    wifiReconnectIfNeeded();

    // MQTT reconnect and loop
    if (wifiIsConnected()) {
        mqttReconnectIfNeeded();
        mqttLoop();
    }

    // Publish at configured interval
    unsigned long now = millis();
    unsigned long intervalMs = (unsigned long)config.mqttPublishInterval * 1000UL;
    if (mqttIsConnected() && (now - lastPublish >= intervalMs)) {
        lastPublish = now;

        mqttPublishSolar(solarData);
        mqttPublishShunt(shuntData);
        mqttPublishBatterySense(batterySenseData);
        mqttPublishGatewayStatus((now - bootTime) / 1000);
    }

    // Update display
    if (bleNewData) {
        bleNewData = false;
        displayNormalUpdate(wifiIsConnected(), mqttIsConnected());
    }

    // Handle buttons
    displayHandleButtons(displayPage, displayRotation);

    // Periodic heap monitoring
    static unsigned long lastHeapLog = 0;
    if (now - lastHeapLog >= 30000) {
        lastHeapLog = now;
        Serial.printf("[HEAP] Free: %d  Min: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    }
}
