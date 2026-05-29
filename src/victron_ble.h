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
//
// Parallel to GatewayConfig.devices[]: rtDevices[i] corresponds to config slot
// i. Holds the decoded MAC/key bytes used for BLE matching and AES decryption.
// ============================================================================
struct VictronRuntimeDevice {
    byte macBytes[6];
    byte keyBytes[16];
    uint8_t type;
    char name[16];
    bool enabled;
};

// ============================================================================
// Per-slot runtime state
//
// One DeviceRuntimeState per configured device slot, indexed by the SAME slot
// index as GatewayConfig.devices[] and rtDevices[] (0..MAX_DEVICES-1). This is
// the core of the N-device refactor: each slot owns its own parsed payload, so
// two devices of the same type never collide. `type` selects which member of
// the union is meaningful. `name` is copied from config and becomes the MQTT
// topic segment. `lastUpdateMs` is the millis() of the last successful parse
// (0 = never seen) and drives display staleness dimming.
//
// The union keeps RAM flat: 6 slots * (largest variant + header) stays well
// under 0.5 KB.
// ============================================================================
struct DeviceRuntimeState {
    bool      inUse;          // slot is enabled & configured
    uint8_t   type;           // DEVICE_TYPE_SOLAR / _SHUNT / _BSENSE
    char      name[16];       // copied from config; becomes MQTT topic segment
    uint32_t  lastUpdateMs;   // millis() of last successful parse (0 = never)
    union {
        SolarDisplayData        solar;
        ShuntDisplayData        shunt;
        BatterySenseDisplayData bsense;
    } data;
};

// Global per-slot state array, indexed by config slot (0..MAX_DEVICES-1).
extern DeviceRuntimeState devStates[MAX_DEVICES];
extern volatile bool bleNewData;

// Functions
void bleInit(const GatewayConfig& cfg);
void bleScan();
int  bleDeviceCount();                            // count of inUse / enabled slots
int  bleEnabledSlots(int* outSlots, int maxOut);  // fills outSlots with inUse slot
                                                  // indices, returns the count

#endif
