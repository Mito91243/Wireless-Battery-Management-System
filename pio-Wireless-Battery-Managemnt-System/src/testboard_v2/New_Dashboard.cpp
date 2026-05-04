#include "BQ76952.h"
#include "tb_config.h"
#include <Arduino.h>
#include <BatteryEKF.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>

// ==================== OBJECTS ====================
BQ76952 bms;
WebServer server(80);
Preferences prefs;

// Helper to safely read subcommands (avoids BQ76952.cpp buggy method which
// writes 0x0000)
uint16_t safeReadSubCommand(uint16_t command, uint8_t length) {
  Wire.beginTransmission(0x08);
  Wire.write(0x3E);
  Wire.write(command & 0xFF);
  Wire.write((command >> 8) & 0xFF);
  Wire.endTransmission();
  delay(2);
  Wire.beginTransmission(0x08);
  Wire.write(0x40);
  Wire.endTransmission(false);
  Wire.requestFrom((int)0x08, (int)length);
  uint16_t result = 0;
  if (Wire.available() >= length) {
    result = Wire.read();
    result |= (Wire.read() << 8);
  }
  return result;
}



// ==================== CACHED BMS DATA ====================
unsigned int cellVoltages[16];
unsigned int vStack = 0;
unsigned int vPack = 0;
int bmsCurrent = 0;
float chipTemp = 0;
float temp1 = 0, temp2 = 0, temp3 = 0;
float charge = 0;
uint32_t chargeTime = 0;
bool isCharging = false;
bool isDischarging = false;
enum BalancingMode {
  BAL_MODE_AUTONOMOUS, // BQ Hardware controls
  BAL_MODE_HOST_ALGO,  // ESP32 math controls
  BAL_MODE_MANUAL      // User forced mask
};
BalancingMode currentBalMode = BAL_MODE_HOST_ALGO;
bool balancingEnabled = false; // Master toggle to enable/disable ANY balancing
bool balSuspended =
    false; // Tracks when ESP32 pauses balancing to protect BQ sleep
bool fetEn = false;
bool watchdogFaultLocked =
    false; // TRUE = safety fault active, ALERT LED stays SOLID RED
bool ledState = false;
bq_protection_t protStatus;
bq_temperature_t tempStatus;
uint16_t balancingMask = 0;
uint16_t hwBalancingMask = 0; // True hardware state
uint16_t hostBalTriggerMv = 3400;
uint16_t hostBalDeltaMv = 1;
uint16_t lastFetStat = 0; // Cached FET_Status register (0x7F) for API access
uint16_t hostRequestedMask = 0; // Tracks what the ESP32 explicitly commands
uint16_t manufStatus = 0;       // Sealing and ConfigUpdate status
uint16_t ssA_val = 0;           // Safety Status A cache
uint16_t ssB_val = 0;           // Safety Status B cache
uint16_t ssC_val = 0;           // Safety Status C cache
uint16_t controlStatus = 0;     // Control Status (0x00) for LD_TIMEOUT
unsigned int ldPinVoltage = 0;  // Direct Command 0x38 (LD Pin)
uint8_t powerMode = 0;          // 0=Active, 1=Sleep, 2=DeepSleep, 3=Shutdown
uint8_t pendingPwrMode =
    0; // 0=none, 1=SLEEP pending, 2=DEEPSLEEP pending, 3=WAKE pending
bool autoSleepEnabled = false; // Tracks Power Config (0x9234) Bit 0
unsigned long pwrCommandTime = 0;

const uint32_t READ_INTERVAL_MS =
    5000; // Increased interval drastically as a fallback only (Interrupts
          // handle normal flow)
unsigned long lastRead = 0;
uint32_t txCount = 0;

// ==================== BALANCING STATUS (DIAGNOSTIC) ====================
bool isHardwareBalancing = false;
uint32_t cellBalancingTimes[16] = {0};
uint16_t totalBalancingTime = 0;
uint16_t hwBalancingTime = 0; // Direct read from BQ76952 CB Active Time
                              // (0x0085) — evidence of real HW balancing
uint16_t cellBalancingDelta = 0;
uint16_t minCellV = 0;
uint16_t maxCellV = 0;

// ==================== EKF DATA ====================
BatteryEKF ekf(1.0f); // 1 second sample time (was 10s)
float soc_ekf = 0.0f;
float soc_uncertainty = 0.0f;
float voltage_error_ekf = 0.0f;
float initial_ekf_soc = -1.0f;   // Stores the boot SOC from the EKF
float software_charge_Ah = 0.0f; // Accurate CC integration
unsigned long last_cc_update = 0;
unsigned long lastEKFUpdate = 0;
const unsigned long EKF_UPDATE_INTERVAL = 1000; // 1 second update (was 10s)

// ==================== PERFORMANCE PROFILING ====================
uint32_t perf_i2c_us = 0;
uint32_t perf_ekf_us = 0;
uint32_t perf_web_us = 0;
uint32_t web_peak_us = 0;

// Helper function for smart SOC initialization
float smartSOCInit(float V_measured, float I_current) {
  const float R0_avg = 0.018f;
  const float PARASITIC_R = 2.43f;

  // Strip both internal resistance AND the massive testboard wiring drop!
  float total_R = R0_avg;
  if (fabsf(I_current) > 0.05f)
    total_R += PARASITIC_R;

  float V_corrected = V_measured - (total_R * I_current);

  // Invert OCV table for accurate initialization
  float soc_init = ekf.invertOCV_Discharge(V_corrected);

  Serial.printf("[EKF] Smart Init (OCV lookup): V=%.3fV (raw=%.3fV), I=%.2fA "
                "-> SOC~%.1f%%\n",
                V_corrected, V_measured, I_current, soc_init);
  return constrain(soc_init, 0.0f, 100.0f);
}

void updateEKF() {
  float current_A =
      (float)bmsCurrent / 1000.0f; // Re-enabled real current reading!
  float voltage_V = (float)cellVoltages[0] / 1000.0f; // Use cell 1 for EKF
  ekf.update(current_A, voltage_V);
  soc_ekf = ekf.getSOC();
  soc_uncertainty = ekf.getSOCUncertainty();
  voltage_error_ekf = ekf.getVoltageError() * 1000.0f;
  Serial.printf(
      "[EKF] I=%.3fA, V=%.3fV -> SOC=%.1f%% (unc=%.2f%%), Verr=%.1fmV\n",
      current_A, voltage_V, soc_ekf, soc_uncertainty, voltage_error_ekf);
}

// ==================== CELL BALANCING LOGIC ====================
// Legacy `runBalancing()` function completely removed to prevent it from
// maliciously overwriting `balancingMask` back to 0.

// ==================== DRY HELPER FUNCTIONS ====================
// Extracted to prevent typos and ensure identical behavior everywhere.

void clearBmsAlarms() {
  bms.directCommandWrite(0x62, 0xFF);
  bms.directCommandWrite(0x63, 0xFF);
}

void wakeBms() {
  bms.directCommandRead(0x12);       // Dummy read to wake I2C engine
  bms.CommandOnlysubCommand(0x009A); // SLEEP_DISABLE
}

void rearmAlerts() {
  clearBmsAlarms();
  bms.directCommandWrite(0x66, 0xFF); // Re-enable alarm mask low
  bms.directCommandWrite(0x67, 0xFF); // Re-enable alarm mask high
}

// Include the BQ76952 configuration packer (must be after all globals &
// helpers)
#include "bms_init.h"

// ==================== I2C LOG ====================
#define LOG_SIZE 50
struct I2CLogEntry {
  uint32_t timestamp;
  uint8_t reg;
  uint16_t value;
  bool isWrite;
  bool ok;
};
I2CLogEntry i2cLog[LOG_SIZE];
int logHead = 0;
int logCount = 0;

void addLog(uint8_t reg, uint16_t value, bool isWrite, bool ok) {
  i2cLog[logHead].timestamp = millis();
  i2cLog[logHead].reg = reg;
  i2cLog[logHead].value = value;
  i2cLog[logHead].isWrite = isWrite;
  i2cLog[logHead].ok = ok;
  logHead = (logHead + 1) % LOG_SIZE;
  if (logCount < LOG_SIZE)
    logCount++;
  txCount++;
}

// ==================== READ BMS DATA VIA I2C ====================
void readBMSData() {
  unsigned int batStat = bms.directCommandRead(DIR_CMD_BAT_STATUS);
  if (batStat == 0 || batStat == 0xFFFF) {
    Serial.println("[I2C] Comm failure. Chip is either in Deep Sleep/Shutdown, "
                   "or bus is noisy.");
    // If we specifically asked it to sleep, and now it's dead, assume it
    // worked!
    if (pendingPwrMode == 2)
      powerMode = 2;
    else if (pendingPwrMode == 3) {
      // WAKE PENDING but comm failed — chip is probably still in DEEPSLEEP
      // Don't set powerMode=3 (SHUTDOWN)! Keep current mode optimistically.
      powerMode = 0;
      Serial.println(
          "[PWR] WAKE pending but I2C comm failed. Assuming wake in progress.");
    } else
      powerMode = 4; // Add a new pseudo-mode: 4 = OFFLINE
    return;          // Skip the rest of the math
  }

  // THE TRUTH FIX: In Host Mode, reading 0x0083 triggers the MAC engine and kills the mask.
  // We MUST trust our internal ESP32 state and leave the BQ chip alone!
  uint16_t currentMask = 0;
  if (currentBalMode == BAL_MODE_AUTONOMOUS) {
    currentMask = safeReadSubCommand(0x0083, 2); // Safe to poll in Auto mode
  } else {
    currentMask = hostRequestedMask; // Trust the ESP32 state!
  }
  isHardwareBalancing = (currentMask > 0);
  bool isSleep = (batStat & 0x8000) != 0; // Bit 15 = SLEEP

  uint8_t prevMode = powerMode;
  // EVENT-DRIVEN POWER STATE CONFIRMATION
  // Instead of blindly trusting BatStatus, we use it as HARDWARE CONFIRMATION.
  // TRM Section 12.2.16: BatStatus bit 15 = SLEEP. No DEEPSLEEP bit exists.
  // pendingPwrMode: 0=none, 1=SLEEP pending, 2=DEEPSLEEP pending, 3=WAKE
  // pending
  bool currentSleepEn = (batStat & 0x0004) != 0; // Bit 2 = SLEEP_EN
  // autoSleepEnabled = currentSleepEn; // REMOVED: Prevent feedback loop! User
  // intent is master.

  // Periodic diagnostic (every ~5s to avoid serial flood)
  static unsigned long lastDiag = 0;
  if (millis() - lastDiag >= 5000) {
    lastDiag = millis();
    bool sleepEnBit = (batStat & 0x0800) != 0;   // Bit 11
    bool fetEnBit = (manufStatus & 0x0010) != 0; // Bit 4
    Serial.printf("[DIAG] BatStat=0x%04X | SLEEP=%d SLEEP_EN=%d FET_EN=%d | "
                  "autoSleepIntent=%d\n",
                  batStat, isSleep ? 1 : 0, sleepEnBit ? 1 : 0,
                  fetEnBit ? 1 : 0, autoSleepEnabled ? 1 : 0);
  }

  if (pendingPwrMode == 1) {
    // SLEEP PENDING: Waiting for BatStatus SLEEP bit to confirm
    if (isSleep) {
      powerMode = 1;
      pendingPwrMode = 0;
      Serial.println("[PWR] SLEEP confirmed by BatStatus bit 15 = 1");
    } else if (millis() - pwrCommandTime > 5000) {
      // Timeout: BQ refused to enter SLEEP after 5 seconds
      pendingPwrMode = 0;
      Serial.println(
          "[PWR] WARNING: SLEEP failed! BQ did not set SLEEP bit after 5s.");
      // Attempt emergency force-sleep
      bms.CommandOnlysubCommand(0x0099); // SLEEP_ENABLE
    }
  } else if (pendingPwrMode == 3) {
    // WAKE PENDING: Waiting for BatStatus SLEEP bit to clear
    if (!isSleep) {
      powerMode = 0;
      pendingPwrMode = 0;
      Serial.println("[PWR] WAKE confirmed by BatStatus bit 15 = 0");
    } else if (millis() - pwrCommandTime > 5000) {
      // Timeout: BQ refused to wake after 5 seconds
      pendingPwrMode = 0;
      Serial.println(
          "[PWR] WARNING: WAKE failed! BQ still shows SLEEP after 5s.");
      // Aggressive wake: send dummy read and SLEEP_DISABLE
      bms.directCommandRead(0x12);
      bms.CommandOnlysubCommand(0x009A);
    }
  } else if (pendingPwrMode == 0) {
    // No transition pending: trust BatStatus normally
    if (isSleep)
      powerMode = 1;
    else
      powerMode = 0;
  }
  // pendingPwrMode == 2 (DEEPSLEEP): handled in loop(), don't touch powerMode
  // here

  if (powerMode != prevMode) {
    const char *modeNames[] = {"ACTIVE", "SLEEP", "DEEP SLEEP", "SHUTDOWN"};
    Serial.printf("[PWR] State changed: %s -> %s (BatStatus=0x%04X)\n",
                  modeNames[prevMode], modeNames[powerMode], batStat);
  }

  minCellV = 65535;
  maxCellV = 0;
  for (int i = 0; i < TB_CONNECTED_CELLS; i++) {
    uint16_t rawV = (i == TB_CONNECTED_CELLS - 1) ? bms.getCellVoltage(16) : bms.getCellVoltage(i + 1);
    
    // GLITCH FILTER: Discard insane values (I2C noise) to prevent phantom safety faults
    if (rawV > 500 && rawV < 5000) {
      cellVoltages[i] = rawV;
    } else {
      // If glitch detected, we stick with the previous value to prevent watchdog trip
      if (cellVoltages[i] == 0) cellVoltages[i] = rawV; 
    }

    if (cellVoltages[i] > 0 && cellVoltages[i] < minCellV)
      minCellV = cellVoltages[i];
    if (cellVoltages[i] > maxCellV)
      maxCellV = cellVoltages[i];
  }
  for (int i = TB_CONNECTED_CELLS; i < 16; i++)
    cellVoltages[i] = 0;

  cellBalancingDelta = (minCellV < 65535) ? (maxCellV - minCellV) : 0;

  vStack = bms.getCellVoltage(17);
  vPack = bms.getCellVoltage(18);
  bmsCurrent = bms.getCurrent();

  // TRM Section 7.3: CC2 ADC noise sigma ~12µV at WK_SPD=0.
  // With 1mΩ shunt: 12µV / 1mΩ = 12mA noise. Apply 2-sigma deadband (±20mA).
  if (abs(bmsCurrent) <= 20)
    bmsCurrent = 0;
  chipTemp = bms.getInternalTemp();
  temp1 = 0.0;                        // TS1 is disabled in config
  temp2 = bms.getThermistorTemp(HDQ); // Replaced TS2 with HDQ
  temp3 = bms.getThermistorTemp(TS3);

  // Raw ADC debug: 0x70 is TS1 Temperature direct command (units: 0.1K)
  unsigned int rawHDQ = bms.directCommandRead(0x76); // HDQ Direct Command
  // Serial.printf("[TEMP] TS1/TS3/HDQ raw=0x%04X (%d) -> %.1f°C |
  // Die=%.1f°C\n",
  //               rawHDQ, rawHDQ, temp2, chipTemp);

  // We only pull the Coulomb Counter data every 30s to avoid sending
  // subcommands
  unsigned long now_cc = millis();
  if (last_cc_update != 0) {
    float dt_h = (now_cc - last_cc_update) / 3600000.0f;
    // We integrate actual current to completely bypass BQ CC scaling bugs
    software_charge_Ah += ((float)bmsCurrent / 1000.0f) * dt_h;
  }
  last_cc_update = now_cc;

  // Poll the true hardware Cumulative Balancing Timers (CBSTATUS2/3)
  // TRM: CBSTATUS2/3 returns cumulative seconds per cell. These are ACCUMULATED
  // by the BQ hardware and only reset on device reset or CONFIG_UPDATE.
  // Poll every ~2 seconds (4 FULLSCAN cycles @ ~500ms each) for smooth UI
  // updates.
  static uint8_t slowPollCounter = 0;
  slowPollCounter++;
  if (slowPollCounter >= 4) {
    if (currentBalMode == BAL_MODE_AUTONOMOUS) {
      bms.GetCellBalancingTimes(cellBalancingTimes);
    }
    slowPollCounter = 0;
  }

  // RESTORED DEBUG LOG: Print real BQ76952 hardware state every poll cycle
  uint16_t fetStat = bms.directCommandRead(0x7F);
  lastFetStat = fetStat; // Cache for API access
  // Update LEDs based purely on BQ Hardware State
  // Bit 0 = CHG, Bit 1 = PCHG, Bit 2 = DSG, Bit 3 = PDSG
  int isChgOn = (fetStat & 0x01) ? 1 : 0;
  int isPchgOn = (fetStat & 0x02) ? 1 : 0;
  int isDsgOn = (fetStat & 0x04) ? 1 : 0;
  int isPdsgOn = (fetStat & 0x08) ? 1 : 0;

  // The ESP32 software variables mirror the hardware 1:1 with ZERO delay.
  isCharging = (isChgOn != 0) || (isPchgOn != 0);
  isDischarging = (isDsgOn != 0) || (isPdsgOn != 0);

  // Drive the original physical LEDs from the ESP32 (Active LOW)
  digitalWrite(TB_PIN_DCHG_LED, isCharging ? LOW : HIGH);
  digitalWrite(TB_PIN_DDSG_LED, isDischarging ? LOW : HIGH);

  // Drive your newly wired external LEDs on GPIO 4 and 2 (Assuming Active HIGH)
  digitalWrite(TB_PIN_CHG_EXT_LED, isCharging ? HIGH : LOW);
  digitalWrite(TB_PIN_DSG_EXT_LED, isDischarging ? HIGH : LOW);


  // ==================== WATCHDOG LOGIC ====================
  // Read Safety Status FIRST to decide whether to clear the ALERT latch.
  ssA_val = bms.directCommandRead(0x03); // Safety Status A
  ssB_val = bms.directCommandRead(0x05); // Safety Status B
  ssC_val = bms.directCommandRead(0x07); // Safety Status C
  controlStatus = bms.directCommandRead(0x00); // Control Status (for LD_TIMEOUT)
  ldPinVoltage = bms.directCommandRead(0x38); // LD Pin Voltage
  uint16_t alarms = bms.directCommandRead(0x62); // Read AlarmStatus
  
  // Determine if any real safety fault is active
  watchdogFaultLocked = (ssA_val != 0) || (ssB_val != 0) || (ssC_val != 0);

  if (watchdogFaultLocked) {
    // WATCHDOG: Do NOT clear the alarm. ALERT LED stays SOLID RED.
    // Serial.printf("[WATCHDOG] FAULT ACTIVE! SSA=0x%02X SSB=0x%02X SSC=0x%02X — ALERT LOCKED\n", ssA_val, ssB_val, ssC_val);
  } else if (alarms > 0) {
    // Normal heartbeat: clear the alarm so ALERT pin deasserts, LED blinks OFF
    bms.directCommandWrite(0x62, 0xFF); // Clear low byte alarms
    bms.directCommandWrite(0x63, 0xFF); // Clear high byte alarms
  }

  // Read Manufacturing Status to get FET_EN (bit 4) and SEC state (bits 1:0)
  manufStatus = bms.getManufacturingStatus();
  fetEn =
      (manufStatus & (1 << 4)) != 0; // Bit 4 = FET_EN in Manufacturing Status
  protStatus = bms.getProtectionStatus();
  tempStatus = bms.getTemperatureStatus();

  // ==================== I2C POLLING FOR BALANCING ====================
  charge = bms.getAccumulatedCharge();
  chargeTime =
      bms.AccumulatedChargeTime; // Already populated by getAccumulatedCharge()
  // THE TRUTH FIX: hardware status is now derived from the logic above (currentMask)
  // which prevents the 'Quantum Observation' collision.
  balancingMask = currentMask; 

  // Clean output for Arduino Serial Plotter
  Serial.printf("Pack_V:%.2f Current_mA:%d Die_Temp:%.1f\n", vPack / 1000.0,
                bmsCurrent, chipTemp);
}

// ==================== HOST BALANCING LOGIC ====================
void hostBalancingLoop() {
  uint16_t mask = balancingMask;

  // Diagnostic: print status every 10s so the user knows what's happening
  static unsigned long lastDiag = 0;
  bool showDiag = (millis() - lastDiag > 10000);

  if (!balancingEnabled) {
    hostRequestedMask = 0;
    if (isHardwareBalancing) {
      if (powerMode == 1) {
        wakeBms();
        delay(10);
      } // Wake to turn OFF!
      bms.setBalancingMask(0);
    }
    if (showDiag) {
      Serial.println("[HOST-BAL] Idle: Master OFF");
      lastDiag = millis();
    }
    return;
  }

  if (currentBalMode == BAL_MODE_AUTONOMOUS) {
    hostRequestedMask = 0;
    if (showDiag) {
      Serial.println("[HOST-BAL] Idle: Autonomous mode (HW controls)");
      lastDiag = millis();
    }
    return;
  }

  if (currentBalMode == BAL_MODE_MANUAL) {
    static unsigned long lastManualRefresh = 0;
    if (millis() - lastManualRefresh > 5000 && balancingMask > 0) {
      bms.setBalancingMask(balancingMask);
      lastManualRefresh = millis();
    }
    for (int i = 0; i < 16; i++) {
      if (balancingMask & (1 << i)) {
        cellBalancingTimes[i]++;
      }
    }
    return;
  }

  // ----- HOST ALGO MODE ENGINE -----
  static bool hostBalancingActive = false;
  static unsigned long lastBalRetry = 0;

  // 1. Hysteresis trigger: engage at >= delta, disengage only when delta drops
  // below (delta - 2mV). This matches the stable old dashboard behavior and
  // prevents measurement noise from rapidly flipping the engine on/off.
  uint16_t stopDelta = (hostBalDeltaMv > 2) ? (hostBalDeltaMv - 2) : 0;
  if (maxCellV >= hostBalTriggerMv && cellBalancingDelta >= hostBalDeltaMv) {
    hostBalancingActive = true;
  } else if (cellBalancingDelta <= stopDelta || maxCellV < hostBalTriggerMv) {
    hostBalancingActive = false;
  }
  // else: in the dead-band — keep the current state (no change)

  if (showDiag) {
    Serial.printf("[HOST-BAL] maxV=%d trig=%d delta=%d target=%d active=%d\n",
                  maxCellV, hostBalTriggerMv, cellBalancingDelta,
                  hostBalDeltaMv, hostBalancingActive);
    lastDiag = millis();
  }

  mask = 0;
  if (hostBalancingActive) {
    if (showDiag) Serial.println("[HOST-BAL-TRACE] Phase 1: Identifying cells to balance...");
    // Phase 1: Identify ALL cells that need balancing
    // Use the hysteresis stopDelta if the engine is already active
    uint16_t currentThreshold = (minCellV + (hostBalancingActive ? stopDelta : hostBalDeltaMv));
    
    bool wantsBalance[16] = {false};
    for (int i = 0; i < TB_CONNECTED_CELLS; i++) {
      if (cellVoltages[i] >= currentThreshold) {
        if (i == TB_CONNECTED_CELLS - 1) {
          wantsBalance[15] = true; // Cell 3 is mapped to VC16
          if (showDiag) Serial.printf("[HOST-BAL-TRACE] Cell %d (VC16) wants balance (V=%d, MinV=%d, Threshold=%d)\n", i+1, cellVoltages[i], minCellV, currentThreshold);
        } else {
          wantsBalance[i] = true;
          if (showDiag) Serial.printf("[HOST-BAL-TRACE] Cell %d wants balance (V=%d, MinV=%d, Threshold=%d)\n", i+1, cellVoltages[i], minCellV, currentThreshold);
        }
      }
    }

    if (showDiag) Serial.println("[HOST-BAL-TRACE] Phase 2: Adjacent cell resolution...");
    // Phase 2: ADJACENT CELL RESOLUTION (The TRM Fix)
    if (showDiag) Serial.println("[HOST-BAL-TRACE] Phase 2: Bitwise Adjacency Resolution...");
    // Phase 2: BITWISE ADJACENCY RESOLUTION
    // The BQ76952 silicon forbids balancing adjacent VC nodes (e.g., VC1 and VC2).
    // This loop scans the 16-bit mask for any N and N+1 conflicts and keeps the highest voltage cell.
    for (int i = 0; i < 15; i++) {
      if (wantsBalance[i] && wantsBalance[i + 1]) {
        // Voltage Lookup
        int vA = (i == 0) ? cellVoltages[0] : (i == 1) ? cellVoltages[1] : (i == 15) ? cellVoltages[2] : 0;
        int vB = (i+1 == 0) ? cellVoltages[0] : (i+1 == 1) ? cellVoltages[1] : (i+1 == 15) ? cellVoltages[2] : 0;

        if (showDiag) Serial.printf("[HOST-BAL-TRACE] Conflict: VC%d (%dmV) vs VC%d (%dmV)\n", i+1, vA, i+2, vB);
        
        if (vA >= vB) {
          wantsBalance[i+1] = false;
          if (showDiag) Serial.printf("[HOST-BAL-TRACE] Dropping VC%d\n", i+2);
        } else {
          wantsBalance[i] = false;
          if (showDiag) Serial.printf("[HOST-BAL-TRACE] Dropping VC%d\n", i+1);
        }
      }
    }

    if (showDiag) Serial.println("[HOST-BAL-TRACE] Phase 3: Constructing final mask...");
    // Phase 3: Construct the final safe mask
    for (int i = 0; i < 16; i++) {
      if (wantsBalance[i]) {
        mask |= (1 << i);
        if (showDiag) Serial.printf("[HOST-BAL-TRACE] Adding bit %d to mask\n", i);
      }
    }
    if (showDiag) Serial.printf("[HOST-BAL-TRACE] Final calculated mask: 0x%04X\n", mask);
  }

  // THE RACE CONDITION FIX: Compare what we WANT (mask) against what we
  // REQUESTED
  if (mask != hostRequestedMask || (millis() - lastBalRetry > 5000)) {
    Serial.printf("[HOST-BAL-STEP 1] Ready to send mask. Wanted: 0x%04X, Last "
                  "Sent: 0x%04X\n",
                  mask, hostRequestedMask);

    // The BQ76952 ignores 0x0083 subcommands if it is asleep!
    if (powerMode == 1) {
      Serial.println(
          "[HOST-BAL-STEP 2] PowerMode is SLEEP. Firing wakeBms()...");
      wakeBms();
      delay(10);
    } else {
      Serial.println("[HOST-BAL-STEP 2] PowerMode is ACTIVE. No wake needed.");
    }

    // 0. CLEAR SAFETY FAULTS (In case a previous glitch is blocking the FETs)
    Wire.beginTransmission(0x08);
    Wire.write(0x3E);
    Wire.write(0x1D); // 0x001D CLEAR_SAFETY
    Wire.write(0x00);
    Wire.endTransmission();
    delay(5);

    // THE FINAL BOSS WEAPON: Precise TRM Section 4.5.2 Execution Sequence
    uint8_t subL = 0x83;
    uint8_t subM = 0x00;
    uint8_t datL = mask & 0xFF;
    uint8_t datM = (mask >> 8) & 0xFF;
    uint8_t checksum = (uint8_t)~(subL + subM + datL + datM);
    uint8_t len = 0x06;

    Serial.printf("[HOST-BAL-STEP 3] Executing TRM MAC Sequence for Mask 0x%04X (CS=0x%02X)\n", mask, checksum);

    // 1. Write the 2-byte subcommand to 0x3E
    Wire.beginTransmission(0x08);
    Wire.write(0x3E);
    Wire.write(subL);
    Wire.write(subM);
    Wire.endTransmission();

    // 2. Write the 2-byte balancing mask to the transfer buffer at 0x40
    Wire.beginTransmission(0x08);
    Wire.write(0x40);
    Wire.write(datL);
    Wire.write(datM);
    Wire.endTransmission();

    // 3. Write Checksum (0x60) and Length (0x61) together
    Wire.beginTransmission(0x08);
    Wire.write(0x60);
    Wire.write(checksum);
    Wire.write(len);
    Wire.endTransmission();
    
    delay(100); // Let the silicon evaluate and execute

    // Let's see if the hardware finally acknowledges!
    uint16_t hardwareConfirmation = safeReadSubCommand(0x0083, 2);
    Serial.printf("[HOST-BAL-VERIFY] Hardware physically reports mask: 0x%04X\n", hardwareConfirmation);
    
    // SYNC WITH WEB DASHBOARD (The API depends on these)
    hwBalancingMask = hardwareConfirmation;
    isHardwareBalancing = (hardwareConfirmation != 0);
    hostRequestedMask = mask;
    lastBalRetry = millis();

    delay(20);
    Serial.println("[HOST-BAL-STEP 4] setBalancingMask completed.");
  }

  // Update cell timers for Host Algo mode
  if (balancingMask > 0) {
    totalBalancingTime += 1;
    for (int i = 0; i < 16; i++) {
      if (balancingMask & (1 << i)) {
        cellBalancingTimes[i]++;
      }
    }
  }
}

// Web handlers, HTML page, and API endpoints moved to web_api.h
#include "web_api.h"

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[TB-Dash] === BQ76952 Test Board Dashboard ===");

  // Load Custom Balancing Settings from NVS
  prefs.begin("bms_cfg", false);
  hostBalTriggerMv = prefs.getUShort("trig", 3400); 
  hostBalDeltaMv = prefs.getUShort("delta", 5);
  Serial.printf(
      "[TB-Dash] Loaded Host Balancing Settings: Trigger=%dmV, Delta=%dmV\n",
      hostBalTriggerMv, hostBalDeltaMv);

  // Start WiFi AP early so the dashboard link is always visible
  WiFi.mode(WIFI_AP);
  WiFi.softAP(TB_AP_SSID, TB_AP_PASSWORD);
  Serial.printf("[TB-Dash] AP: %s  IP: http://%s\n", TB_AP_SSID,
                WiFi.softAPIP().toString().c_str());

  Wire.begin(TB_I2C_SDA, TB_I2C_SCL, 400000); // Back to 400kHz
  bms.begin(TB_I2C_SDA, TB_I2C_SCL);
  bms.setDebug(false);

  // Set EKF Configuration (User Review Recommendations)
  ekf.setParasiticResistance(2.43f);
  ekf.setTrustGuardThreshold(0.120f); // Guard against pulse noise >120mV
  ekf.setMaxGainClamp(0.5f);          // 0.5% max SOC change per loop

  Serial.printf("[TB-Dash] I2C Master SDA=%d SCL=%d\n", TB_I2C_SDA, TB_I2C_SCL);

  bms.reset(); // RESTORED: Ensuring a clean boot state for testing
  delay(500);  // BQ76952 TRM requires 250ms+ after hardware reset before I2C is
               // ready

  // Re-initialize I2C bus after BQ reset — the reset can leave the bus in a bad
  // state
  Wire.end();
  delay(50);
  Wire.begin(TB_I2C_SDA, TB_I2C_SCL, 400000);
  delay(50);

  // Verify BQ is alive before proceeding
  unsigned int resetCheck = bms.directCommandRead(0x12);
  Serial.printf("[TB-Dash] Post-reset BatStatus=0x%04X (expect non-zero)\n",
                resetCheck);
  if (resetCheck == 0) {
    Serial.println("[TB-Dash] WARNING: BQ still not responding! Retrying...");
    delay(500);
    Wire.end();
    delay(50);
    Wire.begin(TB_I2C_SDA, TB_I2C_SCL, 400000);
    delay(50);
    resetCheck = bms.directCommandRead(0x12);
    Serial.printf("[TB-Dash] Retry BatStatus=0x%04X\n", resetCheck);
  }

  // 3. Push the Packed Configuration to the BQ76952
  // This reads saved balancing mode from NVS automatically!
  initBQ76952();

  // 4. Initialize the EKF Math Engine
  readBMSData(); // Refresh cell voltages once for init
  float V_init = (float)cellVoltages[0] / 1000.0f;
  float I_init = (float)bmsCurrent / 1000.0f;
  float SOC_init = smartSOCInit(V_init, I_init);
  initial_ekf_soc = SOC_init;
  ekf.setNNEnabled(true);
  ekf.setAdaptiveTuning(true);
  ekf.begin(SOC_init);
  Serial.printf("[EKF] Initialized with SOC=%.1f%%\n", SOC_init);

  // Initialize original testboard LEDs (Assuming active-low)
  pinMode(TB_PIN_DDSG_LED, OUTPUT);
  pinMode(TB_PIN_DCHG_LED, OUTPUT);
  digitalWrite(TB_PIN_DDSG_LED, HIGH);
  digitalWrite(TB_PIN_DCHG_LED, HIGH);

  // Initialize your NEW external LEDs (Assuming active-high, adjust if needed)
  pinMode(TB_PIN_CHG_EXT_LED, OUTPUT);
  pinMode(TB_PIN_DSG_EXT_LED, OUTPUT);
  digitalWrite(TB_PIN_CHG_EXT_LED, LOW); // Default OFF
  digitalWrite(TB_PIN_DSG_EXT_LED, LOW); // Default OFF

  Serial.printf("[TB-Dash] LED Pins: DDSG_LED=GPIO%d (OUTPUT SINK), "
                "DCHG_LED=GPIO%d (OUTPUT SINK)\n",
                TB_PIN_DDSG_LED, TB_PIN_DCHG_LED);

  isCharging = false;
  isDischarging = false;

  // 8. ALERT LED Control
  // The user confirmed J2 is identical to DCHG/DDSG.
  // This means the ESP32 must provide Ground (LOW), and the BQ drives 3.3V
  // (HIGH) to light it.
  pinMode(TB_PIN_ALERT, OUTPUT);
  digitalWrite(TB_PIN_ALERT, LOW);
  Serial.printf("[TB-Dash] ALERT LED: GPIO%d set to OUTPUT LOW (Ground Sink). "
                "LED acts as BQ heartbeat.\n",
                TB_PIN_ALERT);

  server.on("/", handleRoot);
  server.on("/api/data", handleApiData);
  server.on("/api/log", handleApiLog);
  server.on("/api/cmd", handleApiCmd);
  server.on("/api/ekf_reset", handleApiEKFReset);
  server.begin();

  // 9. ADC SETTLING: Wait for ADC to stabilize after config writes
  Serial.println(
      "[TB-Dash] Waiting 2s for ADC settling after config writes...");
  delay(2000);
  bms.directCommandWrite(0x62, 0xFF);
  bms.directCommandWrite(0x63, 0xFF);
  watchdogFaultLocked = false;
  Serial.println("[TB-Dash] Boot faults cleared. System ready.");

  Serial.printf("[TB-Dash] Connect to '%s' pw '%s' -> http://%s\n", TB_AP_SSID,
                TB_AP_PASSWORD, WiFi.softAPIP().toString().c_str());
}

// ==================== LOOP ====================
void loop() {
  uint32_t t0 = micros();
  server.handleClient();
  uint32_t dt = micros() - t0;
  // FIX: Ignore massive idle WiFi timeouts (>100ms) so they don't corrupt the
  // CPU load math
  if (dt < 100000 && dt > web_peak_us) {
    web_peak_us = dt;
  }

  // 1. ALARM REGISTER HEARTBEAT POLLING
  // Instead of blindly reading cell voltages every 500ms and risking aliasing
  // with the BQ76952's internal ADC loop, we rapidly poll the Alarm Status
  // register (0x62) every 20ms. When FULLSCAN (Bit 13) or a SAFETY ALERT (Bits
  // 0-7) goes HIGH, we KNOW the BQ has fresh data!
  static unsigned long lastAlarmPoll = 0;
  if (millis() - lastAlarmPoll >=
      150) { // 150ms — syncs with LOOP_SLOW ~500ms FULLSCAN
    lastAlarmPoll = millis();
    uint16_t alarms = bms.directCommandRead(0x62);

    // POWER TRANSITION GUARD
    // pendingPwrMode: 0=none, 1=SLEEP pending, 2=DEEPSLEEP pending, 3=WAKE
    // pending
    if (pendingPwrMode == 2) {
      // DEEPSLEEP PENDING: No DEEPSLEEP bit in BatStatus.
      // Since writes are ignored in DEEPSLEEP, we cannot clear the alarm
      // register to check it. TRM guarantees DEEPSLEEP after double command. We
      // trust the command after 2 seconds.
      if (millis() - pwrCommandTime > 2000) {
        powerMode = 2;
        pendingPwrMode = 0;
        Serial.println("[PWR] DEEPSLEEP confirmed by timer (2s).");
      }
    }
    // SLEEP PENDING or WAKE PENDING: Keep reading BatStatus so we can confirm
    // the transition! readBMSData() will check BatStatus bit 15 and
    // confirm/deny the transition.
    else if (pendingPwrMode == 1 || pendingPwrMode == 3) {
      // Force a read every ~500ms so we catch the transition quickly
      if ((alarms & 0x2000) != 0 || (millis() - lastRead > 500)) {
        lastRead = millis();
        t0 = micros();
        readBMSData();
        perf_i2c_us = micros() - t0;
      }
    }
    // Normal operation: trigger readBMSData() when fresh data is available
    // 0x2000 = FULLSCAN (fresh ADC data ready), 0x00FF = Any Safety Fault
    // active
    else if ((alarms & 0x2000) != 0 || (alarms & 0x00FF) != 0 ||
             (millis() - lastRead > 2000)) {
      lastRead = millis();
      t0 = micros();
      readBMSData();
      perf_i2c_us = micros() - t0;
    }
  }
  if (millis() - lastEKFUpdate >= EKF_UPDATE_INTERVAL) {
    lastEKFUpdate = millis();
    t0 = micros();
    updateEKF();
    hostBalancingLoop(); // Engage explicitly if Auto is manually toggled out
    perf_ekf_us = micros() - t0;

    // Lock in the peak web-API execution time over the last second for stable
    // UI monitoring
    perf_web_us = web_peak_us;
    web_peak_us = 0;
  }
  delay(2);
}
