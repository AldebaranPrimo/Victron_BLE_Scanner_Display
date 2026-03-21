# Victron BLE Gateway

ESP32-based gateway that reads BLE advertisements from Victron Energy devices and publishes data via MQTT. Built for the **M5StickC** (first generation).

## Features

- **Passive BLE scanning** of up to 3 Victron devices simultaneously:
  - SmartSolar MPPT (solar charger)
  - SmartShunt (battery monitor)
  - SmartBatterySense (temperature sensor)
- **AES-128-CTR decryption** of Victron Instant Readout data
- **MQTT publishing** with JSON payloads, compatible with Home Assistant and Node-RED
- **Web-based configuration portal** (no need to recompile for WiFi/MQTT/device changes)
- **Compact power display**: solar, battery, and consumption in Watts

## Hardware

| Component | Spec |
|---|---|
| Board | M5StickC (ESP32-PICO-D4) |
| CPU | 240 MHz dual-core |
| SRAM | 520 KB (no PSRAM) |
| Flash | 4 MB |
| Display | ST7735S 80x160 |
| BLE | v4.2 (sufficient for Victron advertisements) |
| WiFi | 802.11 b/g/n 2.4 GHz |

Also builds for M5StickC Plus (`m5stick-c-plus` environment).

## Quick Start

### 1. Build and Flash

Requires [PlatformIO](https://platformio.org/).

```bash
# Clone
git clone https://github.com/AldebaranPrimo/Victron_BLE_Gateway.git
cd Victron_BLE_Gateway

# Build for M5StickC
pio run -e m5stick-c

# Upload
pio run -e m5stick-c -t upload
```

Or open in VS Code with the PlatformIO extension (workspace file included).

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

Default base topic: `victron`

| Topic | Data |
|---|---|
| `victron/mppt/state` | Solar charger: voltage, current, PV power, yield, charge state |
| `victron/smartshunt/state` | Battery monitor: SOC, voltage, current, time-to-go |
| `victron/battery_sense/state` | Battery sensor: voltage, temperature |
| `victron/gateway/status` | Gateway health: uptime, free heap, WiFi RSSI |

See [docs/mqtt-topics.md](docs/mqtt-topics.md) for full payload details and Home Assistant examples.

## Display

In normal operation, the display shows 3 rows:

| Label | Color | Value |
|---|---|---|
| PV | Cyan | Solar input power (W) |
| BT | Green/Red | Battery power (W), green=charge, red=discharge |
| LD | Yellow | Consumption (W) |

Status indicator (top-right dot): green = all connected, yellow = WiFi only, red = disconnected.

## Documentation

- [Configuration Guide](docs/configuration-guide.md) - Setup instructions and device key retrieval
- [MQTT Topics](docs/mqtt-topics.md) - Topic structure, JSON payloads, Home Assistant integration
- [Architecture](docs/architecture.md) - System design, boot flow, memory budget, module structure

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
