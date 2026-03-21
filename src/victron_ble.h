#ifndef VICTRON_BLE_H
#define VICTRON_BLE_H

#include <Arduino.h>
#include "config_manager.h"

// Victron Record Types
#define VICTRON_TYPE_SOLAR_CHARGER    0x01
#define VICTRON_TYPE_BATTERY_MONITOR  0x02

// Device types (matches DeviceConfig.type)
#define DEVICE_TYPE_SOLAR   0
#define DEVICE_TYPE_SHUNT   1
#define DEVICE_TYPE_BSENSE  2

// ============================================================================
// Victron protocol structures
// ============================================================================
typedef struct {
    uint8_t deviceState;
    uint8_t errorCode;
    int16_t batteryVoltage;
    int16_t batteryCurrent;
    uint16_t todayYield;
    uint16_t inputPower;
    uint8_t outputCurrentLo;
    uint8_t outputCurrentHi;
    uint8_t unused[4];
} __attribute__((packed)) victronSolarData;

typedef struct {
    uint16_t ttg;
    int16_t batteryVoltage;
    uint16_t alarm;
    int16_t auxValue;
    uint8_t packedData[7];
} __attribute__((packed)) victronBatteryMonitorData;

typedef struct {
    uint16_t vendorID;
    uint8_t beaconType;
    uint16_t productID;
    uint8_t dataCounter;
    uint8_t victronRecordType;
    uint16_t nonceDataCounter;
    uint8_t encryptKeyMatch;
    uint8_t victronEncryptedData[21];
    uint8_t nullPad;
} __attribute__((packed)) victronManufacturerData;

// ============================================================================
// Display data structures (shared with display_ui and mqtt_publisher)
// ============================================================================
struct SolarDisplayData {
    bool valid;
    float batteryVoltage;
    float batteryCurrent;
    float todayYield;
    uint16_t inputPower;
    float loadCurrent;
    uint8_t chargeState;
    uint8_t errorCode;
    int rssi;
    char deviceName[32];
};

struct ShuntDisplayData {
    bool valid;
    float batteryVoltage;
    float batteryCurrent;
    float soc;
    float consumedAh;
    uint16_t ttg;
    int rssi;
    char deviceName[32];
};

struct BatterySenseDisplayData {
    bool valid;
    float batteryVoltage;
    float temperature;
    int rssi;
    char deviceName[32];
};

// ============================================================================
// Runtime device info (populated from config)
// ============================================================================
struct VictronRuntimeDevice {
    byte macBytes[6];
    byte keyBytes[16];
    uint8_t type;
    char name[16];
    bool enabled;
};

// Global data (extern)
extern SolarDisplayData solarData;
extern ShuntDisplayData shuntData;
extern BatterySenseDisplayData batterySenseData;
extern volatile bool bleNewData;

// Functions
void bleInit(const GatewayConfig& cfg);
void bleScan();
int bleDeviceCount();

#endif
