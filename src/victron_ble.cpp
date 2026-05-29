// ============================================================================
// victron_ble.cpp — BLE scan, AES-CTR decrypt, per-slot record parsing
//
// N-device routing model:
//   - rtDevices[] and devStates[] are BOTH parallel to GatewayConfig.devices[]:
//     index i in all three refers to the same configured slot. There is NO
//     compaction — disabled slots are simply skipped (gated by `enabled`).
//   - Each incoming advertisement is matched to a slot BY MAC (findDeviceByMac),
//     never by type, so multiple devices of the same type stay distinct.
//   - The matched slot index `devIdx` is the single source of truth used to
//     write devStates[devIdx] and (later, in mqtt_publisher) to derive the
//     topic from devStates[devIdx].name.
//
// The parsing math is kept byte-for-byte identical to the legacy single-device
// code so the published JSON payloads remain unchanged (back-compat guarantee).
// ============================================================================

#include "victron_ble.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <aes/esp_aes.h>

// ============================================================================
// Global data
// ============================================================================
// Per-slot runtime state, indexed by config slot (see victron_ble.h).
DeviceRuntimeState devStates[MAX_DEVICES] = {};
volatile bool bleNewData = false;

// ============================================================================
// Runtime devices
//
// rtDevices is parallel to GatewayConfig.devices[] (NOT compacted): rtDevices[i]
// is the runtime form of config slot i. Sparse — only `enabled` entries are
// populated; the rest stay enabled=false.
// ============================================================================
static VictronRuntimeDevice rtDevices[MAX_DEVICES];
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

// Returns the config-slot index of the enabled device whose MAC matches, or -1.
// Iterates the full slot range because slots are now sparse (not compacted).
static int findDeviceByMac(byte* mac) {
    for (int i = 0; i < MAX_DEVICES; i++) {
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
// Parse a SmartSolar charger record into the matched slot's solar state.
// Math is unchanged from the legacy single-device path; only the write target
// moved from the `solarData` singleton to devStates[devIdx].data.solar.
static void processSolarCharger(byte* data, int devIdx, int rssi, const char* devName) {
    victronSolarData* solar = (victronSolarData*)data;

    byte unusedBits = solar->outputCurrentHi & 0xfe;
    if (unusedBits != 0xfe) return;

    SolarDisplayData& s = devStates[devIdx].data.solar;
    s.valid = true;
    s.batteryVoltage = float(solar->batteryVoltage) * 0.01f;
    s.batteryCurrent = float(solar->batteryCurrent) * 0.1f;
    s.todayYield = float(solar->todayYield) * 0.01f * 1000.0f;
    s.inputPower = solar->inputPower;
    s.chargeState = solar->deviceState;
    s.errorCode = solar->errorCode;
    s.rssi = rssi;

    int outputCurrentInt = ((solar->outputCurrentHi & 0x01) << 9) | solar->outputCurrentLo;
    s.loadCurrent = float(outputCurrentInt) * 0.1f;

    strncpy(s.deviceName, devName, 31);
    devStates[devIdx].lastUpdateMs = millis();

    Serial.printf("[SOLAR slot%d] %s | %.2fV %.1fA | %dW | Yield:%.0fWh | RSSI:%d\n",
        devIdx, devName, s.batteryVoltage, s.batteryCurrent,
        s.inputPower, s.todayYield, rssi);
}

// Parse a SmartShunt battery-monitor record into the matched slot's shunt state.
// NOTE: consumed_ah is not present in this BLE record; it is left at 0.0 from the
// zero-initialized state, exactly as the legacy code did, to preserve byte-for-
// byte payload back-compat.
static void processSmartShunt(byte* data, int devIdx, int rssi, const char* devName) {
    victronBatteryMonitorData* mon = (victronBatteryMonitorData*)data;

    ShuntDisplayData& s = devStates[devIdx].data.shunt;
    s.valid = true;
    s.batteryVoltage = float(mon->batteryVoltage) * 0.01f;
    s.ttg = mon->ttg;
    s.rssi = rssi;

    // Current: 22 bit signed, starts at bit 2 of packedData
    int32_t currentRaw = ((mon->packedData[0] >> 2) |
                          (mon->packedData[1] << 6) |
                          (mon->packedData[2] << 14)) & 0x3FFFFF;
    if (currentRaw & 0x200000) currentRaw |= 0xFFC00000;
    s.batteryCurrent = float(currentRaw) * 0.001f;

    // SOC: 10 bit
    uint16_t socRaw = ((mon->packedData[5] >> 4) | (mon->packedData[6] << 4)) & 0x3FF;
    if (socRaw != 0x3FF) {
        s.soc = float(socRaw) * 0.1f;
    }

    strncpy(s.deviceName, devName, 31);
    devStates[devIdx].lastUpdateMs = millis();

    Serial.printf("[SHUNT slot%d] %s | %.2fV %.2fA | SOC:%.1f%% | TTG:%dmin | RSSI:%d\n",
        devIdx, devName, s.batteryVoltage, s.batteryCurrent,
        s.soc, s.ttg, rssi);
}

// Parse a SmartBatterySense record into the matched slot's bsense state.
// Same record layout as the shunt (battery monitor 0x02) but only voltage and
// temperature are meaningful. Math unchanged from the legacy path.
static void processBatterySense(byte* data, int devIdx, int rssi, const char* devName) {
    victronBatteryMonitorData* mon = (victronBatteryMonitorData*)data;

    BatterySenseDisplayData& s = devStates[devIdx].data.bsense;
    s.valid = true;
    s.batteryVoltage = float(mon->batteryVoltage) * 0.01f;
    s.rssi = rssi;

    float tempKelvin = float(mon->auxValue) * 0.01f;
    s.temperature = tempKelvin - 273.15f;

    strncpy(s.deviceName, devName, 31);
    devStates[devIdx].lastUpdateMs = millis();

    Serial.printf("[BSENSE slot%d] %s | %.2fV | Temp:%.1fC | RSSI:%d\n",
        devIdx, devName, s.batteryVoltage, s.temperature, rssi);
}

// ============================================================================
// BLE Callback
// ============================================================================
class VictronBLECallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (!advertisedDevice.haveManufacturerData()) return;

        uint8_t buf[32];
        // arduino-esp32 3.x: getManufacturerData() returns an Arduino String
        // (it returned std::string in 2.x). Copy by explicit length because the
        // manufacturer data is binary and may contain embedded NUL bytes.
        String manData = advertisedDevice.getManufacturerData();
        int dataSize = manData.length();
        if (dataSize > 31) dataSize = 31;
        memcpy(buf, manData.c_str(), dataSize);

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
// Initialize runtime devices + per-slot state from config, WITHOUT compaction:
// slot i of rtDevices/devStates maps 1:1 to cfg.devices[i]. This keeps the
// config index, runtime index, state index, and topic name all on the same
// index, removing index-aliasing bugs and letting topics derive from the slot.
void bleInit(const GatewayConfig& cfg) {
    int configured = 0;

    for (int i = 0; i < MAX_DEVICES; i++) {
        devStates[i] = {};            // clear per-slot state
        rtDevices[i].enabled = false; // default the slot to inactive

        const DeviceConfig& dc = cfg.devices[i];
        if (!dc.enabled) continue;
        if (strlen(dc.mac) != 12) continue;
        if (strlen(dc.aesKey) != 32) continue;

        VictronRuntimeDevice& dev = rtDevices[i]; // SAME index as config slot
        dev.enabled = true;
        dev.type = dc.type;
        strncpy(dev.name, dc.name, sizeof(dev.name) - 1);
        hexStrToBytes(dc.mac, dev.macBytes, 6);
        hexStrToBytes(dc.aesKey, dev.keyBytes, 16);

        // Initialize the parallel runtime-state slot
        devStates[i].inUse = true;
        devStates[i].type = dc.type;
        strncpy(devStates[i].name, dc.name, sizeof(devStates[i].name) - 1);
        devStates[i].lastUpdateMs = 0;

        Serial.printf("[BLE] Slot %d: %s type=%d MAC:", i, dev.name, dev.type);
        for (int j = 0; j < 6; j++) Serial.printf("%02x", dev.macBytes[j]);
        Serial.println();

        configured++;
    }

    Serial.printf("[BLE] %d devices configured\n", configured);

    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new VictronBLECallback());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}

void bleScan() {
    if (!pBLEScan) return;
    // arduino-esp32 3.x: BLEScan::start() returns BLEScanResults* (it returned a
    // value in 2.x). We don't use the aggregated result here — each advertisement
    // is handled in the callback — so the return value is ignored.
    pBLEScan->start(1, false);
    pBLEScan->clearResults();
}

// Number of active (enabled & configured) device slots.
int bleDeviceCount() {
    int n = 0;
    for (int i = 0; i < MAX_DEVICES; i++) if (rtDevices[i].enabled) n++;
    return n;
}

// Fills outSlots with the indices of all in-use slots (in slot order) and
// returns how many were written, capped at maxOut. Lets callers (e.g. the
// display) iterate present devices without walking over disabled gaps.
int bleEnabledSlots(int* outSlots, int maxOut) {
    int n = 0;
    for (int i = 0; i < MAX_DEVICES && n < maxOut; i++) {
        if (devStates[i].inUse) outSlots[n++] = i;
    }
    return n;
}
