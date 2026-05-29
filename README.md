# Victron BLE Gateway

ESP32-based gateway that reads BLE advertisements from Victron Energy devices and publishes data via MQTT. Primary board: **M5StickC Plus** (135×240). The original M5StickC still builds.

> **⚠️ Testing branch `feature/multi-device-mppt` — firmware v2.0.0.**
> Multi-device refactor: monitor several Victron devices of the same type
> (e.g. multiple SmartSolar MPPTs), each on its own `victron/<name>/state` topic.
> Validated on real hardware (M5StickC Plus) but **not a tagged release** — see
> [Running alongside an existing gateway](#running-alongside-an-existing-gateway),
> [Build & Flash Notes](docs/build-and-flash.md), and
> [Known limitations](#known-limitations-testing-branch) before using it.

## Features

- **Passive BLE scanning** of up to **6 Victron devices** simultaneously,
  including **multiple SmartSolar MPPTs** (e.g. one per sub-array):
  - SmartSolar MPPT (solar charger) — any number, up to the slot limit
  - SmartShunt (battery monitor)
  - SmartBatterySense (temperature sensor)
- **Per-device MQTT topics** derived from each device's name (`victron/<name>/state`),
  with **byte-for-byte back-compat** for the default `mppt` / `smartshunt` /
  `battery_sense` topics.
- **AES-128-CTR decryption** of Victron Instant Readout data
- **MQTT publishing** with JSON payloads, compatible with Home Assistant and Node-RED
- **Web-based configuration portal** (no need to recompile for WiFi/MQTT/device changes)
- **Paged multi-device display**: a summary page plus one detail page per device

## Hardware

| Component | Spec |
|---|---|
| Board (primary) | **M5StickC Plus (ESP32-PICO-D4)** |
| CPU | 240 MHz dual-core |
| SRAM | 520 KB (no PSRAM) |
| Flash | 4 MB |
| Display | **ST7789v2 135×240** (Plus) / ST7735S 80×160 (base M5StickC) |
| BLE | v4.2 (sufficient for Victron advertisements) |
| WiFi | 802.11 b/g/n 2.4 GHz |

The base M5StickC (80×160) still builds via the `m5stick-c` environment; the
display code is resolution-independent.

## Quick Start

### 1. Build and Flash

Requires [PlatformIO](https://platformio.org/).

```bash
# Clone
git clone https://github.com/AldebaranPrimo/Victron_BLE_Gateway.git
cd Victron_BLE_Gateway

# Build for M5StickC Plus (primary / default env)
pio run -e m5stick-c-plus

# Upload
pio run -e m5stick-c-plus -t upload

# (Legacy) base M5StickC:
#   pio run -e m5stick-c -t upload
```

Or open in VS Code with the PlatformIO extension (workspace file included).

> This branch is set up for the **pioarduino** platform (`arduino-esp32 3.x`),
> which affects the board/variant settings and the upload procedure on some
> USB-serial chips. If `pio run`/upload misbehaves, read
> **[docs/build-and-flash.md](docs/build-and-flash.md)** — it documents the
> toolchain, board/variant, and flashing quirks with copy-paste fixes.

### 2. Configure

On first boot, the device automatically enters **Setup Mode**:

1. Connect to WiFi **`VictronBLE-Setup`** (open, no password)
2. A configuration page opens at `http://192.168.4.1`
3. Enter your WiFi credentials, MQTT broker, and Victron device details
4. Click **Save** - the device reboots and starts operating

To re-enter setup mode later, hold **Button B** (side) for 3 seconds during boot.

### 3. Get Victron Device Keys

For each Victron device, open **VictronConnect**:

Settings > Product Info > enable "Instant readout via Bluetooth" > tap SHOW

Note the **Encryption Key** (32 hex chars) and **MAC address** (enter without colons).

**Requirements**: Victron firmware v3.61+, Instant Readout enabled.

## MQTT Topics

Default base topic: `victron`. Each device publishes to a topic built from its
**name**: `victron/<name>/state`. Names are lowercase `[a-z0-9_]`.

| Topic | Data |
|---|---|
| `victron/<name>/state` | One device. Schema depends on its type (solar / shunt / bsense). |
| `victron/gateway/status` | Gateway health: uptime, free heap, WiFi RSSI |

**Back-compat defaults** (unchanged from the legacy firmware):

| Default name | Topic | Device |
|---|---|---|
| `mppt` | `victron/mppt/state` | First SmartSolar MPPT |
| `mppt2`, `mppt3`, … | `victron/mppt2/state`, … | Additional MPPTs |
| `smartshunt` | `victron/smartshunt/state` | SmartShunt |
| `battery_sense` | `victron/battery_sense/state` | SmartBatterySense |

See [docs/mqtt-topics.md](docs/mqtt-topics.md) for full payload details and Home Assistant examples (including a second MPPT).

## Running alongside an existing gateway

A topic is unique per **(base topic, device name)** pair. Two gateways only
collide if they publish a device with the **same name under the same base topic**.
This lets you run this firmware next to a legacy single-device gateway while you
validate it:

- **Distinct names (simplest).** Keep base topic `victron` on both, but give the
  new gateway's device(s) names the old one doesn't use — e.g. configure a single
  device named `mppt2` → it publishes `victron/mppt2/state`, leaving the legacy
  `victron/mppt/state` untouched. Add the new sensor in Home Assistant and the two
  coexist.
- **Distinct base topic (full isolation).** Set the new gateway's base topic to
  e.g. `victron_test`; then no name can ever clash with the legacy `victron/...`.

When validation is done, let this gateway **replace** the legacy one: it becomes
the single publisher on base `victron` with `mppt` / `mppt2` / `mppt3` /
`smartshunt` / `battery_sense`, and you retire the old unit — one publisher per
namespace, no possible conflict.

## Display

In normal operation the display is **paged** (Button A cycles pages):

- **Page 0 — Summary**: one compact line per device (name + key metric: MPPT W,
  SmartShunt SoC %, BatterySense °C). Scales from 1 to 6 devices.
- **Pages 1..N — Detail**: one device per page, with its name in the header and
  type-specific large readouts. Stale devices (no data for >60 s) are dimmed.

Status indicator (top-right dot): green = all connected, yellow = WiFi only, red = disconnected. Button B rotates the orientation.

## Documentation

- [Configuration Guide](docs/configuration-guide.md) - Setup instructions and device key retrieval
- [MQTT Topics](docs/mqtt-topics.md) - Topic structure, JSON payloads, Home Assistant integration
- [Architecture](docs/architecture.md) - System design, boot flow, memory budget, module structure
- [Build & Flash Notes](docs/build-and-flash.md) - Platform/board setup, flashing, and environment troubleshooting
- [Multi-device refactor spec](docs/multi-device-refactor-spec.md) - Design intent for this branch (historical)

## Known limitations (testing branch)

- **Duplicate-name dedup is single-pass.** The portal auto-disambiguates a device
  name that collides with an earlier slot, but does not re-check the result
  against *all* slots. With default names you are safe; pathological custom names
  could still produce two devices on the same topic. Hardening planned.
- **Base M5StickC (80×160) display clips with many devices.** The layout is
  resolution-aware but not row-limiting on the tiny panel; >~4 devices overflow.
  The primary **M5StickC Plus** (135×240) renders 1–6 devices cleanly.
- **arduino-esp32 3.x boot warnings.** The `M5StickCPlus@0.1.1` library logs
  non-fatal `gpio`/`ledc` warnings on 3.x; the display works regardless. See
  [Build & Flash Notes](docs/build-and-flash.md).

## Project Structure

```
src/
  main.cpp              # Boot flow, mode selection, main loop
  config_manager.h/cpp  # NVS-based configuration storage
  config_portal.h/cpp   # WiFi AP + captive portal web server
  config_html.h         # Configuration page HTML (PROGMEM)
  wifi_manager.h/cpp    # WiFi STA connection management
  mqtt_publisher.h/cpp  # MQTT JSON publishing
  victron_ble.h/cpp     # BLE scanning, AES decryption, data parsing
  display_ui.h/cpp      # Display rendering
docs/                   # Documentation
platformio.ini          # PlatformIO configuration
partitions_noota.csv    # Custom partition table (3MB app, no OTA)
```

## Credits

- BLE parsing based on [AldebaranPrimo/Victron_BLE_Scanner_Display](https://github.com/AldebaranPrimo/Victron_BLE_Scanner_Display) and [hoberman/Victron_BLE_Scanner_Display](https://github.com/hoberman/Victron_BLE_Scanner_Display)
- Victron BLE protocol reference: [Fabian-Schmidt/esphome-victron_ble](https://github.com/Fabian-Schmidt/esphome-victron_ble)

## License

MIT
