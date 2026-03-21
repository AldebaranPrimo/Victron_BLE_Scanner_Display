# Architecture

## Overview

The firmware runs on an M5StickC (ESP32-PICO-D4, 520KB SRAM, no PSRAM) and operates in two mutually exclusive modes:

- **Config Mode**: WiFi Access Point + web portal for configuration
- **Normal Mode**: BLE scanning + WiFi STA + MQTT publishing

The two modes never run simultaneously, which keeps RAM usage well within limits.

## System Diagram

```
                    BLE Advertisements (~1/sec)
┌──────────────┐         ┌──────────────┐         ┌──────────────┐
│SmartSolar    │ )))  ((( │SmartBattery  │ )))  ((( │ SmartShunt   │
│MPPT          │         │Sense         │         │              │
│(Record 0x01) │         │(Record 0x02) │         │(Record 0x02) │
└──────────────┘         └──────────────┘         └──────────────┘
       │                        │                        │
       └────────────────────────┼────────────────────────┘
                                │
                           BLE Passive Scan
                                │
                        ┌───────┴───────┐
                        │   M5StickC    │
                        │  ESP32-PICO   │
                        │               │
                        │ BLE scan      │
                        │ AES decrypt   │
                        │ Parse records │
                        │ WiFi + MQTT   │
                        └───────┬───────┘
                                │
                            WiFi/MQTT
                                │
                        ┌───────┴───────┐
                        │  MQTT Broker  │
                        │  (Mosquitto)  │
                        └───────┬───────┘
                                │
                    ┌───────────┼───────────┐
                    │                       │
            ┌───────┴───────┐       ┌───────┴───────┐
            │   Node-RED    │       │Home Assistant  │
            └───────────────┘       └───────────────┘
```

## Boot Flow

```
setup()
  │
  ├─ M5.begin() + display init
  ├─ Show "VictronBLE GW" + "Tieni B 3s = Setup"
  ├─ Poll GPIO39 (Button B) for 3 seconds
  │
  ├─ Button held OR no valid config?
  │   ├─ YES → CONFIG MODE
  │   │   ├─ WiFi.softAP("VictronBLE-Setup")  // open AP
  │   │   ├─ Start WebServer + DNS (captive portal)
  │   │   ├─ Serve config form at 192.168.4.1
  │   │   └─ On save → write NVS → reboot
  │   │
  │   └─ NO → NORMAL MODE
  │       ├─ Load config from NVS
  │       ├─ bleInit() → setup BLE scanner with device keys
  │       ├─ wifiConnect() → join home WiFi
  │       └─ mqttSetup() → configure MQTT client
  │
loop()
  │
  ├─ CONFIG MODE: portalLoop() → handle web clients
  │
  └─ NORMAL MODE:
      ├─ bleScan() → ~1s blocking scan
      ├─ wifiReconnectIfNeeded()
      ├─ mqttReconnectIfNeeded() + mqttLoop()
      ├─ publishIfDue() → MQTT JSON every N seconds
      ├─ displayNormalUpdate() → 3-row power display
      └─ handleButtons()
```

## Module Structure

```
src/
  main.cpp              ← Orchestrator: boot flow, mode selection, main loop
  config_manager.h/cpp  ← Config struct, NVS load/save/validate
  config_portal.h/cpp   ← WiFi AP + WebServer + captive portal
  config_html.h         ← HTML form (PROGMEM, ~4KB in flash, zero RAM)
  wifi_manager.h/cpp    ← WiFi STA connect/reconnect with backoff
  mqtt_publisher.h/cpp  ← MQTT publish JSON payloads (PubSubClient)
  victron_ble.h/cpp     ← BLE scan, AES-CTR decrypt, bit-level parsing
  display_ui.h/cpp      ← Display rendering for boot/config/normal modes
```

## Memory Budget

| Mode | Free Heap (approx) | Notes |
|---|---|---|
| Config mode (WiFi AP + WebServer) | ~280 KB | No BLE running |
| Normal mode (BLE + WiFi STA + MQTT) | ~140 KB | Comfortable margin |

The BLE Bluedroid stack uses ~115 KB. If future requirements push RAM too tight, migrating to NimBLE-Arduino (~60 KB) is the fallback plan.

## Flash Budget

Using a custom partition table without OTA (`partitions_noota.csv`), the app partition is 3 MB. Current firmware uses ~1.66 MB (52.8%). OTA would require migrating to NimBLE to fit in the default 1.25 MB partition.

## Configuration Storage

Configuration is stored in ESP32's NVS (Non-Volatile Storage) using the `Preferences` library, namespace `"victron"`. Keys:

| Key | Type | Description |
|---|---|---|
| `config_ok` | bool | Validity flag |
| `wifi_ssid` | string | WiFi SSID |
| `wifi_pass` | string | WiFi password |
| `mqtt_broker` | string | MQTT broker IP/hostname |
| `mqtt_port` | uint16 | MQTT port (default 1883) |
| `mqtt_user` | string | MQTT username |
| `mqtt_pass` | string | MQTT password |
| `mqtt_topic` | string | Base topic (default "victron") |
| `mqtt_interval` | uint16 | Publish interval in seconds |
| `dev{0-2}_en` | bool | Device enabled |
| `dev{0-2}_name` | string | Device name |
| `dev{0-2}_mac` | string | MAC address (12 hex chars) |
| `dev{0-2}_key` | string | AES encryption key (32 hex chars) |
| `dev{0-2}_type` | uint8 | 0=Solar, 1=Shunt, 2=BatterySense |
