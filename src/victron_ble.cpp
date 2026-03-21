#include "victron_ble.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <aes/esp_aes.h>

// ============================================================================
// Global data
// ============================================================================
SolarDisplayData solarData = {};
ShuntDisplayData shuntData = {};
BatterySenseDisplayData batterySenseData = {};
volatile bool bleNewData = false;

// ============================================================================
// Runtime devices
// ============================================================================
static VictronRuntimeDevice rtDevices[MAX_DEVICES];
static int rtDeviceCount = 0;
static BLEScan* pBLEScan = nullptr;

// ============================================================================
// Helpers
// ============================================================================
static byte hexCharToByte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void hexStrToBytes(const char* hex, byte* out, int outLen) {
    for (int i = 0; i < outLen; i++) {
        out[i] = (hexCharToByte(hex[i * 2]) << 4) | hexCharToByte(hex[i * 2 + 1]);
    }
}

static int findDeviceByMac(byte* mac) {
    for (int i = 0; i < rtDeviceCount; i++) {
        if (!rtDevices[i].enabled) continue;
        bool match = true;
        for (int j = 0; j < 6; j++) {
            if (mac[j] != rtDevices[i].macBytes[j]) { match = false; break; }
        }
        if (match) return i;
    }
    return -1;
}

static bool decryptData(victronManufacturerData* vicData, int devIdx, byte* out, int dataSize) {
    byte input[16];
    for (int i = 0; i < dataSize && i < 16; i++) {
        input[i] = vicData->victronEncryptedData[i];
    }

    esp_aes_context ctx;
    esp_aes_init(&ctx);
    if (esp_aes_setkey(&ctx, rtDevices[devIdx].keyBytes, 128) != 0) {
        esp_aes_free(&ctx);
        return false;
    }

    byte nonce_counter[16] = {0};
    nonce_counter[0] = vicData->nonceDataCounter & 0xFF;
    nonce_counter[1] = (vicData->nonceDataCounter >> 8) & 0xFF;
    uint8_t stream_block[16] = {0};
    size_t nonce_offset = 0;

    int status = esp_aes_crypt_ctr(&ctx, dataSize, &nonce_offset, nonce_counter, stream_block, input, out);
    esp_aes_free(&ctx);
    return (status == 0);
}

// ============================================================================
// Processors
// ============================================================================
static void processSolarCharger(byte* data, int devIdx, int rssi, const char* devName) {
    victronSolarData* solar = (victronSolarData*)data;

    byte unusedBits = solar->outputCurrentHi & 0xfe;
    if (unusedBits != 0xfe) return;

    solarData.valid = true;
    solarData.batteryVoltage = float(solar->batteryVoltage) * 0.01f;
    solarData.batteryCurrent = float(solar->batteryCurrent) * 0.1f;
    solarData.todayYield = float(solar->todayYield) * 0.01f * 1000.0f;
    solarData.inputPower = solar->inputPower;
    solarData.chargeState = solar->deviceState;
    solarData.errorCode = solar->errorCode;
    solarData.rssi = rssi;

    int outputCurrentInt = ((solar->outputCurrentHi & 0x01) << 9) | solar->outputCurrentLo;
    solarData.loadCurrent = float(outputCurrentInt) * 0.1f;

    strncpy(solarData.deviceName, devName, 31);

    Serial.printf("[SOLAR] %s | %.2fV %.1fA | %dW | Yield:%.0fWh | RSSI:%d\n",
        devName, solarData.batteryVoltage, solarData.batteryCurrent,
        solarData.inputPower, solarData.todayYield, rssi);
}

static void processSmartShunt(byte* data, int devIdx, int rssi, const char* devName) {
    victronBatteryMonitorData* mon = (victronBatteryMonitorData*)data;

    shuntData.valid = true;
    shuntData.batteryVoltage = float(mon->batteryVoltage) * 0.01f;
    shuntData.ttg = mon->ttg;
    shuntData.rssi = rssi;

    // Current: 22 bit signed, starts at bit 2 of packedData
    int32_t currentRaw = ((mon->packedData[0] >> 2) |
                          (mon->packedData[1] << 6) |
                          (mon->packedData[2] << 14)) & 0x3FFFFF;
    if (currentRaw & 0x200000) currentRaw |= 0xFFC00000;
    shuntData.batteryCurrent = float(currentRaw) * 0.001f;

    // SOC: 10 bit
    uint16_t socRaw = ((mon->packedData[5] >> 4) | (mon->packedData[6] << 4)) & 0x3FF;
    if (socRaw != 0x3FF) {
        shuntData.soc = float(socRaw) * 0.1f;
    }

    strncpy(shuntData.deviceName, devName, 31);

    Serial.printf("[SHUNT] %s | %.2fV %.2fA | SOC:%.1f%% | TTG:%dmin | RSSI:%d\n",
        devName, shuntData.batteryVoltage, shuntData.batteryCurrent,
        shuntData.soc, shuntData.ttg, rssi);
}

static void processBatterySense(byte* data, int devIdx, int rssi, const char* devName) {
    victronBatteryMonitorData* mon = (victronBatteryMonitorData*)data;

    uint8_t auxInput = mon->packedData[0] & 0x03;

    batterySenseData.valid = true;
    batterySenseData.batteryVoltage = float(mon->batteryVoltage) * 0.01f;
    batterySenseData.rssi = rssi;

    float tempKelvin = float(mon->auxValue) * 0.01f;
    batterySenseData.temperature = tempKelvin - 273.15f;

    strncpy(batterySenseData.deviceName, devName, 31);

    Serial.printf("[BSENSE] %s | %.2fV | Temp:%.1fC | RSSI:%d\n",
        devName, batterySenseData.batteryVoltage, batterySenseData.temperature, rssi);
}

// ============================================================================
// BLE Callback
// ============================================================================
class VictronBLECallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (!advertisedDevice.haveManufacturerData()) return;

        uint8_t buf[32];
        std::string manData = advertisedDevice.getManufacturerData();
        int dataSize = manData.length();
        if (dataSize > 31) dataSize = 31;
        manData.copy((char*)buf, dataSize);

        victronManufacturerData* vicData = (victronManufacturerData*)buf;
        if (vicData->vendorID != 0x02E1) return;

        // Get MAC and find device
        char macStr[18];
        strcpy(macStr, advertisedDevice.getAddress().toString().c_str());
        byte macByte[6];
        hexStrToBytes(macStr, macByte, 6);

        // The toString() returns "xx:xx:xx:xx:xx:xx" - we need to strip colons
        // Actually let's parse the raw mac properly
        byte macParsed[6];
        int mi = 0;
        for (int i = 0; macStr[i] && mi < 6; i++) {
            if (macStr[i] == ':') continue;
            if (i + 1 < 18 && macStr[i+1] != ':' && macStr[i+1] != '\0') {
                macParsed[mi++] = (hexCharToByte(macStr[i]) << 4) | hexCharToByte(macStr[i+1]);
                i++; // skip next char
            }
        }

        int devIdx = findDeviceByMac(macParsed);
        if (devIdx == -1) return;

        // Verify encryption key first byte
        if (vicData->encryptKeyMatch != rtDevices[devIdx].keyBytes[0]) return;

        // Decrypt
        byte outputData[16] = {0};
        int encrDataSize = dataSize - 10;
        if (!decryptData(vicData, devIdx, outputData, encrDataSize)) return;

        int rssi = advertisedDevice.getRSSI();

        // Get device name
        char devName[32];
        strncpy(devName, rtDevices[devIdx].name, 31);
        if (advertisedDevice.haveName()) {
            strncpy(devName, advertisedDevice.getName().c_str(), 31);
        }

        // Process based on configured type
        switch (rtDevices[devIdx].type) {
            case DEVICE_TYPE_SOLAR:
                if (vicData->victronRecordType == VICTRON_TYPE_SOLAR_CHARGER) {
                    processSolarCharger(outputData, devIdx, rssi, devName);
                    bleNewData = true;
                }
                break;
            case DEVICE_TYPE_SHUNT:
                if (vicData->victronRecordType == VICTRON_TYPE_BATTERY_MONITOR) {
                    processSmartShunt(outputData, devIdx, rssi, devName);
                    bleNewData = true;
                }
                break;
            case DEVICE_TYPE_BSENSE:
                processBatterySense(outputData, devIdx, rssi, devName);
                bleNewData = true;
                break;
        }
    }
};

// ============================================================================
// Public API
// ============================================================================
void bleInit(const GatewayConfig& cfg) {
    rtDeviceCount = 0;

    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!cfg.devices[i].enabled) continue;
        if (strlen(cfg.devices[i].mac) != 12) continue;
        if (strlen(cfg.devices[i].aesKey) != 32) continue;

        VictronRuntimeDevice& dev = rtDevices[rtDeviceCount];
        dev.enabled = true;
        dev.type = cfg.devices[i].type;
        strncpy(dev.name, cfg.devices[i].name, sizeof(dev.name) - 1);
        hexStrToBytes(cfg.devices[i].mac, dev.macBytes, 6);
        hexStrToBytes(cfg.devices[i].aesKey, dev.keyBytes, 16);

        Serial.printf("[BLE] Device %d: %s type=%d MAC:", rtDeviceCount, dev.name, dev.type);
        for (int j = 0; j < 6; j++) Serial.printf("%02x", dev.macBytes[j]);
        Serial.println();

        rtDeviceCount++;
    }

    Serial.printf("[BLE] %d devices configured\n", rtDeviceCount);

    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new VictronBLECallback());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}

void bleScan() {
    if (!pBLEScan) return;
    BLEScanResults foundDevices = pBLEScan->start(1, false);
    pBLEScan->clearResults();
}

int bleDeviceCount() {
    return rtDeviceCount;
}
