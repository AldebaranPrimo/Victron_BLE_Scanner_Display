/*
  Initial BLE code adapted from Examples->BLE->Beacon_Scanner.
  Victron decryption code snippets from:
  
    https://github.com/Fabian-Schmidt/esphome-victron_ble
  
  Information on the "extra manufacturer data" that we're picking up from Victron SmartSolar
  BLE advertising beacons can be found at:
  
    https://community.victronenergy.com/storage/attachments/48745-extra-manufacturer-data-2022-12-14.pdf
  
  Thanks, Victron, for providing both the beacon and the documentation on its contents!
*/

//
// This code receives, decrypts, and decodes Victron SmartSolar BLE beacons.
// The data will be output to the Serial device and shown on the built-in display
// of M5StickC and M5StickCPlus modules if the appropriate #define is uncommented.
//

// Tested with the following boards:
//    M5StickC              ESP32-PICO with integrated  80x160 ST7735S TFT display
//    M5StickCPlus          ESP32-PICO with integrated 135x240 ST7789v2 TFT display

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <aes/esp_aes.h>  // AES decryption

// Board selection is handled by PlatformIO (platformio.ini)
#if defined M5STICKC
  #include <M5StickC.h>
#endif

#if defined M5STICKCPLUS
  #include <M5StickCPlus.h>
#endif

#if defined M5STICKC || defined M5STICKCPLUS
  M5Display display;

  // Note: user-defined colors are:     5 bits (0x1F) Red    6 bits (0x3f) Green    5 bits (0x1f) Blue
  #define COLOR_BACKGROUND              TFT_BLACK
  #define COLOR_TEXT                    TFT_DARKGREEN
  #define COLOR_NEGATIVE                TFT_MAROON
  #define COLOR_UNKNOWN                 TFT_DARKGREY
  #define COLOR_CHARGEROFF              TFT_MAROON                          // maroon = dark red
  #define COLOR_BULK                    ((0x00 << 11) + (0x00 << 5) + 0x18)   // dark blue
  #define COLOR_ABSORPTION              ((0x0f << 11) + (0x1f << 5) + 0x00)   // dim yellow
  #define COLOR_FLOAT                   TFT_DARKGREEN
  #define COLOR_EQUALIZATION            ((0x15 << 11) + (0x15 << 5) + 0x00)   // dim orange

  #define BUTTON_1 37
#endif

// The Espressif people decided to use String instead of std::string in some versions of
// their ESP32 libraries. This version uses std::string.
// #define USE_String

BLEScan *pBLEScan;

#define AES_KEY_BITS 128
int scanTime = 1;  //In seconds

// Victron data structures
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
} __attribute__((packed)) victronPanelData;

typedef struct {
  uint16_t vendorID;
  uint8_t beaconType;
  uint8_t unknownData1[3];
  uint8_t victronRecordType;
  uint16_t nonceDataCounter;
  uint8_t encryptKeyMatch;
  uint8_t victronEncryptedData[21];
  uint8_t nullPad;
} __attribute__((packed)) victronManufacturerData;

struct solarController {
  char charMacAddr[13];
  char charKey[33];
  char comment[16];
  byte byteMacAddr[6];
  byte byteKey[16];
  char cachedDeviceName[32];
};

// ============================================================================
// CONFIGURAZIONE DEL TUO CONTROLLER VICTRON - MODIFICA QUI I TUOI DATI
// ============================================================================
struct solarController solarControllers[1] = {
  { "c15639b47db5", "f2dcc3ba40edb8de7e07d7638f13f971", "Tomita", {0}, {0}, "(unknown)" }
};
// ============================================================================

int knownSolarControllerCount = sizeof(solarControllers) / sizeof(solarControllers[0]);

int bestRSSI = -200;
int selectedSolarControllerIndex = -1;

time_t lastLEDBlinkTime = 0;
time_t lastTick = 0;
int displayRotation = 3;
bool packetReceived = false;

char chargeStateNames[][6] = {
  "  off",
  "   1?",
  "   2?",
  " bulk",
  "  abs",
  "float",
  "   6?",
  "equal"
};

#if defined M5STICKC || defined M5STICKCPLUS
  uint16_t chargeStateColors[] = {
    COLOR_CHARGEROFF,
    COLOR_UNKNOWN,
    COLOR_UNKNOWN,
    COLOR_BULK,
    COLOR_ABSORPTION,
    COLOR_FLOAT,
    COLOR_UNKNOWN,
    COLOR_EQUALIZATION
  };
#endif

// Function prototypes
void hexCharStrToByteArray(char * hexCharStr, byte * byteArray);
byte hexCharToByte(char hexChar);

// ============================================================================
// hexCharStrToByteArray
// ============================================================================
byte hexCharToByte(char hexChar) {
  if (hexChar >= '0' && hexChar <= '9') {
    return hexChar - '0';
  } else if (hexChar >= 'a' && hexChar <= 'f') {
    return hexChar - 'a' + 10;
  } else if (hexChar >= 'A' && hexChar <= 'F') {
    return hexChar - 'A' + 10;
  }
  return 255;
}

void hexCharStrToByteArray(char * hexCharStr, byte * byteArray) {
  int hexCharStrLength = strlen(hexCharStr);
  int byteArrayIndex = 0;
  bool oddByte = true;
  byte hiNibble;
  
  for (int i = 0; i < hexCharStrLength; i++) {
    byte nibble = hexCharToByte(hexCharStr[i]);
    if (nibble != 255) {
      if (oddByte) {
        hiNibble = nibble;
      } else {
        byteArray[byteArrayIndex] = (hiNibble << 4) | nibble;
        byteArrayIndex++;
      }
      oddByte = !oddByte;
    }
  }
  if (!oddByte) {
    byteArray[byteArrayIndex] = hiNibble;
  }
}

// ============================================================================
// BLE Advertised Device Callback
// ============================================================================
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {

    #define manDataSizeMax 31

    if (advertisedDevice.haveManufacturerData() == true) {

      uint8_t manCharBuf[manDataSizeMax + 1];

      #ifdef USE_String
        String manData = advertisedDevice.getManufacturerData();
      #else
        std::string manData = advertisedDevice.getManufacturerData();
      #endif
      int manDataSize = manData.length();

      if (manDataSize > manDataSizeMax) {
        Serial.printf("  Note: Truncating malformed %2d byte manufacturer data to max %d byte array size\n", manDataSize, manDataSizeMax);
        manDataSize = manDataSizeMax;
      }

      #ifdef USE_String
        memcpy(manCharBuf, manData.c_str(), manDataSize);
      #else
        manData.copy((char *)manCharBuf, manDataSize);
      #endif

      victronManufacturerData * vicData = (victronManufacturerData *)manCharBuf;

      // Ignore non-Victron packets
      if (vicData->vendorID != 0x02e1) {
        return;
      }

      // Ignore non-Solar Charger packets
      if (vicData->victronRecordType != 0x01) {
        return;
      }

      char receivedMacStr[18];
      strcpy(receivedMacStr, advertisedDevice.getAddress().toString().c_str());

      byte receivedMacByte[6];
      hexCharStrToByteArray(receivedMacStr, receivedMacByte);

      int solarControllerIndex = -1;
      for (int trySolarControllerIndex = 0; trySolarControllerIndex < knownSolarControllerCount; trySolarControllerIndex++) {
        bool matchedMac = true;
        for (int i = 0; i < 6; i++) {
          if (receivedMacByte[i] != solarControllers[trySolarControllerIndex].byteMacAddr[i]) {
            matchedMac = false;
            break;
          }
        }
        if (matchedMac) {
          solarControllerIndex = trySolarControllerIndex;
          break;
        }
      }

      char deviceName[32];
      strcpy(deviceName, "(unknown device name)");
      bool deviceNameFound = false;
      if (advertisedDevice.haveName()) {
        strcpy(deviceName, advertisedDevice.getName().c_str());
        if (strstr(deviceName, "SmartSolar ") == deviceName) {
          strcpy(deviceName, deviceName + 11);
        }
        deviceNameFound = true;
      }

      if (solarControllerIndex == -1) {
        Serial.printf("Discarding packet from unconfigured Victron SmartSolar %s at MAC %s\n", deviceName, receivedMacStr);
        delay(5000);
        return;
      }

      if (deviceNameFound) {
        strcpy(solarControllers[solarControllerIndex].cachedDeviceName, deviceName);
      }

      if (vicData->encryptKeyMatch != solarControllers[solarControllerIndex].byteKey[0]) {
        Serial.printf("Encryption key mismatch for %s at MAC %s\n",
          solarControllers[solarControllerIndex].cachedDeviceName, receivedMacStr);
        return;
      }

      int RSSI = advertisedDevice.getRSSI();

      #if defined M5STICKC || defined M5STICKCPLUS
        if (selectedSolarControllerIndex == solarControllerIndex) {
          if (RSSI > bestRSSI) {
            bestRSSI = RSSI;
          }
        } else {
          if (RSSI > bestRSSI) {
            selectedSolarControllerIndex = solarControllerIndex;
            Serial.printf("Selected Victon SmartSolar %s at MAC %s as preferred device based on RSSI %d\n",
              solarControllers[solarControllerIndex].cachedDeviceName, receivedMacStr, RSSI);
          } else {
            Serial.printf("Discarding RSSI-based non-selected Victon SmartSolar %s at MAC %s\n",
              solarControllers[solarControllerIndex].cachedDeviceName, receivedMacStr);
            return;
          }
        }
      #endif

      // Decrypt the data
      byte inputData[16];
      byte outputData[16] = {0};
      victronPanelData * victronData = (victronPanelData *) outputData;

      int encrDataSize = manDataSize - 10;
      for (int i = 0; i < encrDataSize; i++) {
        inputData[i] = vicData->victronEncryptedData[i];
      }

      esp_aes_context ctx;
      esp_aes_init(&ctx);

      auto status = esp_aes_setkey(&ctx, solarControllers[solarControllerIndex].byteKey, AES_KEY_BITS);
      if (status != 0) {
        Serial.printf("  Error during esp_aes_setkey operation (%i).\n", status);
        esp_aes_free(&ctx);
        return;
      }

      byte data_counter_lsb = (vicData->nonceDataCounter) & 0xff;
      byte data_counter_msb = ((vicData->nonceDataCounter) >> 8) & 0xff;
      u_int8_t nonce_counter[16] = {data_counter_lsb, data_counter_msb, 0};
      u_int8_t stream_block[16] = {0};

      size_t nonce_offset = 0;
      status = esp_aes_crypt_ctr(&ctx, encrDataSize, &nonce_offset, nonce_counter, stream_block, inputData, outputData);
      if (status != 0) {
        Serial.printf("Error during esp_aes_crypt_ctr operation (%i).", status);
        esp_aes_free(&ctx);
        return;
      }
      esp_aes_free(&ctx);

      byte deviceState = victronData->deviceState;
      byte errorCode = victronData->errorCode;
      float batteryVoltage = float(victronData->batteryVoltage) * 0.01;
      float batteryCurrent = float(victronData->batteryCurrent) * 0.1;
      float todayYield = float(victronData->todayYield) * 0.01 * 1000;
      uint16_t inputPower = victronData->inputPower;

      int integerOutputCurrent = ((victronData->outputCurrentHi & 0x01) << 9) | victronData->outputCurrentLo;
      float outputCurrent = float(integerOutputCurrent) * 0.1;

      byte unusedBits = victronData->outputCurrentHi & 0xfe;
      if (unusedBits != 0xfe) {
        return;
      }

      char chargeStateName[6];
      sprintf(chargeStateName, "%4d?", deviceState);
      if (deviceState >= 0 && deviceState <= 7) {
        strcpy(chargeStateName, chargeStateNames[deviceState]);
      }

      #if defined M5STICKC || defined M5STICKCPLUS
        int chargeStateColor = COLOR_UNKNOWN;
        if (deviceState >= 0 && deviceState <= 7) {
          chargeStateColor = chargeStateColors[deviceState];
        }
      #endif

      Serial.printf("%-31s  Battery: %6.2f Volts %6.2f Amps  Solar: %6d Watts  Yield: %4.0f Wh  Load: %5.1f Amps  Charger: %-13s Err: %2d RSSI: %d\n",
        solarControllers[solarControllerIndex].cachedDeviceName,
        batteryVoltage, batteryCurrent,
        inputPower, todayYield,
        outputCurrent, chargeStateName, errorCode, RSSI
      );

      #if defined M5STICKC || defined M5STICKCPLUS
        display.fillScreen(COLOR_BACKGROUND);
        display.setCursor(0, 0);

        char screenDeviceName[14];
        strncpy(screenDeviceName, solarControllers[solarControllerIndex].cachedDeviceName, 13);
        screenDeviceName[13] = '\0';
        display.printf("%s\n", screenDeviceName);

        display.printf("%5.2fV", batteryVoltage);
        if (batteryCurrent < 0.0) {
          display.setTextColor(COLOR_NEGATIVE, COLOR_BACKGROUND);
        }
        display.printf("%6.1fA\n", batteryCurrent);
        display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);

        display.printf("%5dW %4.0fWh\n", inputPower, todayYield);
        display.printf("Load: %6.1fA\n", outputCurrent);

        display.printf("Charge: ");
        display.setTextColor(chargeStateColor, COLOR_BACKGROUND);
        display.printf("%s", chargeStateName);
        display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
      #endif

      packetReceived = true;
    }
  }
};

// ============================================================================
// setup()
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();
  Serial.println("Reset.");
  Serial.println();
  Serial.printf("Source file: %s\n", __FILE__);
  Serial.printf(" Build time: %s\n", __TIMESTAMP__);
  Serial.println();
  delay(1000);

  #if defined BUTTON_1
    pinMode(BUTTON_1, INPUT_PULLUP);
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

    display.println("SmartSolar");
    display.println("BLE Scanner");
    display.println("Reset.");
  #endif

  delay(2000);

  Serial.printf("Controller count: %d\n", knownSolarControllerCount);

  #if defined M5STICKC || defined M5STICKCPLUS
    display.fillScreen(COLOR_BACKGROUND);
    display.setCursor(0, 0);
    display.println("Controllers");
    display.printf("Configured:%2d\n", knownSolarControllerCount);
  #endif

  for (int i = 0; i < knownSolarControllerCount; i++) {
    hexCharStrToByteArray(solarControllers[i].charMacAddr, solarControllers[i].byteMacAddr);
    hexCharStrToByteArray(solarControllers[i].charKey, solarControllers[i].byteKey);
    strcpy(solarControllers[i].cachedDeviceName, "(unknown)");
  }

  for (int i = 0; i < knownSolarControllerCount; i++) {
    Serial.printf("  %-16s", solarControllers[i].comment);
    Serial.printf("  Mac:   ");
    for (int j = 0; j < 6; j++) {
      Serial.printf(" %2.2x", solarControllers[i].byteMacAddr[j]);
    }
    Serial.printf("    Key: ");
    for (int j = 0; j < 16; j++) {
      Serial.printf("%2.2x", solarControllers[i].byteKey[j]);
    }
    Serial.println();
  }
  Serial.println();
  Serial.println();

  delay(2000);

  #if defined M5STICKC || defined M5STICKCPLUS
    display.fillScreen(COLOR_BACKGROUND);
    display.setCursor(0, 0);
    display.printf("Setup BLE\n");
  #endif

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  delay(2000);

  Serial.println("Done with setup()");

  #if defined M5STICKC || defined M5STICKCPLUS
    display.fillScreen(COLOR_BACKGROUND);
    display.setCursor(0, 0);
    display.printf("Ready\n");
  #endif

  delay(2000);
}

// ============================================================================
// loop()
// ============================================================================
void loop() {
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  pBLEScan->clearResults();

  #if defined BUTTON_1
    if (digitalRead(BUTTON_1) == LOW) {
      while (digitalRead(BUTTON_1) == LOW) {
        delay(100);
      }

      if (displayRotation == 3) {
        displayRotation = 1;
      } else {
        displayRotation = 3;
      }
      Serial.printf("Setting display rotation to %d\n", displayRotation);
      display.setRotation(displayRotation);
      lastTick = 0;
      packetReceived = false;
    }
  #endif

  time_t timeNow = time(nullptr);
  if (!packetReceived && timeNow != lastTick) {
    lastTick = timeNow;
    Serial.println("BLE listening...");
    #if defined M5STICKC || defined M5STICKCPLUS
      display.fillScreen(COLOR_BACKGROUND);
      display.setCursor(0, 0);
      display.printf("   Waiting   \n");
      display.printf("     for     \n");
      display.printf("   Victron   \n");
      display.printf("  SmartSolar \n");
    #endif
  }
}
