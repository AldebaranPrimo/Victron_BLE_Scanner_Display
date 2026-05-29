// ============================================================================
// display_ui.cpp — On-device status display rendering
//
// Primary target: M5StickC Plus (135x240 ST7789v2; landscape via setRotation(3)
// => 240 wide x 135 tall). The base M5StickC (80x160) still compiles: every
// layout value is derived from lcd.width()/lcd.height(), so the smaller panel
// simply shows fewer summary rows and tighter detail pages — no clipping, no
// hard-coded coordinates.
//
// UI model (see displayNormalUpdate):
//   - Page 0  = summary: header + one compact line per present device.
//   - Page 1..N = per-device detail: large header with the device NAME (so it is
//     always clear which device is shown) + type-specific big readouts.
//   - A "page x/N" footer makes paging discoverable.
//   - Stale devices (no data for >60 s, or never seen) are dimmed.
//
// The view is informational only; it never controls anything.
// ============================================================================

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

// A device is considered stale (dimmed) if it has not been parsed within this
// window, or has never been seen (lastUpdateMs == 0).
#define STALE_MS 60000UL

static M5Display& lcd = M5.Lcd;

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

// True if a slot's data is fresh enough to show in full color.
static bool slotFresh(const DeviceRuntimeState& s) {
    if (s.lastUpdateMs == 0) return false;
    return (millis() - s.lastUpdateMs) <= STALE_MS;
}

// Connection-status dot, top-right corner. Resolution-independent.
static void drawStatusDot(bool online, bool wifiOk, bool mqttOk) {
    int W = lcd.width();
    // Display-only (no MQTT configured) → dim/neutral dot: offline by design,
    // not an error. Online → green = all ok, yellow = WiFi only, red = down.
    uint16_t c = !online           ? C_DIM
               : (wifiOk && mqttOk) ? C_CHARGE
               : (wifiOk            ? C_LOAD : C_DRAIN);
    lcd.fillCircle(W - 8, 8, 4, c);
}

// Footer "page x/N" centered-left at the bottom.
static void drawFooter(int page, int n) {
    int H = lcd.height();
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(2, H - 10);
    lcd.printf("page %d/%d", page, n);
}

// Returns the one-line "key metric" string for a device in the summary view.
static void summaryMetric(const DeviceRuntimeState& s, char* out, size_t outLen) {
    switch (s.type) {
        case DEVICE_TYPE_SOLAR:
            if (s.data.solar.valid) snprintf(out, outLen, "%dW", s.data.solar.inputPower);
            else                    snprintf(out, outLen, "-- W");
            break;
        case DEVICE_TYPE_SHUNT:
            if (s.data.shunt.valid) snprintf(out, outLen, "SoC %.0f%%", s.data.shunt.soc);
            else                    snprintf(out, outLen, "SoC --");
            break;
        case DEVICE_TYPE_BSENSE:
            if (s.data.bsense.valid) snprintf(out, outLen, "%.1fC", s.data.bsense.temperature);
            else                     snprintf(out, outLen, "-- C");
            break;
        default:
            snprintf(out, outLen, "?");
            break;
    }
}

// Page 0: header + one compact row per present device. Scales 1..6 devices: at
// 240x135 with textSize(1) (~6x8 px glyphs) there is room for well over 6 rows.
static void drawSummary(const int* slots, int n) {
    lcd.setTextSize(1);
    lcd.setTextColor(C_TITLE, C_BG);
    lcd.setCursor(2, 2);
    lcd.print("Victron GW");

    int y = 16;
    const int rowH = 14;
    char metric[24];

    if (n == 0) {
        lcd.setTextColor(C_DIM, C_BG);
        lcd.setCursor(2, y);
        lcd.print("no devices");
        return;
    }

    for (int k = 0; k < n; k++) {
        const DeviceRuntimeState& s = devStates[slots[k]];
        bool fresh = slotFresh(s);

        // Name (left), metric (right-ish). Color by type, dimmed if stale.
        uint16_t nameColor = fresh ? C_WHITE : C_DIM;
        uint16_t valColor;
        switch (s.type) {
            case DEVICE_TYPE_SOLAR:  valColor = C_SOLAR; break;
            case DEVICE_TYPE_SHUNT:  valColor = C_CHARGE; break;
            default:                 valColor = C_LOAD; break;
        }
        if (!fresh) valColor = C_DIM;

        lcd.setTextColor(nameColor, C_BG);
        lcd.setCursor(2, y);
        lcd.printf("%-13s", s.name);

        summaryMetric(s, metric, sizeof(metric));
        lcd.setTextColor(valColor, C_BG);
        // Place the metric at a fixed column ~ 14 chars * 6px in.
        lcd.setCursor(86, y);
        lcd.print(metric);

        y += rowH;
    }
}

// Detail header: device name in large text + a thin separator. Shared by all
// detail pages so it is always clear which device is shown.
static void drawDetailHeader(const DeviceRuntimeState& s, uint16_t color) {
    lcd.setTextColor(color, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(2, 2);
    lcd.printf("%s", s.name);
}

static void drawSolarDetail(const DeviceRuntimeState& s) {
    const SolarDisplayData& d = s.data.solar;
    bool fresh = slotFresh(s);
    uint16_t big = fresh ? C_SOLAR : C_DIM;
    uint16_t txt = fresh ? C_WHITE : C_DIM;

    drawDetailHeader(s, big);

    // Big PV power
    lcd.setTextColor(big, C_BG);
    lcd.setTextSize(3);
    lcd.setCursor(2, 26);
    if (d.valid) lcd.printf("%dW", d.inputPower);
    else         lcd.print("-- W");

    // Secondary lines
    lcd.setTextSize(1);
    lcd.setTextColor(txt, C_BG);
    int y = 60;
    if (d.valid) {
        const char* states[] = {"off", "low_power", "fault", "bulk",
                                "absorption", "float", "storage", "equalize"};
        const char* st = (d.chargeState <= 7) ? states[d.chargeState] : "unknown";
        lcd.setCursor(2, y);      lcd.printf("V %.2f  I %.1fA", d.batteryVoltage, d.batteryCurrent);
        lcd.setCursor(2, y + 12); lcd.printf("Yield %.0fWh", d.todayYield);
        lcd.setCursor(2, y + 24); lcd.printf("%s  RSSI %d", st, d.rssi);
    } else {
        lcd.setCursor(2, y); lcd.print("waiting for data...");
    }
}

static void drawShuntDetail(const DeviceRuntimeState& s) {
    const ShuntDisplayData& d = s.data.shunt;
    bool fresh = slotFresh(s);
    uint16_t txt = fresh ? C_WHITE : C_DIM;

    drawDetailHeader(s, fresh ? C_CHARGE : C_DIM);

    // Big SoC
    lcd.setTextColor(fresh ? C_CHARGE : C_DIM, C_BG);
    lcd.setTextSize(3);
    lcd.setCursor(2, 26);
    if (d.valid) lcd.printf("%.0f%%", d.soc);
    else         lcd.print("--%");

    lcd.setTextSize(1);
    int y = 60;
    if (d.valid) {
        // Current colored by sign (charge green / drain red), dimmed if stale.
        uint16_t curColor = fresh ? (d.batteryCurrent >= 0 ? C_CHARGE : C_DRAIN) : C_DIM;
        lcd.setTextColor(txt, C_BG);
        lcd.setCursor(2, y); lcd.printf("V %.2f", d.batteryVoltage);
        lcd.setTextColor(curColor, C_BG);
        lcd.setCursor(80, y); lcd.printf("I %+.2fA", d.batteryCurrent);
        lcd.setTextColor(txt, C_BG);
        lcd.setCursor(2, y + 12); lcd.printf("TTG %dmin", d.ttg);
        lcd.setCursor(2, y + 24); lcd.printf("RSSI %d", d.rssi);
    } else {
        lcd.setTextColor(txt, C_BG);
        lcd.setCursor(2, y); lcd.print("waiting for data...");
    }
}

static void drawBSenseDetail(const DeviceRuntimeState& s) {
    const BatterySenseDisplayData& d = s.data.bsense;
    bool fresh = slotFresh(s);
    uint16_t txt = fresh ? C_WHITE : C_DIM;

    drawDetailHeader(s, fresh ? C_LOAD : C_DIM);

    // Big temperature
    lcd.setTextColor(fresh ? C_LOAD : C_DIM, C_BG);
    lcd.setTextSize(3);
    lcd.setCursor(2, 26);
    if (d.valid) lcd.printf("%.1fC", d.temperature);
    else         lcd.print("-- C");

    lcd.setTextSize(1);
    lcd.setTextColor(txt, C_BG);
    int y = 60;
    if (d.valid) {
        lcd.setCursor(2, y);      lcd.printf("V %.2f", d.batteryVoltage);
        lcd.setCursor(2, y + 12); lcd.printf("RSSI %d", d.rssi);
    } else {
        lcd.setCursor(2, y); lcd.print("waiting for data...");
    }
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

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
    lcd.setTextColor(C_DIM, C_BG);
    lcd.printf("FW:   v%s\n", FW_VERSION);
}

void displayNormalUpdate(bool online, bool wifiOk, bool mqttOk, int page) {
    int slots[MAX_DEVICES];
    int n = bleEnabledSlots(slots, MAX_DEVICES);

    lcd.fillScreen(C_BG);
    drawStatusDot(online, wifiOk, mqttOk);

    if (page <= 0 || n == 0) {
        // Summary page (also the fallback when there are no devices).
        drawSummary(slots, n);
        drawFooter(0, n);
        return;
    }

    // Detail page. Map page 1..n -> slot index. Clamp defensively.
    int di = page - 1;
    if (di >= n) di = n - 1;
    const DeviceRuntimeState& s = devStates[slots[di]];

    switch (s.type) {
        case DEVICE_TYPE_SOLAR:  drawSolarDetail(s);  break;
        case DEVICE_TYPE_SHUNT:  drawShuntDetail(s);  break;
        case DEVICE_TYPE_BSENSE: drawBSenseDetail(s); break;
        default: break;
    }
    drawFooter(page, n);
}

void displayHandleButtons(int& displayPage, int& displayRotation, int pageCount) {
    #define BUTTON_A 37
    #define BUTTON_B 39

    if (pageCount < 1) pageCount = 1;

    if (digitalRead(BUTTON_A) == LOW) {
        while (digitalRead(BUTTON_A) == LOW) delay(50);
        displayPage = (displayPage + 1) % pageCount;
    }

    if (digitalRead(BUTTON_B) == LOW) {
        while (digitalRead(BUTTON_B) == LOW) delay(50);
        displayRotation = (displayRotation == 3) ? 1 : 3;
        lcd.setRotation(displayRotation);
    }
}

#else
// No display stubs (build target without a display)
void displayInit() {}
void displayBootMessage(const char*, const char*) {}
void displayConfigMode(const char*, const char*) {}
void displayNormalUpdate(bool, bool, bool, int) {}
void displayHandleButtons(int&, int&, int) {}
#endif
