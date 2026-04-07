#ifndef TB_CONFIG_H
#define TB_CONFIG_H

#include <stdint.h>

// ==================== I2C CONFIG ====================
// Both ESPs must share these same pins via jumper wires (SDA-SDA, SCL-SCL, GND-GND)
const int TB_I2C_SDA = 21;
const int TB_I2C_SCL = 22;
const uint8_t BQ_I2C_ADDR_TB = 0x08; // Same as real BQ76952

// ==================== DASHBOARD AP CONFIG ====================
const char *const TB_AP_SSID = "wBMS-TestBoard";
const char *const TB_AP_PASSWORD = "wbms1234";

// ==================== TEST BOARD CONFIG ====================
const int TB_CONNECTED_CELLS = 3;
const uint32_t TB_UPDATE_INTERVAL_MS = 500;

// ==================== BQ76952 HARDWARE CONTROL PINS ====================
// Wire: ESP32 GPIO 4 -> BQ76952 CFETOFF, ESP32 GPIO 2 -> BQ76952 DFETOFF
const int TB_PIN_CFETOFF = 4;  // GPIO 4 -> CFETOFF (Charge FET hardware override)
const int TB_PIN_DFETOFF = 2;  // GPIO 2 -> DFETOFF (Discharge FET hardware override, also blue LED)

// ==================== BQ76952 FET STATUS LED PINS ====================
// These read the BQ76952 DDSG/DCHG output signals to drive indicator LEDs.
// DDSG = HIGH when Discharge FET is ON, DCHG = HIGH when Charge FET is ON.
const int TB_PIN_DDSG_LED = 15; // GPIO 15 -> DDSG (Discharge FET status indicator LED)
const int TB_PIN_DCHG_LED = 17; // GPIO 17 -> DCHG (Charge FET status indicator LED)

#endif
