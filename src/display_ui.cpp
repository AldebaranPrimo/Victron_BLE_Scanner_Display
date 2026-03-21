#include "display_ui.h"

#if defined M5STICKC
  #include <M5StickC.h>
#elif defined M5STICKCPLUS
  #include <M5StickCPlus.h>
#endif

#if defined M5STICKC || defined M5STICKCPLUS

// Colors
#define C_BG     TFT_BLACK
#define C_SOLAR  TFT_CYAN
#define C_CHARGE TFT_GREEN
#define C_DRAIN  TFT_RED
#define C_LOAD   TFT_YELLOW
#define C_TITLE  TFT_CYAN
#define C_TEXT   TFT_DARKGREEN
#define C_DIM    TFT_DARKGREY
#define C_WHITE  TFT_WHITE

static M5Display& lcd = M5.Lcd;

void displayInit() {
    M5.begin();
    lcd.setRotation(3);
    lcd.fillScreen(C_BG);
    lcd.setTextColor(C_TEXT, C_BG);
    lcd.setTextSize(1);
    lcd.setTextFont(1);
}

void displayBootMessage(const char* line1, const char* line2) {
    lcd.fillScreen(C_BG);
    lcd.setTextColor(C_WHITE, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(0, 10);
    lcd.println(line1);
    lcd.setTextColor(C_SOLAR, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(0, 40);
    lcd.println(line2);
}

void displayConfigMode(const char* ssid, const char* ip) {
    lcd.fillScreen(C_BG);
    lcd.setTextSize(2);
    lcd.setTextColor(C_CHARGE, C_BG);
    lcd.setCursor(0, 2);
    lcd.println("SETUP");
    lcd.setTextSize(1);
    lcd.setTextColor(C_WHITE, C_BG);
    lcd.setCursor(0, 24);
    lcd.printf("WiFi: %s\n", ssid);
    lcd.printf("URL:  %s\n", ip);
}

void displayNormalUpdate(bool wifiOk, bool mqttOk) {
    lcd.fillScreen(C_BG);

    // Status indicator top-right
    uint16_t statusColor = (wifiOk && mqttOk) ? C_CHARGE : (wifiOk ? C_LOAD : C_DRAIN);
    lcd.fillCircle(155, 4, 3, statusColor);

    // 3 rows: solar power, battery power, consumption
    int y = 2;
    int rowH = 25;

    // Row 1: Solar power (PV)
    lcd.setTextColor(C_SOLAR, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(0, y + 2);
    lcd.print("PV");
    lcd.setTextSize(2);
    lcd.setCursor(20, y);
    if (solarData.valid) {
        lcd.printf("%4d W", solarData.inputPower);
    } else {
        lcd.print("  -- W");
    }

    y += rowH;

    // Row 2: Battery power (V * I from SmartShunt)
    float battWatt = 0;
    bool battValid = shuntData.valid;
    if (battValid) {
        battWatt = shuntData.batteryVoltage * shuntData.batteryCurrent;
    }
    lcd.setTextColor(battWatt >= 0 ? C_CHARGE : C_DRAIN, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(0, y + 2);
    lcd.print("BT");
    lcd.setTextSize(2);
    lcd.setCursor(20, y);
    if (battValid) {
        lcd.printf("%+4.0f W", battWatt);
    } else {
        lcd.print("  -- W");
    }

    y += rowH;

    // Row 3: Consumption (solar - battery)
    lcd.setTextColor(C_LOAD, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(0, y + 2);
    lcd.print("LD");
    lcd.setTextSize(2);
    lcd.setCursor(20, y);
    if (solarData.valid && battValid) {
        float consumption = float(solarData.inputPower) - battWatt;
        if (consumption < 0) consumption = 0;
        lcd.printf("%4.0f W", consumption);
    } else {
        lcd.print("  -- W");
    }
}

void displayHandleButtons(int& displayPage, int& displayRotation) {
    #define BUTTON_A 37
    #define BUTTON_B 39

    if (digitalRead(BUTTON_A) == LOW) {
        while (digitalRead(BUTTON_A) == LOW) delay(50);
        displayPage = (displayPage + 1) % 2;
    }

    if (digitalRead(BUTTON_B) == LOW) {
        while (digitalRead(BUTTON_B) == LOW) delay(50);
        displayRotation = (displayRotation == 3) ? 1 : 3;
        lcd.setRotation(displayRotation);
    }
}

#else
// No display stubs
void displayInit() {}
void displayBootMessage(const char*, const char*) {}
void displayConfigMode(const char*, const char*) {}
void displayNormalUpdate(bool, bool) {}
void displayHandleButtons(int&, int&) {}
#endif
