/*
  Victron BLE Scanner Display - Multi-Device Version
  
  Supports:
  - SmartSolar MPPT (Solar Charger)
  - Smart Shunt (Battery Monitor) - SOC, Current, Consumed Ah
  - Smart Battery Sense - Temperature
  
  Based on Victron BLE Advertising protocol:
    https://community.victronenergy.com/storage/attachments/48745-extra-manufacturer-data-2022-12-14.pdf
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <aes/esp_aes.h>

// Board selection
#if defined M5STICKC
  #include <M5StickC.h>
#endif

#if defined M5STICKCPLUS
  #include <M5StickCPlus.h>
#endif

#if defined M5STICKC || defined M5STICKCPLUS
  M5Display display;

  #define COLOR_BACKGROUND              TFT_BLACK
  #define COLOR_TEXT                    TFT_DARKGREEN
  #define COLOR_NEGATIVE                TFT_MAROON
  #define COLOR_UNKNOWN                 TFT_DARKGREY
  #define COLOR_CHARGEROFF              TFT_MAROON
  #define COLOR_BULK                    ((0x00 << 11) + (0x00 << 5) + 0x18)
  #define COLOR_ABSORPTION              ((0x0f << 11) + (0x1f << 5) + 0x00)
  #define COLOR_FLOAT                   TFT_DARKGREEN
  #define COLOR_EQUALIZATION            ((0x15 << 11) + (0x15 << 5) + 0x00)
  #define COLOR_TITLE                   TFT_CYAN
  #define COLOR_SOC_HIGH                TFT_GREEN
  #define COLOR_SOC_MED                 TFT_YELLOW
  #define COLOR_SOC_LOW                 TFT_RED
  #define COLOR_TEMP                    TFT_ORANGE

  #define BUTTON_1 37
  #define BUTTON_2 39  // Side button on M5StickC Plus
#endif

BLEScan *pBLEScan;

#define AES_KEY_BITS 128
int scanTime = 1;

// ============================================================================
// Victron Record Types
// ============================================================================
#define VICTRON_TYPE_SOLAR_CHARGER    0x01
#define VICTRON_TYPE_BATTERY_MONITOR  0x02  // Smart Shunt
#define VICTRON_TYPE_INVERTER         0x03
#define VICTRON_TYPE_DCDC_CONVERTER   0x04
#define VICTRON_TYPE_SMART_LITHIUM    0x05

// ============================================================================
// Data Structures for Different Device Types
// ============================================================================

// Solar Charger (SmartSolar MPPT)
typedef struct {
  uint8_t deviceState;
  uint8_t errorCode;
  int16_t batteryVoltage;    // 0.01V
  int16_t batteryCurrent;    // 0.1A
  uint16_t todayYield;       // 0.01kWh
  uint16_t inputPower;       // W
  uint8_t outputCurrentLo;
  uint8_t outputCurrentHi;
  uint8_t unused[4];
} __attribute__((packed)) victronSolarData;

// Battery Monitor (Smart Shunt AND Smart Battery Sense - record type 0x02)
// Smart Battery Sense ha product_id 0xA3A4 o 0xA3A5
typedef struct {
  uint16_t ttg;              // Time to go in minutes (0xFFFF = N/A)
  int16_t batteryVoltage;    // 0.01V
  uint16_t alarm;            // Alarm reason
  int16_t auxValue;          // Temperature in Kelvin*100 (if aux_input=2) or aux voltage
  // Packed bits: aux_input(2) + current(22) + consumed_ah(20) + soc(10) = 54 bits
  uint8_t packedData[7];     // Remaining packed data
} __attribute__((packed)) victronBatteryMonitorData;

// Product IDs for Battery Sense
#define PRODUCT_ID_BATTERY_SENSE_1  0xA3A4
#define PRODUCT_ID_BATTERY_SENSE_2  0xA3A5
#define IS_BATTERY_SENSE(pid) ((pid) == PRODUCT_ID_BATTERY_SENSE_1 || (pid) == PRODUCT_ID_BATTERY_SENSE_2)

// Manufacturer data header (common to all Victron devices)
typedef struct {
  uint16_t vendorID;         // 0x02E1 = Victron
  uint8_t beaconType;        // 0x10 = Product Advertisement
  uint16_t productID;        // Product identifier (e.g., 0xA3A4 = Battery Sense)
  uint8_t dataCounter;       // Record counter
  uint8_t victronRecordType; // 0x01=Solar, 0x02=Battery Monitor, etc.
  uint16_t nonceDataCounter; // Nonce for decryption
  uint8_t encryptKeyMatch;   // Should match first byte of encryption key
  uint8_t victronEncryptedData[21];
  uint8_t nullPad;
} __attribute__((packed)) victronManufacturerData;

// ============================================================================
// Device Types for Configuration
// ============================================================================
enum VictronDeviceType {
  DEVICE_SOLAR_CHARGER,
  DEVICE_SMART_SHUNT,
  DEVICE_BATTERY_SENSE
};

struct victronDevice {
  char charMacAddr[13];
  char charKey[33];
  char comment[16];
  VictronDeviceType deviceType;
  byte byteMacAddr[6];
  byte byteKey[16];
  char cachedDeviceName[32];
};

// ============================================================================
// CONFIGURAZIONE DEI TUOI DISPOSITIVI VICTRON - MODIFICA QUI
// ============================================================================
// Per ogni dispositivo, ottieni MAC e Encryption Key da VictronConnect:
// Menu dispositivo → Impostazioni → Info prodotto → Bluetooth Instant Readout
// ============================================================================

struct victronDevice victronDevices[] = {
  // SmartSolar MPPT - GIA' CONFIGURATO
  { "c15639b47db5", "f2dcc3ba40edb8de7e07d7638f13f971", "SmartSolar", DEVICE_SOLAR_CHARGER, {0}, {0}, "(unknown)" },
  
  // Smart Shunt - CONFIGURATO
  { "f93ccf0c1b2e", "4c1e3ccd3d892db13d7a43740b7f1021", "SmartShunt", DEVICE_SMART_SHUNT, {0}, {0}, "(unknown)" },
  
  // Battery Smart Sense - CONFIGURATO
  { "c1b691bd9e2b", "b7abe19c003240be9dae89b8c372dd43", "BattSense", DEVICE_BATTERY_SENSE, {0}, {0}, "(unknown)" },
};

// ============================================================================

int victronDeviceCount = sizeof(victronDevices) / sizeof(victronDevices[0]);

// Global data storage for display
struct {
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
} solarData = {false};

struct {
  bool valid;
  float batteryVoltage;
  float batteryCurrent;
  float soc;              // State of Charge %
  float consumedAh;
  uint16_t ttg;           // Time to go (minutes)
  int rssi;
  char deviceName[32];
} shuntData = {false};

struct {
  bool valid;
  float batteryVoltage;
  float temperature;      // °C
  int rssi;
  char deviceName[32];
} batterySenseData = {false};

// Display state
time_t lastLEDBlinkTime = 0;
time_t lastTick = 0;
int displayRotation = 3;
bool packetReceived = false;
int displayPage = 0;  // 0=Solar, 1=Shunt/Overview

char chargeStateNames[][6] = {
  "  off", "   1?", "   2?", " bulk", "  abs", "float", "   6?", "equal"
};

#if defined M5STICKC || defined M5STICKCPLUS
  uint16_t chargeStateColors[] = {
    COLOR_CHARGEROFF, COLOR_UNKNOWN, COLOR_UNKNOWN, COLOR_BULK,
    COLOR_ABSORPTION, COLOR_FLOAT, COLOR_UNKNOWN, COLOR_EQUALIZATION
  };
#endif

// Function prototypes
void hexCharStrToByteArray(char * hexCharStr, byte * byteArray);
byte hexCharToByte(char hexChar);
int findDeviceByMac(byte* mac);
bool decryptVictronData(victronManufacturerData* vicData, int deviceIndex, byte* outputData, int dataSize);
void processSolarCharger(byte* data, int deviceIndex, int rssi, const char* deviceName);
void processSmartShunt(byte* data, int deviceIndex, int rssi, const char* deviceName);
void processBatterySense(byte* data, int deviceIndex, int rssi, const char* deviceName);
void updateDisplay();
float kelvinToCelsius(int16_t kelvin);

// ============================================================================
// Helper Functions
// ============================================================================
byte hexCharToByte(char hexChar) {
  if (hexChar >= '0' && hexChar <= '9') return hexChar - '0';
  if (hexChar >= 'a' && hexChar <= 'f') return hexChar - 'a' + 10;
  if (hexChar >= 'A' && hexChar <= 'F') return hexChar - 'A' + 10;
  return 255;
}

void hexCharStrToByteArray(char * hexCharStr, byte * byteArray) {
  int len = strlen(hexCharStr);
  int idx = 0;
  bool odd = true;
  byte hi;
  
  for (int i = 0; i < len; i++) {
    byte nibble = hexCharToByte(hexCharStr[i]);
    if (nibble != 255) {
      if (odd) {
        hi = nibble;
      } else {
        byteArray[idx++] = (hi << 4) | nibble;
      }
      odd = !odd;
    }
  }
}

float kelvinToCelsius(int16_t kelvinRaw) {
  // Temperature in 0.01K, convert to Celsius
  float kelvin = float(kelvinRaw) * 0.01;
  return kelvin - 273.15;
}

int findDeviceByMac(byte* mac) {
  for (int i = 0; i < victronDeviceCount; i++) {
    bool match = true;
    for (int j = 0; j < 6; j++) {
      if (mac[j] != victronDevices[i].byteMacAddr[j]) {
        match = false;
        break;
      }
    }
    if (match) return i;
  }
  return -1;
}

bool decryptVictronData(victronManufacturerData* vicData, int deviceIndex, byte* outputData, int dataSize) {
  byte inputData[16];
  
  for (int i = 0; i < dataSize && i < 16; i++) {
    inputData[i] = vicData->victronEncryptedData[i];
  }

  esp_aes_context ctx;
  esp_aes_init(&ctx);

  auto status = esp_aes_setkey(&ctx, victronDevices[deviceIndex].byteKey, AES_KEY_BITS);
  if (status != 0) {
    esp_aes_free(&ctx);
    return false;
  }

  byte data_counter_lsb = (vicData->nonceDataCounter) & 0xff;
  byte data_counter_msb = ((vicData->nonceDataCounter) >> 8) & 0xff;
  uint8_t nonce_counter[16] = {data_counter_lsb, data_counter_msb, 0};
  uint8_t stream_block[16] = {0};
  size_t nonce_offset = 0;

  status = esp_aes_crypt_ctr(&ctx, dataSize, &nonce_offset, nonce_counter, stream_block, inputData, outputData);
  esp_aes_free(&ctx);
  
  return (status == 0);
}

// ============================================================================
// Device Data Processors
// ============================================================================
void processSolarCharger(byte* data, int deviceIndex, int rssi, const char* deviceName) {
  victronSolarData* solar = (victronSolarData*)data;
  
  // Validate data
  byte unusedBits = solar->outputCurrentHi & 0xfe;
  if (unusedBits != 0xfe) return;
  
  solarData.valid = true;
  solarData.batteryVoltage = float(solar->batteryVoltage) * 0.01;
  solarData.batteryCurrent = float(solar->batteryCurrent) * 0.1;
  solarData.todayYield = float(solar->todayYield) * 0.01 * 1000;
  solarData.inputPower = solar->inputPower;
  solarData.chargeState = solar->deviceState;
  solarData.errorCode = solar->errorCode;
  solarData.rssi = rssi;
  
  int outputCurrentInt = ((solar->outputCurrentHi & 0x01) << 9) | solar->outputCurrentLo;
  solarData.loadCurrent = float(outputCurrentInt) * 0.1;
  
  strncpy(solarData.deviceName, deviceName, 31);
  
  char stateName[6];
  if (solar->deviceState <= 7) {
    strcpy(stateName, chargeStateNames[solar->deviceState]);
  } else {
    sprintf(stateName, "%d?", solar->deviceState);
  }
  
  Serial.printf("[SOLAR] %s | %.2fV %.1fA | %dW | Yield:%.0fWh | Load:%.1fA | %s | RSSI:%d\n",
    deviceName, solarData.batteryVoltage, solarData.batteryCurrent,
    solarData.inputPower, solarData.todayYield, solarData.loadCurrent,
    stateName, rssi);
}

void processSmartShunt(byte* data, int deviceIndex, int rssi, const char* deviceName) {
  victronBatteryMonitorData* monitor = (victronBatteryMonitorData*)data;
  
  shuntData.valid = true;
  shuntData.batteryVoltage = float(monitor->batteryVoltage) * 0.01;
  shuntData.ttg = monitor->ttg;
  shuntData.rssi = rssi;
  
  // Estrai aux_input (primi 2 bit di packedData[0])
  uint8_t auxInput = monitor->packedData[0] & 0x03;
  
  // Estrai current (22 bit, signed) - bit 2-23
  int32_t currentRaw = ((monitor->packedData[0] >> 2) | 
                        (monitor->packedData[1] << 6) | 
                        (monitor->packedData[2] << 14)) & 0x3FFFFF;
  // Sign extend da 22 bit
  if (currentRaw & 0x200000) currentRaw |= 0xFFC00000;
  shuntData.batteryCurrent = float(currentRaw) * 0.001;
  
  // Estrai SOC (10 bit) - ultimi 10 bit
  uint16_t socRaw = ((monitor->packedData[5] >> 4) | (monitor->packedData[6] << 4)) & 0x3FF;
  if (socRaw != 0x3FF) {  // 0x3FF = N/A
    shuntData.soc = float(socRaw) * 0.1;
  }
  
  strncpy(shuntData.deviceName, deviceName, 31);
  
  Serial.printf("[SHUNT] %s | %.2fV %.2fA | SOC:%.1f%% | TTG:%dmin | RSSI:%d\n",
    deviceName, shuntData.batteryVoltage, shuntData.batteryCurrent,
    shuntData.soc, shuntData.ttg, rssi);
}

void processBatterySense(byte* data, int deviceIndex, int rssi, const char* deviceName) {
  // Battery Sense usa lo stesso formato del Battery Monitor
  victronBatteryMonitorData* monitor = (victronBatteryMonitorData*)data;
  
  // Estrai aux_input (primi 2 bit)
  uint8_t auxInput = monitor->packedData[0] & 0x03;
  
  batterySenseData.valid = true;
  batterySenseData.batteryVoltage = float(monitor->batteryVoltage) * 0.01;
  batterySenseData.rssi = rssi;
  
  // Temperatura: auxValue contiene Kelvin * 100 (se auxInput == 2)
  if (auxInput == 2) {
    float tempKelvin = float(monitor->auxValue) * 0.01;
    batterySenseData.temperature = tempKelvin - 273.15;  // Converti in Celsius
  } else {
    // Fallback: prova comunque a interpretare come temperatura
    float tempKelvin = float(monitor->auxValue) * 0.01;
    batterySenseData.temperature = tempKelvin - 273.15;
  }
  
  strncpy(batterySenseData.deviceName, deviceName, 31);
  
  Serial.printf("[TEMP] %s | %.2fV | Temp:%.1f C | auxIn:%d | RSSI:%d\n",
    deviceName, batterySenseData.batteryVoltage, batterySenseData.temperature, 
    auxInput, rssi);
}

// ============================================================================
// Display Update
// ============================================================================
void updateDisplay() {
#if defined M5STICKC || defined M5STICKCPLUS
  display.fillScreen(COLOR_BACKGROUND);
  display.setCursor(0, 0);
  
  if (displayPage == 0) {
    // Pagina SOLAR
    display.setTextColor(COLOR_TITLE, COLOR_BACKGROUND);
    display.println("=SOLAR=");
    display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    
    if (solarData.valid) {
      display.printf("%.2fV", solarData.batteryVoltage);
      if (solarData.batteryCurrent < 0) {
        display.setTextColor(COLOR_NEGATIVE, COLOR_BACKGROUND);
      }
      display.printf(" %.1fA\n", solarData.batteryCurrent);
      display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
      
      display.printf("%dW\n", solarData.inputPower);
      display.printf("%.0fWh\n", solarData.todayYield);
      
      // Charge state
      char stateName[6];
      if (solarData.chargeState <= 7) {
        strcpy(stateName, chargeStateNames[solarData.chargeState]);
        display.setTextColor(chargeStateColors[solarData.chargeState], COLOR_BACKGROUND);
      } else {
        sprintf(stateName, "%d?", solarData.chargeState);
      }
      display.printf("%s", stateName);
    } else {
      display.println("Waiting...");
    }
    
  } else if (displayPage == 1) {
    // Pagina INFO
    display.setTextColor(COLOR_TITLE, COLOR_BACKGROUND);
    display.println("=INFO=");
    display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    
    // Temperatura batteria
    if (batterySenseData.valid) {
      display.setTextColor(COLOR_TEMP, COLOR_BACKGROUND);
      display.printf("Temp:%.1fC\n", batterySenseData.temperature);
      display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    } else {
      display.println("Temp:--");
    }
    
    // Dati Smart Shunt
    if (shuntData.valid) {
      display.printf("SOC:%.0f%%\n", shuntData.soc);
      display.printf("%.2fA\n", shuntData.batteryCurrent);
      if (shuntData.ttg != 0xFFFF) {
        int hours = shuntData.ttg / 60;
        int mins = shuntData.ttg % 60;
        display.printf("TTG:%dh%dm\n", hours, mins);
      }
    } else {
      display.println("SOC:--");
    }
    
  }
#endif
}

// ============================================================================
// BLE Callback
// ============================================================================
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    
    if (!advertisedDevice.haveManufacturerData()) return;
    
    uint8_t manCharBuf[32];
    std::string manData = advertisedDevice.getManufacturerData();
    int manDataSize = manData.length();
    
    if (manDataSize > 31) manDataSize = 31;
    manData.copy((char*)manCharBuf, manDataSize);
    
    victronManufacturerData* vicData = (victronManufacturerData*)manCharBuf;
    
    // Check Victron vendor ID
    if (vicData->vendorID != 0x02e1) return;
    
    // Get MAC and find device
    char macStr[18];
    strcpy(macStr, advertisedDevice.getAddress().toString().c_str());
    byte macByte[6];
    hexCharStrToByteArray(macStr, macByte);
    
    int deviceIndex = findDeviceByMac(macByte);
    
    // Get device name
    char deviceName[32] = "(unknown)";
    if (advertisedDevice.haveName()) {
      strcpy(deviceName, advertisedDevice.getName().c_str());
      // Strip common prefixes
      if (strstr(deviceName, "SmartSolar ") == deviceName) {
        memmove(deviceName, deviceName + 11, strlen(deviceName) - 10);
      } else if (strstr(deviceName, "SmartShunt ") == deviceName) {
        memmove(deviceName, deviceName + 11, strlen(deviceName) - 10);
      }
    }
    
    // Handle unknown devices - log them for configuration
    if (deviceIndex == -1) {
      // Log ALL Victron devices for debugging
      Serial.printf("[NEW DEVICE] Type:0x%02X Name:%s MAC:%s\n", 
        vicData->victronRecordType, deviceName, macStr);
      return;
    }
    
    // Verify encryption key
    if (vicData->encryptKeyMatch != victronDevices[deviceIndex].byteKey[0]) {
      Serial.printf("[KEY MISMATCH] %s - check encryption key!\n", deviceName);
      return;
    }
    
    // Decrypt data
    byte outputData[16] = {0};
    int encrDataSize = manDataSize - 10;
    if (!decryptVictronData(vicData, deviceIndex, outputData, encrDataSize)) {
      Serial.printf("[DECRYPT FAIL] %s\n", deviceName);
      return;
    }
    
    int rssi = advertisedDevice.getRSSI();
    
    // Process based on configured device type (ignore record type for Battery Sense)
    switch (victronDevices[deviceIndex].deviceType) {
      case DEVICE_SOLAR_CHARGER:
        if (vicData->victronRecordType == VICTRON_TYPE_SOLAR_CHARGER) {
          processSolarCharger(outputData, deviceIndex, rssi, deviceName);
          packetReceived = true;
        }
        break;
        
      case DEVICE_SMART_SHUNT:
        if (vicData->victronRecordType == VICTRON_TYPE_BATTERY_MONITOR) {
          processSmartShunt(outputData, deviceIndex, rssi, deviceName);
          packetReceived = true;
        }
        break;
        
      case DEVICE_BATTERY_SENSE:
        // Battery Sense - process ANY record type from this device
        processBatterySense(outputData, deviceIndex, rssi, deviceName);
        packetReceived = true;
        break;
        
      default:
        Serial.printf("[UNKNOWN TYPE] 0x%02X from %s\n", vicData->victronRecordType, deviceName);
        break;
    }
    
    // Update display
    updateDisplay();
  }
};

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("Victron BLE Multi-Device Scanner v2.0");
  Serial.println("========================================");
  Serial.printf("Build: %s\n", __TIMESTAMP__);
  Serial.println();

#if defined BUTTON_1
  pinMode(BUTTON_1, INPUT_PULLUP);
#endif
#if defined BUTTON_2
  pinMode(BUTTON_2, INPUT_PULLUP);
#endif

#if defined M5STICKC || defined M5STICKCPLUS
  M5.begin();
  display.init();
  display.setRotation(displayRotation);
  display.fillScreen(COLOR_BACKGROUND);
  display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  #if defined M5STICKC
    display.setTextSize(2);
  #else
    display.setTextSize(3);
  #endif
  display.setTextFont(1);
  display.setCursor(0, 0);
  display.println("Victron");
  display.println("Scanner");
  display.println("v2.0");
#endif

  delay(1500);

  Serial.printf("Configured devices: %d\n", victronDeviceCount);
  
  for (int i = 0; i < victronDeviceCount; i++) {
    hexCharStrToByteArray(victronDevices[i].charMacAddr, victronDevices[i].byteMacAddr);
    hexCharStrToByteArray(victronDevices[i].charKey, victronDevices[i].byteKey);
    strcpy(victronDevices[i].cachedDeviceName, "(unknown)");
    
    const char* typeStr = "?";
    switch (victronDevices[i].deviceType) {
      case DEVICE_SOLAR_CHARGER: typeStr = "Solar"; break;
      case DEVICE_SMART_SHUNT: typeStr = "Shunt"; break;
      case DEVICE_BATTERY_SENSE: typeStr = "BattSense"; break;
    }
    
    Serial.printf("  [%d] %-10s %-10s MAC:", i, victronDevices[i].comment, typeStr);
    for (int j = 0; j < 6; j++) Serial.printf("%02x", victronDevices[i].byteMacAddr[j]);
    Serial.println();
  }
  Serial.println();

#if defined M5STICKC || defined M5STICKCPLUS
  display.fillScreen(COLOR_BACKGROUND);
  display.setCursor(0, 0);
  display.printf("Dev:%d\n", victronDeviceCount);
  display.println("BLE...");
#endif

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  delay(1500);

  Serial.println("Ready! Scanning for Victron devices...");
  Serial.println("New Victron devices will be logged with their MAC address.\n");

#if defined M5STICKC || defined M5STICKCPLUS
  display.fillScreen(COLOR_BACKGROUND);
  display.setCursor(0, 0);
  display.println("Ready!");
#endif

  delay(1000);
}

// ============================================================================
// Loop
// ============================================================================
void loop() {
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  pBLEScan->clearResults();

#if defined BUTTON_1
  // Main button - change display page
  if (digitalRead(BUTTON_1) == LOW) {
    while (digitalRead(BUTTON_1) == LOW) delay(50);
    displayPage = (displayPage + 1) % 2;
    Serial.printf("Display page: %d\n", displayPage);
    updateDisplay();
  }
#endif

#if defined BUTTON_2
  // Side button - rotate display
  if (digitalRead(BUTTON_2) == LOW) {
    while (digitalRead(BUTTON_2) == LOW) delay(50);
    displayRotation = (displayRotation == 3) ? 1 : 3;
    Serial.printf("Display rotation: %d\n", displayRotation);
    display.setRotation(displayRotation);
    updateDisplay();
  }
#endif

  time_t timeNow = time(nullptr);
  if (!packetReceived && timeNow != lastTick) {
    lastTick = timeNow;
    Serial.println("Scanning...");
    updateDisplay();
  }
}
