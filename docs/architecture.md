# Architecture

## Overview

The firmware runs on an **M5StickC Plus** (ESP32-PICO-D4, 520KB SRAM, no PSRAM; 135x240 ST7789v2 display) and operates in two mutually exclusive modes. The base M5StickC (80x160) is still buildable as a legacy target.

- **Config Mode**: WiFi Access Point + web portal for configuration
- **Normal Mode**: BLE scanning + WiFi STA + MQTT publishing

The two modes never run simultaneously, which keeps RAM usage well within limits.

## System Diagram

Up to `MAX_DEVICES` (6) Victron devices are monitored, including **multiple
MPPTs**. Each advertisement is routed to its configured slot **by MAC address**,
parsed by record type, and published to its own `victron/<name>/state` topic.

```
                    BLE Advertisements (~1/sec)
┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│SmartSolar    │  │SmartSolar    │  │SmartBattery  │  │ SmartShunt   │
│MPPT  (mppt)  │  │MPPT2 (mppt2) │  │Sense         │  │              │
│(Record 0x01) │  │(Record 0x01) │  │(Record 0x02) │  │(Record 0x02) │
└──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘
       │                 │                 │                 │
       └─────────────────┴────────┬────────┴─────────────────┘
                                │
                           BLE Passive Scan
                          (route by MAC → slot)
                                │
                        ┌───────┴───────┐
                        │ M5StickC Plus │
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
      ├─ publishIfDue() → MQTT JSON every N seconds (one msg per device slot)
      ├─ displayNormalUpdate(page) → paged multi-device display
      └─ handleButtons() → Button A cycles pages, Button B rotates
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

### N-device data flow

- `rtDevices[]`, `devStates[]` and `GatewayConfig.devices[]` are **all parallel
  arrays** indexed by the same config slot `0..MAX_DEVICES-1`. There is **no
  compaction**: disabled slots are simply skipped (gated by `enabled`). This
  removes index-aliasing between the config slot and the runtime index.
- The BLE callback matches each advertisement to a slot via `findDeviceByMac()`
  (by MAC, not by type) and writes the parsed result into
  `devStates[slot].data.<variant>`, stamping `lastUpdateMs`.
- `mqtt_publisher` derives the topic from `devStates[slot].name`
  (`<base>/<name>/state`) and selects the JSON schema from `devStates[slot].type`.
- `display_ui` pages over `devStates[]` (summary + one detail page per device).

## Memory Budget

| Mode | Free Heap (approx) | Notes |
|---|---|---|
| Config mode (WiFi AP + WebServer) | ~280 KB | No BLE running |
| Normal mode (BLE + WiFi STA + MQTT) | ~140 KB | Comfortable margin |

The BLE Bluedroid stack uses ~115 KB. If future requirements push RAM too tight, migrating to NimBLE-Arduino (~60 KB) is the fallback plan.

The per-slot state array (`DeviceRuntimeState devStates[MAX_DEVICES]`) uses a
`union` of the per-type payload structs, so all 6 slots together add under
0.5 KB — negligible against the heap margin above.

## Flash Budget

Using a custom partition table without OTA (`partitions_noota.csv`), the app partition is 3 MB. Current firmware uses ~1.66 MB (52.8%). OTA would require migrating to NimBLE to fit in the default 1.25 MB partition.

## Configuration Storage

Configuration is stored in ESP32's NVS (Non-Volatile Storage) using the `Preferences` library, namespace `"victron"`. Keys:

| Key | Type | Description |
|---|---|---|
| `config_ok` | bool | Validity flag |
| `cfg_ver` | uint8 | Schema version (migration). Absent ⇒ legacy v1; current ⇒ 2 |
| `wifi_ssid` | string | WiFi SSID |
| `wifi_pass` | string | WiFi password |
| `mqtt_broker` | string | MQTT broker IP/hostname |
| `mqtt_port` | uint16 | MQTT port (default 1883) |
| `mqtt_user` | string | MQTT username |
| `mqtt_pass` | string | MQTT password |
| `mqtt_topic` | string | Base topic (default "victron") |
| `mqtt_interval` | uint16 | Publish interval in seconds |
| `dev{0-5}_en` | bool | Device enabled |
| `dev{0-5}_name` | string | Device name (also the MQTT topic segment) |
| `dev{0-5}_mac` | string | MAC address (12 hex chars) |
| `dev{0-5}_key` | string | AES encryption key (32 hex chars) |
| `dev{0-5}_type` | uint8 | 0=Solar, 1=Shunt, 2=BatterySense |

### Schema migration (v1 → v2)

The legacy firmware stored 3 device slots and **no** `cfg_ver` key. The current
firmware reads 6 slots: missing keys (`dev3..dev5_*`) return their defaults
(`enabled=false`), so an old config upgrades transparently and losslessly. The
load path stays **read-only** — the v2 layout (with `cfg_ver=2`) is committed
lazily on the next save, avoiding write-on-boot flash wear. A **downgrade guard**
rejects any config whose `cfg_ver` is newer than the running firmware (forcing
setup mode) so a future layout is never silently misread.
