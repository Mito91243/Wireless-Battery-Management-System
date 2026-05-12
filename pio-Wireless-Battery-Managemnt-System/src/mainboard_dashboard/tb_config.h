#ifndef TB_CONFIG_H
#define TB_CONFIG_H

#include <stdint.h>

// ==================== I2C CONFIG ====================
const int TB_I2C_SDA = 12;
const int TB_I2C_SCL = 13;
const uint8_t BQ_I2C_ADDR_TB = 0x08; // Same as real BQ76952

// ==================== DASHBOARD AP CONFIG ====================
const char *const TB_AP_SSID = "wBMS-MainBoard";
const char *const TB_AP_PASSWORD = "wbms1234";

// ==================== HOME WIFI (STA) CONFIG ====================
const char *const TB_STA_SSID = "MEMO11";
const char *const TB_STA_PASSWORD = "01111631233Eng";

// ==================== PACK CONFIG (13S) ====================
const int TB_CONNECTED_CELLS = 13;
const uint32_t TB_UPDATE_INTERVAL_MS = 500;

// BQ76952 Cell Pin Mapping for 13S configuration:
//   Cells 1-12  → VC1 through VC12 (normal)
//   VC13, VC14, VC15 are shorted to VC12 (unused/bypassed)
//   Cell 13     → VC16-VC15 (reads VC16 minus VC12 through the shorted chain)
const uint8_t CELL_TO_BQ[13]      = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16};
const uint8_t CELL_TO_BAL_BIT[13] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 15};

// Vcell Mode register value: enable cells 1-12, 16 / disable 13-15
// Binary: 1000_1111_1111_1111 = 0x8FFF
const uint16_t VCELL_MODE_13S = 0x8FFF;

// ==================== INDICATOR LED PINS ====================
const int TB_PIN_CHG_EXT_LED = 47; // Charge process indicator LED
const int TB_PIN_DSG_EXT_LED = 48; // Discharge process indicator LED

// ==================== ALERT PIN (BQ76952 Open-Drain Watchdog) ====================
const int TB_PIN_ALERT = 14; // GPIO 14 → ALERT (Input, BQ pulls LOW)

// ==================== LEGACY TESTBOARD PINS (NOT WIRED ON MAIN BOARD) ====================
// Kept to prevent compile errors; these GPIOs are unused on the main PCB.
const int TB_PIN_DDSG_LED = 15;
const int TB_PIN_DCHG_LED = 17;
const int TB_PIN_CFETOFF = 4;
const int TB_PIN_DFETOFF = 2;

#endif
