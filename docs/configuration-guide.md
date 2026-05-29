# Configuration Guide

## First-Time Setup

1. **Flash the firmware** to your M5StickC Plus via USB-C (`pio run -e m5stick-c-plus -t upload`)
2. The device boots and shows: `"VictronBLE GW"` / `"Tieni B 3s = Setup"`
3. Since no configuration exists, it automatically enters **Setup Mode**

## Setup Mode

When in setup mode, the M5StickC:
- Creates a WiFi Access Point named **`VictronBLE-Setup`**
- No password (open network)
- Runs a captive portal web server

### Steps:

1. Connect your phone/PC to the `VictronBLE-Setup` WiFi network
2. A captive portal page should open automatically. If not, navigate to **`http://192.168.4.1`**
3. Fill in the configuration form:

#### WiFi Section
- **SSID**: Your home WiFi network name
- **Password**: Your WiFi password

#### MQTT Section
- **Broker**: IP address or hostname of your MQTT broker (e.g., `192.168.1.100`)
- **Port**: MQTT port (default: `1883`)
- **Topic base**: Base MQTT topic (default: `victron`)
- **Publish interval**: How often to publish data in seconds (default: `5`)
- **Username/Password**: MQTT credentials (leave empty if not required)

#### Victron Devices (up to 6)
For each device:
- **Enabled**: Check to activate monitoring
- **Name**: The device's MQTT topic segment. The data is published to
  `victron/<name>/state`. Constraints: lowercase letters, digits and underscore
  only (`[a-z0-9_]`, up to 15 chars). Anything else is stripped on save.
  - **Back-compat:** leave the first SmartSolar MPPT named `mppt`, the SmartShunt
    `smartshunt`, and the SmartBatterySense `battery_sense` to keep the exact
    legacy topics. If you add a second/third MPPT and leave its name blank, it is
    auto-named `mppt2`, `mppt3`, … Two enabled devices can never share a name —
    the portal disambiguates duplicates automatically.
- **Type**: Select device type (SmartSolar MPPT, SmartShunt, or SmartBatterySense).
  The payload schema is chosen by **type**, independent of the name.
- **MAC Address**: 12 hex characters without colons (e.g., `c15639b47db5`).
  Routing is done by MAC, so several devices of the same type stay distinct.
- **Encryption Key**: 32 hex characters

4. Click **"Salva e Riavvia"**
5. The device saves the configuration and reboots into normal mode

## Getting MAC Address and Encryption Key

For each Victron device:

1. Open **VictronConnect** app on your phone
2. Connect to the device
3. Go to **Settings** (gear icon) > **Product Info**
4. Enable **"Instant readout via Bluetooth"**
5. Tap **"SHOW"** next to "Instant Readout Details"
6. Note the **Encryption Key** (32 hex characters)
7. The **MAC address** is shown in the device info (remove the colons)

### Prerequisites on Victron devices
- Firmware version **3.61 or newer**
- "Instant readout via Bluetooth" must be **enabled**

## Re-entering Setup Mode

To change configuration after initial setup:

1. **Reboot** the M5StickC (press reset or power cycle)
2. When you see `"Tieni B 3s = Setup"` on the display, **hold Button B** (side button) **for 3 seconds**
3. The device enters setup mode with the previous configuration pre-filled

## Factory Reset

To completely clear the configuration, enter setup mode and submit an empty form, or re-flash the firmware.

## Display in Normal Mode (M5StickC Plus, 135×240)

The display is **paged** and multi-device aware. **Button A** cycles through the
pages:

| Page | Content |
|---|---|
| **0 — Summary** | Header `Victron GW` + one compact line per present device: name + key metric (MPPT → `<n>W`, SmartShunt → `SoC <n>%`, BatterySense → `<n>C`). Scales cleanly from 1 to 6 devices. |
| **1..N — Detail** | One page per present device. Large header with the device **name** (so it is always clear which device is shown), then type-specific readouts: MPPT → PV power (large), V/I, yield, charge-state, RSSI; SmartShunt → SoC (large), V, signed current (green=charge/red=drain), TTG, RSSI; BatterySense → temperature (large), V, RSSI. |

- A small `page x/N` footer at the bottom makes paging discoverable.
- A device whose data is stale (no update for >60 s, or never seen) is **dimmed**.
- A small circle in the top-right corner indicates connection status:
  - Green = WiFi + MQTT connected
  - Yellow = WiFi connected, MQTT disconnected
  - Red = WiFi disconnected

## Buttons in Normal Mode

- **Button A** (front): Cycle display page (summary → device 1 → … → device N → summary)
- **Button B** (side): Rotate display orientation
