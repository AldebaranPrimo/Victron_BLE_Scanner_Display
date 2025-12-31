# Victron BLE Multi-Device Scanner Display

Display data from multiple Victron devices on M5StickC Plus via BLE.

## Fork Information

This project is a fork of the original work by [@hoberman](https://github.com/hoberman):
- **Original Repository**: [https://github.com/hoberman/Victron_BLE_Scanner_Display](https://github.com/hoberman/Victron_BLE_Scanner_Display)

The fork was created to extend functionality for multi-device monitoring of a complete Victron solar system.

## What's New in This Fork

### Multi-Device Support
The original code only supported SmartSolar MPPT devices. This fork adds support for:
- **SmartSolar MPPT** - Solar charger with voltage, current, power, yield, and charge state
- **Smart Battery Sense** - Battery temperature sensor
- **Smart Shunt** - Battery monitor with SOC, current, voltage, and time-to-go

### Migration to PlatformIO
The project has been migrated from Arduino IDE to **PlatformIO** for better dependency management and build system:
- Moved source code from `.ino` files to `src/main.cpp`
- Added `platformio.ini` configuration for M5StickC Plus
- Automatic library management (M5StickCPlus, ESP32 BLE Arduino)
- Consistent build environment across different systems

### Display Pages
Two display pages accessible via button press:
1. **SOLAR Page** - Shows SmartSolar data: voltage, current, power (W), daily yield (Wh), charge state
2. **INFO Page** - Shows Battery Sense temperature and Smart Shunt data (SOC, current, TTG)

### Code Improvements
- Restructured data processing with separate handlers for each device type
- Added proper Victron BLE protocol parsing for Battery Monitor format (record type 0x02)
- Temperature conversion from Kelvin to Celsius for Battery Sense
- Bit-packed data extraction for Smart Shunt fields
- Cleaner device configuration structure

## Hardware Requirements

- **M5StickC Plus** (recommended) or M5StickC
- Victron devices with BLE Instant Readout enabled:
  - SmartSolar MPPT
  - Smart Battery Sense (optional)
  - Smart Shunt (optional)

## Configuration

Edit the device configuration in `src/main.cpp`:

```cpp
struct victronDevice victronDevices[] = {
  // SmartSolar MPPT
  { "YOUR_MAC_HERE", "YOUR_KEY_HERE", "SmartSolar", DEVICE_SOLAR_CHARGER, {0}, {0}, "(unknown)" },
  
  // Smart Shunt
  { "YOUR_MAC_HERE", "YOUR_KEY_HERE", "SmartShunt", DEVICE_SMART_SHUNT, {0}, {0}, "(unknown)" },
  
  // Battery Smart Sense
  { "YOUR_MAC_HERE", "YOUR_KEY_HERE", "BattSense", DEVICE_BATTERY_SENSE, {0}, {0}, "(unknown)" },
};
```

Get MAC address and Encryption Key from VictronConnect app:
**Device Menu → Settings → Product Info → Bluetooth Instant Readout**

## Building with PlatformIO

1. Install [PlatformIO](https://platformio.org/)
2. Clone this repository
3. Open in VS Code with PlatformIO extension
4. Edit device configuration with your MAC/Keys
5. Build: `Ctrl+Alt+B`
6. Upload: `Ctrl+Alt+U`

## Usage

- **Button A (front)**: Change display page (SOLAR ↔ INFO)
- **Button B (side)**: Rotate display

## Credits

- Original project by [@hoberman](https://github.com/hoberman)
- Victron BLE protocol documentation from [keshavdv/victron-ble](https://github.com/keshavdv/victron-ble)
- M5StickCPlus library by [M5Stack](https://github.com/m5stack)

## License

This project maintains the same license as the original repository.
