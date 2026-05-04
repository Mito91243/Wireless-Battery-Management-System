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
// These were originally physical BQ output pins. Now, they are just ESP32 LEDs.
const int TB_PIN_DDSG_LED = 15; // Original Testboard LED
const int TB_PIN_DCHG_LED = 17; // Original Testboard LED

// ==================== NEW REPURPOSED LED PINS ====================
// We severed the connection to the BQ chip. GPIO 4 and 2 are now external LEDs.
const int TB_PIN_CHG_EXT_LED = 4; // Wire your new Charge LED here
const int TB_PIN_DSG_EXT_LED = 2; // Wire your new Discharge LED here

const int TB_PIN_ALERT = 5;     // GPIO 5 -> ALERT (Fault/Scan indicator)

#endif
