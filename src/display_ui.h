#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

// ============================================================================
// display_ui.h — On-device status display (M5StickC Plus primary, base M5StickC
// still builds)
//
// Multi-device paged UI:
//   - Page 0 = summary: one compact line per present device (key metric).
//   - Pages 1..N = one device per present slot, detailed. Button A cycles pages.
// All layout is resolution-independent (uses lcd.width()/height()), so it works
// on both the Plus (240x135 landscape) and the base M5StickC (160x80).
// ============================================================================

#include <Arduino.h>
#include "victron_ble.h"

void displayInit();
void displayBootMessage(const char* line1, const char* line2);
void displayConfigMode(const char* ssid, const char* ip);

// Renders the given page (0 = summary, 1..N = per-device detail). The page index
// is mapped to a device slot via bleEnabledSlots(). `online` is false in
// display-only mode (no MQTT) → the status dot shows a neutral colour.
void displayNormalUpdate(bool online, bool wifiOk, bool mqttOk, int page);

// Button A cycles `displayPage` over `pageCount`; Button B toggles rotation.
void displayHandleButtons(int& displayPage, int& displayRotation, int pageCount);

#endif
