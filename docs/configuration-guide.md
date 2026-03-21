# Configuration Guide

## First-Time Setup

1. **Flash the firmware** to your M5StickC via USB-C
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

#### Victron Devices (up to 3)
For each device:
- **Enabled**: Check to activate monitoring
- **Name**: Friendly name (e.g., "SmartSolar")
- **Type**: Select device type (SmartSolar MPPT, SmartShunt, or SmartBatterySense)
- **MAC Address**: 12 hex characters without colons (e.g., `c15639b47db5`)
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

## Display in Normal Mode

The display shows 3 rows of power data:

| Row | Color | Value | Source |
|---|---|---|---|
| **PV** | Cyan | Solar power (W) | MPPT `pv_power` |
| **BT** | Green/Red | Battery power (W) | SmartShunt `voltage * current` |
| **LD** | Yellow | Consumption (W) | `pv_power - battery_power` |

- **Green** battery = charging, **Red** = discharging
- A small circle in the top-right corner indicates connection status:
  - Green = WiFi + MQTT connected
  - Yellow = WiFi connected, MQTT disconnected
  - Red = WiFi disconnected

## Buttons in Normal Mode

- **Button A** (front): Change display page
- **Button B** (side): Rotate display orientation
