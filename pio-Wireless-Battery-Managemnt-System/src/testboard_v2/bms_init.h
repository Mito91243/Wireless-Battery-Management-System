#pragma once
// ==============================================================
// bms_init.h — BQ76952 Hardware Configuration Packer
// ==============================================================
// All Data Memory writes (CONFIG_UPDATE-triggering) are collected
// here and executed exactly ONCE during setup(). The main .cpp
// stays clean and no runtime CONFIG_UPDATE bombs can occur.
//
// MUST be #included AFTER all globals (bms, prefs, helpers) are
// defined so the inline function can reference them.
// ==============================================================

#include "BQ76952.h"
#include "tb_config.h"

// Register aliases are #defines in BQ76952.h, included above

inline void initBQ76952() {
  Serial.println("[BMS-INIT] Packing and applying full BQ76952 configuration...");

  // 1. Wake and Unseal
  wakeBms();
  bms.unseal();
  delay(50);

  // 2. Core Power & VCell Routing
  // Read auto_sleep from NVS (Restart-to-Apply)
  bool autoSlp = prefs.getBool("auto_sleep", false);
  autoSleepEnabled = autoSlp;
  if (autoSlp) {
    bms.writeIntToMemory(0x9234, 0x298D); // LOOP_SLOW=3, SLEEP ENABLED
  } else {
    bms.writeIntToMemory(0x9234, 0x298C); // LOOP_SLOW=3, SLEEP DISABLED
  }
  Serial.printf("[BMS-INIT] Power Config: autoSleep=%d\n", autoSlp);
  bms.writeIntToMemory(0x9237, 0);
  bms.writeByteToMemory(0x9236, 0x0D);  // REG1 = 3.3V
  bms.setConnectedCells(TB_CONNECTED_CELLS);
  delay(100);
  bms.CommandOnlysubCommand(0x0092);     // EXIT_CFGUPDATE
  delay(200);
  bms.setConnectedCells(TB_CONNECTED_CELLS);
  bms.writeIntToMemory(0x9304, 0x8003);  // Cells 1, 2, 16

  // 3. Thermistors
  Serial.println("[BMS-INIT] Thermistors: TS1/TS2 OFF, HDQ+TS3 = NTC10K");
  bms.writeByteToMemory(0x92FD, 0x00); // TS1 DISABLED
  bms.writeByteToMemory(0x92FE, 0x00); // TS2 DISABLED
  bms.writeByteToMemory(0x9300, 0x07); // HDQ = NTC10K + 18kΩ
  bms.writeByteToMemory(0x92FF, 0x07); // TS3 = NTC10K + 18kΩ
  delay(50);

  // Verify HDQ
  byte *tsCheck = bms.readDataMemory(0x9300);
  if (tsCheck)
    Serial.printf("[BMS-INIT] HDQ readback: 0x%02X (expect 0x07)\n", tsCheck[0]);

  // 4. Protection Thresholds (Samsung 30Q + 1mΩ shunt)
  Serial.println("[BMS-INIT] Protection thresholds (30Q + 1mΩ)");
  bms.writeByteToMemory(0x9275, 54); // CUV: ~2.75V
  bms.writeByteToMemory(0x9276, 74); // CUV Delay: ~250ms
  bms.writeByteToMemory(0x9278, 81); // COV: ~4.1V
  bms.writeByteToMemory(0x9279, 74); // COV Delay: ~250ms
  bms.writeByteToMemory(0x9280, 3);  // OCC: 6A
  bms.writeByteToMemory(0x9281, 20); // OCC Delay: ~73ms
  bms.writeByteToMemory(0x9282, 6);  // OCD1: 12A
  bms.writeByteToMemory(0x9283, 60); // OCD1 Delay: ~201ms
  bms.writeByteToMemory(0x9284, 10); // OCD2: 20A
  bms.writeByteToMemory(0x9285, 14); // OCD2 Delay: ~50ms
  bms.writeByteToMemory(0x9286, 2);  // SCD: 40A
  bms.writeByteToMemory(0x9287, 2);  // SCD Delay: ~15µs
  delay(50);

  // 4.1 Enable V/I protections, DISABLE temperature protections
  bms.writeByteToMemory(0x9261, 0xFC); // Protections_A
  bms.writeByteToMemory(0x9262, 0x00); // Protections_B (temp OFF)
  bms.writeByteToMemory(0x9263, 0x00); // Protections_C
  delay(50);

  // 4.2 Permanent Failure & Autonomous Recovery Configuration (MAIN BOARD PREP)
  Serial.println("[BMS-INIT] Configuring PF and Recovery Logic (TOSF, Charger, LD)");
  
  // TOSF: Top of Stack Fault (1V threshold, 5s delay)
  // TODO (Main Board): Uncomment and use exact BQStudio addresses for 13S setup
  // bms.writeByteToMemory(0x92C3, 0x01); // Settings:Permanent Failure:Enabled PF D[TOSF]
  // bms.writeByteToMemory(0x92D1, 100);  // Permanent Fail:TOS:Threshold = 1V (100 * 10mV units)
  // bms.writeByteToMemory(0x92D2, 5);    // Permanent Fail:TOS:Delay = 5s

  // Charger Detection (Recovery from CUV)
  // bms.writeIntToMemory(CHG_DETECT_ADDRESS, 50); // Settings:Protection:Recovery:Charger Detect = 500mV (50 * 10mV)
  
  // Load Detect (Autonomous recovery from SCD/OCD)
  // bms.writeByteToMemory(LD_TIMEOUT_ADDRESS, 2); // Protections:Load Detect:Timeout = 2 seconds
  
  // Pre-Charge (PCHG) Logic
  // bms.writeIntToMemory(PCHG_START_ADDRESS, 2500); // Settings:Protection:Precharge Start Voltage = 2.5V
  // bms.writeIntToMemory(PCHG_STOP_ADDRESS, 3000);  // Settings:Protection:Precharge Stop Voltage = 3.0V
  
  // ----- TEST: READ THE DEFAULTS TO VERIFY ADDRESSES -----
  byte* pfEnD = bms.readDataMemory(0x92C3);
  byte* tosThresh = bms.readDataMemory(0x92D1);
  byte* tosDelay = bms.readDataMemory(0x92D2);
  
  Serial.printf("[BMS-INIT] TEST READ -> Enabled PF D (0x92C3) expected 0x00: 0x%02X\n", pfEnD ? pfEnD[0] : 0xFF);
  Serial.printf("[BMS-INIT] TEST READ -> PF TOS Thresh (0x92D1) expected ~100: %d\n", tosThresh ? tosThresh[0] : -1);
  Serial.printf("[BMS-INIT] TEST READ -> PF TOS Delay (0x92D2) expected ~5: %d\n", tosDelay ? tosDelay[0] : -1);

  delay(50);


  // 5. FET Control & Manufacturing Init
  Serial.println("[BMS-INIT] FET Options -> 0x0D (Autonomous)");
  bms.writeByteToMemory(0x9308, 0x0D);
  delay(50);

  // Mfg Status Init: FET_EN=1, PF_EN=1 (survives CONFIG_UPDATE exits)
  Serial.println("[BMS-INIT] Mfg Status Init -> 0x0050 (FET_EN + PF_EN)");
  bms.writeIntToMemory(0x9343, 0x0050);
  delay(50);

  // CFETOFF / DFETOFF hardware pin enable (Lobotomized)
  bms.writeByteToMemory(0x92FA, 0x00);
  bms.writeByteToMemory(0x92FB, 0x00);
  delay(50);

  // 6. Pin Configuration (Hardware Lobotomy)
  bms.writeByteToMemory(0x92FC, 0x2A); // ALERT: Keep this enabled! (Active Drive, REG1/3.3V)
  bms.writeByteToMemory(0x926D, 0x01); // Alarm Mask Low (Safety)
  bms.writeByteToMemory(0x926E, 0x20); // Alarm Mask High (FULLSCAN)
  
  // The hardware team left these floating or wired them poorly.
  // We write 0x00 to disable them entirely inside the BQ76952 hardware.
  bms.writeByteToMemory(0x9301, 0x00); // DCHG Pin = Disabled
  bms.writeByteToMemory(0x9302, 0x00); // DDSG Pin = Disabled
  delay(50);

  // 7. DYNAMIC BALANCING MODE (Restart-to-Apply)
  // FORCED TO HOST ALGO FOR DEBUGGING
  int savedBalMode = prefs.getInt("bal_mode", BAL_MODE_HOST_ALGO);
  currentBalMode = BAL_MODE_HOST_ALGO; // FORCED
  balancingEnabled = true; // FORCED
  bool balChg = prefs.getBool("bal_chg", true);
  bool balRlx = prefs.getBool("bal_rlx", true);

  Serial.printf("[BMS-INIT] Loaded: balMode=%d, master=%d, chg=%d, rlx=%d\n",
                currentBalMode, balancingEnabled, balChg, balRlx);

  // Build the 0x9335 config byte.
  // THE 0x4C BASELINE (The Truth):
  // Bit 6 (CB_EN) = 1: Master Enable ON.
  // Bit 3 (CB_NOSLEEP) = 1: Host commands allowed while awake.
  // Bit 2 (CB_SLEEP) = 1: Host commands allowed while asleep.
  // Bits 0 & 1 (CB_CHG & CB_RLX) = 0: Autonomous rules OFF.
  uint8_t balCfg = 0x4C; // Default to Host-Ready (0x4C)
  if (balancingEnabled && currentBalMode == BAL_MODE_AUTONOMOUS) {
    balCfg = 0x4F; // CB_EN + CB_SLEEP + CB_NOSLEEP + CB_CHG + CB_RLX
    if (!balChg) balCfg &= ~0x01; 
    if (!balRlx) balCfg &= ~0x02; 
  }
  bms.writeByteToMemory(Balancing_Configuration, balCfg);
  bms.writeByteToMemory(Cell_Balance_Max_Cells, 3);
  bms.writeIntToMemory(Cell_Balance_Min_Cell_V_Relaxed, 2500);

  // Override charge & relax limits for immediate engagement
  bms.writeIntToMemory(0x933B, 2500);  // Min Cell V (Charge)
  bms.writeByteToMemory(0x933D, 1);    // Min Delta (Charge)
  bms.writeByteToMemory(0x933E, 1);    // Stop Delta (Charge)
  bms.writeIntToMemory(0x933F, 2500);  // Min Cell V (Relax)
  bms.writeByteToMemory(0x9341, 1);    // Min Delta (Relax)
  bms.writeByteToMemory(0x9342, 1);    // Stop Delta (Relax)
  bms.writeByteToMemory(0x9339, 10);   // Interval: 10s
  delay(250);

  // 8. Post-config verification
  unsigned int batStat = bms.directCommandRead(0x12);
  Serial.printf("[BMS-INIT] Final BatStatus=0x%04X (SEC1:0=%d)\n",
                batStat, (batStat >> 8) & 0x03);

  Serial.println("\n[BMS-INIT VERIFY]");
  for (uint16_t addr = 0x9335; addr <= 0x9345; addr++) {
    byte *val = bms.readDataMemory(addr);
    if (val)
      Serial.printf("  0x%04X: 0x%02X (%d)\n", addr, val[0], val[0]);
  }
  Serial.println("=================================\n");

  // 9. Finalize & Kickstart
  wakeBms();
  rearmAlerts();
  Serial.println("[BMS-INIT] Configuration packed and successfully applied.");
}
