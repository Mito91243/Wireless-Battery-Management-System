# wBMS Slave Board — Complete Feature Implementation Plan

**Author:** Mahmoud + AI Assistant  
**Date:** 2026-06-05  
**Target:** ESP32-S3 Slave Board, 13S3P Pack, BQ76952 AFE  
**Files Modified:** `bms_init.h`, `sender.cpp`, `web_api.h`

---

## Table of Contents

1. [Feature 1: Protection B & C Activation](#feature-1)
   - 1.1 Background
   - 1.2 Register Address Map (TRM-verified)
   - 1.3 CRITICAL: Existing Address Bugs Found
   - 1.4 Exact Code Changes in `bms_init.h`
   - 1.5 Dashboard Changes in `web_api.h`
2. [Feature 2: DCIR Measurement](#feature-2)
   - 2.1 Background & Physics
   - 2.2 State Machine Design
   - 2.3 Exact Code Changes in `sender.cpp`
   - 2.4 Dashboard + JSON API Changes in `web_api.h`
3. [Feature 3: EKF & Current Calibration (Post-Upload)](#feature-3)
4. [Verification Checklist](#verification)

---

<a name="feature-1"></a>
## Feature 1 — Protection B & C Activation

### 1.1 Background

The BQ76952 has three Protection Enable registers that control which safety features are active:

| Register | Address | Name | Current Value | Purpose |
|----------|---------|------|---------------|---------|
| Prot A | `0x9261` | Enabled Protections A | `0xFC` | Voltage + Current protections (already ON) |
| Prot B | `0x9262` | Enabled Protections B | `0x00` | Temperature protections (currently OFF) |
| Prot C | `0x9263` | Enabled Protections C | `0x00` | Open Wire + Latch protections (currently OFF) |

**What we are enabling:**

#### Protection B — Temperature Faults (Register 0x9262)
```
Bit 7: OTF  (Overtemp FET)           → ENABLE
Bit 6: OTINT (Overtemp Internal)     → leave OFF (die temp, not critical for us)
Bit 5: OTD  (Overtemp Discharge)     → ENABLE
Bit 4: OTC  (Overtemp Charge)        → ENABLE
Bit 3: Reserved
Bit 2: UTINT (Undertemp Internal)    → leave OFF
Bit 1: UTD  (Undertemp Discharge)    → leave OFF (Egypt, not cold climate)
Bit 0: UTC  (Undertemp Charge)       → leave OFF

Result: 0xB0 = 1011 0000
```

**What each protection does when triggered:**
- **OTC (Overtemp Charge):** Opens the CHG FET → stops charging current. Protects cells from thermal runaway during charge.
- **OTD (Overtemp Discharge):** Opens the DSG FET → stops discharge current. Protects cells from overheating under load.
- **OTF (Overtemp FET):** Opens ALL FETs → complete isolation. Protects the MOSFETs themselves from melting. Uses the CFETOFF thermistor which is physically mounted on the FET heatsink.

#### Protection C — Open Wire Detection (Register 0x9263)
```
Bit 7: HWDF (Hardware Diagnostic Fail) → ENABLE
Bit 6: COVL (reserved for COV latch)   → leave OFF
Bit 5: RSVD
Bit 4: PCHG_OVRD
Bit 3: SCDL (SCD Latch)               → leave OFF
Bit 2: OCDL (OCD Latch)               → leave OFF
Bit 1: Reserved
Bit 0: Reserved

Result: 0x80 = 1000 0000
```

**What HWDF (Open Wire) does:**
The BQ76952 periodically checks if all cell sense wires are properly connected. It does this by toggling a small test current through each VC pin and verifying the expected voltage response. If a wire is broken, disconnected, or has a bad solder joint, the HWDF fault fires. This is critical for a 13S pack because a broken sense wire means the BQ76952 cannot measure that cell's voltage, which can lead to silent overcharge or overdischarge.

---

### 1.2 Register Address Map (Verified from TRM Pages 202–204)

#### Temperature Protection Threshold Registers

| Parameter | TRM Name | Address | Size | Unit | Default | We Set |
|-----------|----------|---------|------|------|---------|--------|
| OTC Threshold | `Protections:OTC:Threshold` | **`0x929A`** | I1 (signed byte) | °C | 55 | **50** |
| OTC Delay | `Protections:OTC:Delay` | **`0x929B`** | U1 (unsigned byte) | s | 10 | **5** |
| OTC Recovery | `Protections:OTC:Recovery` | **`0x929C`** | I1 | °C | 50 | *leave default* |
| OTD Threshold | `Protections:OTD:Threshold` | **`0x929D`** | I1 | °C | 60 | **60** |
| OTD Delay | `Protections:OTD:Delay` | **`0x929E`** | U1 | s | 10 | **5** |
| OTD Recovery | `Protections:OTD:Recovery` | **`0x929F`** | I1 | °C | 55 | *leave default* |
| OTF Threshold | `Protections:OTF:Threshold` | **`0x92A0`** | I1 | °C | 100 | **80** |
| OTF Delay | `Protections:OTF:Delay` | **`0x92A1`** | U1 | s | 10 | **5** |
| OTF Recovery | `Protections:OTF:Recovery` | **`0x92A2`** | I1 | °C | 90 | *leave default* |

> **IMPORTANT:** All temperature threshold registers are **I1** (signed 1-byte integer) in **°C** directly. NOT in 0.1°C. You write the temperature as a plain number: 50 = 50°C. No multiplication needed.

#### Thermistor Source Mapping

The BQ76952 needs to know which thermistors feed which temperature protections. This is NOT in the 0x929x range — it is configured through:
- **TS1, TS3, HDQ, CFETOFF Config registers** (0x92FD, 0x92FF, 0x9300, 0x92FA) — already configured in our code as NTC10K thermistors.
- The BQ76952 automatically uses all enabled cell thermistors (TS1, TS3, HDQ) for OTC/OTD, and the FET thermistor (CFETOFF) for OTF based on the thermistor type configuration.

Our thermistors are already configured correctly:
| Thermistor | Address | Value | Role |
|-----------|---------|-------|------|
| TS1 | 0x92FD | 0x07 (NTC10K) | Cell temperature → feeds OTC/OTD |
| TS3 | 0x92FF | 0x07 (NTC10K) | Cell temperature → feeds OTC/OTD |
| HDQ | 0x9300 | 0x07 (NTC10K) | Cell temperature → feeds OTC/OTD |
| CFETOFF | 0x92FA | 0x07 (NTC10K) | FET temperature → feeds OTF |

#### Open Wire / Hardware Diagnostic Registers

| Parameter | TRM Name | Address | Size | Default | We Set |
|-----------|----------|---------|------|---------|--------|
| HWD Delay | `Protections:HWD:Delay` | **`0x92B2`** | U2 (2 bytes) | 60s | *leave default* |

The delay is how many seconds the BQ76952 waits without host communication before triggering a host watchdog fault. We leave this at default 60s. The HWDF (open wire) check happens independently as part of every FULLSCAN cycle — it does not need separate threshold configuration.

#### Load Detect Registers (CORRECTED)

| Parameter | TRM Name | Address | Size | Default |
|-----------|----------|---------|------|---------|
| LD Active Time | `Protections:Load Detect:Active Time` | **`0x92B4`** | U1 | 0s |
| LD Retry Delay | `Protections:Load Detect:Retry Delay` | **`0x92B5`** | U1 | 50s |
| LD Timeout | `Protections:Load Detect:Timeout` | **`0x92B6`** | U2 | 1 hr |

---

### 1.3 CRITICAL: Two Address Bugs Found in Current Code

While verifying every address against the TRM (pages 202–203), we discovered that **two register writes** in `bms_init.h` (lines 115 and 118) are targeting the **wrong addresses**. Here is a full explanation of each:

---

#### Bug 1: Line 115 — "Charger Detect" Does Not Exist at 0x929F

**Current code (line 115 of `bms_init.h`):**
```cpp
bms.writeByteToMemory(0x929F, 50);    // Settings:Protection:Recovery:Charger Detect = 500mV (50 * 10mV)
```

**What the comment claims:** This writes a charger detect voltage threshold (500mV) so the BQ76952 can detect when a charger is plugged in and recover from CUV faults.

**What actually happens:** Address `0x929F` is **NOT** a "Charger Detect" register. According to the TRM (page 203, Section 13.6.13.3), address `0x929F` is:

| Address | TRM Name | Type | Unit | Default |
|---------|----------|------|------|---------|
| `0x929F` | `Protections:OTD:Recovery` | I1 (signed byte) | °C | 55 |

This register controls the **temperature at which an OTD (Overtemp Discharge) fault auto-clears**. Writing `50` set OTD Recovery to 50°C. Since Protection B was disabled (`0x00`), OTD never fires anyway, so this write had zero visible effect — it was harmless but meaningless.

**So where is the actual charger detection?**

The BQ76952 does **NOT** have a single "Charger Detect threshold" register that you configure. Instead, charger detection is a **built-in hardware function** that works automatically:

```
┌──────────────────────────────────────────────────────────┐
│  HOW THE BQ76952 DETECTS A CHARGER (automatic, no config)│
│                                                          │
│  1. A fault trips (e.g., CUV) → DSG FET opens           │
│  2. Pack is now isolated from the load                   │
│  3. The BQ76952 continuously monitors the PACK pin       │
│  4. If someone connects a charger:                       │
│     → V_PACK rises above V_Stack (cell sum)              │
│     → The chip sees external voltage on PACK pin         │
│     → "Charger detected!" (hardware logic)               │
│  5. Chip activates PCHG FET for trickle precharge        │
│  6. Once cells > Precharge Stop Voltage → CHG FET ON     │
│                                                          │
│  The PACK pin comparison is hardwired in silicon.        │
│  You do NOT need to write any register to enable this.   │
└──────────────────────────────────────────────────────────┘
```

The **only related registers** we configure are the precharge voltage thresholds, which are already correct at lines 121–122:
```cpp
bms.writeIntToMemory(0x930B, 2500);   // Precharge Start Voltage = 2.5V (below this → PCHG ON)
bms.writeIntToMemory(0x930D, 3000);   // Precharge Stop Voltage = 3.0V (above this → PCHG OFF, CHG ON)
```

**Fix:** Delete line 115 entirely. It was writing to the wrong address and the feature it tried to configure doesn't need a register write — it's automatic hardware.

**Impact of deleting it:** None. The value `50` at `0x929F` (OTD Recovery = 50°C) is close to the default (55°C) and didn't matter since OTD was disabled. Now that we ARE enabling OTD, we will leave `0x929F` at its default 55°C which is correct (OTD trips at 60°C, recovers at 55°C = 5°C hysteresis).

---

#### Bug 2: Line 118 — "Load Detect Timeout" Writes to OTF Threshold Address

**Current code (line 118 of `bms_init.h`):**
```cpp
bms.writeByteToMemory(0x92A0, 2);     // Protections:Load Detect:Timeout = 2 seconds
```

**What the comment claims:** This sets the Load Detect timeout to 2 seconds, so the BQ76952 checks if the load (e.g., motor) is removed after an SCD/OCD latch fault.

**What actually happens:** Address `0x92A0` is **NOT** Load Detect. According to the TRM (page 203, Section 13.6.14.1):

| Address | TRM Name | Type | Unit | Default |
|---------|----------|------|------|---------|
| `0x92A0` | `Protections:OTF:Threshold` | I1 (signed byte) | °C | 100 |

This is the **OTF (Overtemp FET) trigger temperature**. Writing `2` set OTF to trigger at **2°C** — meaning if anyone had enabled Protection B, the FETs would have immediately opened because the FET temperature is obviously above 2°C! Luckily, Prot B was `0x00`, so OTF was disabled and this write was invisible.

**So where is Load Detect actually configured?**

The correct Load Detect registers are at a completely different address range (TRM page 188, Section 13.6.21):

| Address | TRM Name | Type | Unit | Default | Meaning |
|---------|----------|------|------|---------|---------|
| `0x92B4` | `Load Detect:Active Time` | U1 | seconds | **0** | How long LD current source is ON |
| `0x92B5` | `Load Detect:Retry Delay` | U1 | seconds | 50 | Pause between LD checks |
| `0x92B6` | `Load Detect:Timeout` | U2 | **hours** | 1 | Max time to keep checking |

> **IMPORTANT NOTE:** The default for Active Time is **0**, and the TRM explicitly states:
> *"0 = Recovery based on Load Detect functionality is **disabled**."*

**This means Load Detect was NEVER actually enabled.** The code tried to write `2` to enable it with a 2-second active time, but it wrote to the wrong address (`0x92A0` = OTF Threshold instead of `0x92B4` = LD Active Time).

**What is Load Detect and do we need it?**

```
┌──────────────────────────────────────────────────────────┐
│  LOAD DETECT — Recovery from SCD/OCD Latch Faults       │
│                                                          │
│  When an SCD or OCD LATCH fault fires, the DSG FET       │
│  stays permanently open (latched). To auto-recover:      │
│                                                          │
│  1. BQ76952 enables a current source on the LD pin       │
│  2. If load is removed → LD pin voltage > 3V (open)     │
│  3. "Load removed!" → chip clears the latch fault        │
│  4. If load still connected → LD pin stays low           │
│  5. Wait Retry Delay seconds, try again                  │
│  6. Give up after Timeout hours                          │
│                                                          │
│  NOTE: Our slave board has the LD pin routed.            │
│  If we enable Active Time > 0, the chip will             │
│  automatically try to recover from SCD/OCD latches.      │
└──────────────────────────────────────────────────────────┘
```

**Fix:** Write to the correct address to actually enable Load Detect:
```cpp
bms.writeByteToMemory(0x92B4, 2);    // Load Detect: Active Time = 2 seconds (ENABLES LD recovery)
// 0x92B5: Retry Delay → leave at default 50 seconds
// 0x92B6: Timeout → leave at default 1 hour
```

**Impact:** After this fix, Load Detect will actually work for the first time. If an SCD/OCD latch fault occurs and the user disconnects the load, the BQ76952 will detect that and auto-clear the latch within ~2 seconds. Previously, latch faults could only be cleared by a host subcommand or a full power cycle.

---

#### Summary of Both Bugs

| Line | What code says | What it actually does | Impact | Fix |
|------|---------------|----------------------|--------|-----|
| 115 | "Charger Detect = 500mV" | Writes OTD Recovery = 50°C | None (OTD was disabled, value is sane) | Delete the line. Charger detect is automatic hardware. |
| 118 | "Load Detect Timeout = 2s" | Writes OTF Threshold = 2°C | None (OTF was disabled) BUT Load Detect was never enabled | Change address from `0x92A0` to `0x92B4`. This actually enables LD for the first time. |

---

#### Bug 3: Dead GPIO Code — CFETOFF/DFETOFF Pins Not Wired

**Six locations in `web_api.h`** contain `digitalWrite(MB_PIN_CFETOFF/DFETOFF, ...)` calls that do absolutely nothing:

```cpp
// tb_config.h line 41-44:
// ==================== FET OVERRIDE PINS (NOT WIRED ON SLAVE BOARD) ====================
const int MB_PIN_CFETOFF = -1;   // ← NOT CONNECTED
const int MB_PIN_DFETOFF = -1;   // ← NOT CONNECTED
```

All six calls are guarded by `if (MB_PIN_CFETOFF >= 0)` which is always `false` since the pin is `-1`. The code compiles but does nothing at runtime.

**Why CFETOFF can NEVER be used as FET control on our board:**  
The CFETOFF pin on the BQ76952 is configured as `0x07` (NTC10K thermistor) at address `0x92FA`. It's physically connected to an NTC thermistor on the FET heatsink and reads the FET temperature that feeds the OTF (Overtemp FET) protection we're now enabling. You cannot simultaneously use it as a digital output to kill the CHG gate.

**Lines to clean up in `web_api.h`:**

| Line | Dead Code | In Function |
|------|----------|-------------|
| 1306 | `if (MB_PIN_CFETOFF >= 0) digitalWrite(MB_PIN_CFETOFF, LOW);` | `chgOn` |
| 1315 | `if (MB_PIN_CFETOFF >= 0) digitalWrite(MB_PIN_CFETOFF, HIGH);` | `chgOff` |
| 1325 | `if (MB_PIN_DFETOFF >= 0) digitalWrite(MB_PIN_DFETOFF, LOW);` | `dsgOn` |
| 1334 | `if (MB_PIN_DFETOFF >= 0) digitalWrite(MB_PIN_DFETOFF, HIGH);` | `dsgOff` |
| 1344-1345 | Both CFETOFF + DFETOFF LOW | `allFetsOn` |
| 1361-1362 | Both CFETOFF + DFETOFF HIGH | `allFetsOff` |

**Fix:** Remove all six lines. FET control works exclusively through I2C toggle subcommands (0x001D, 0x001E, 0x001F, 0x0020) which are already in the code and working.

**Also note:** The `allFetsOff` command (line 1359) does NOT send any subcommand — it only does the dead GPIO writes and reads back FET status. After removing the GPIO lines, `allFetsOff` will need the `ALL_FETS_OFF` subcommand (0x0095) added:

```cpp
} else if (strcmp(action, "allFetsOff") == 0) {
    Serial.println("=== ALL FETS OFF (ENTER MAINTENANCE) ===");
    bms.CommandOnlysubCommand(0x0095);  // ALL_FETS_OFF subcommand
    delay(100);
    uint16_t stat = bms.directCommandRead(0x7F);
    isCharging = (stat & 0x01) != 0;
    isDischarging = (stat & 0x04) != 0;
    return RC_OK;
}
```

---

### 1.4 Exact Code Changes in `bms_init.h`

**Replace lines 100–118** with the following block:

```cpp
  // 4.1 Enable ALL protections: V/I + Temperature + Open Wire
  //   Prot A (0x9261): 0xFC = COV, CUV, OCC, OCD1, OCD2, SCD     (unchanged)
  //   Prot B (0x9262): 0xB0 = OTF(b7) + OTD(b5) + OTC(b4)        (NEW)
  //   Prot C (0x9263): 0x80 = HWDF(b7) = Open Wire detection      (NEW)
  bms.writeByteToMemory(0x9261, 0xFC); // Protections_A: V/I protections
  bms.writeByteToMemory(0x9262, 0xB0); // Protections_B: OTF + OTD + OTC
  bms.writeByteToMemory(0x9263, 0x80); // Protections_C: HWDF (Open Wire)
  delay(50);

  // 4.1.1 Temperature Protection Thresholds (TRM Section 13.6.12-13.6.14)
  //
  //   OTC (Overtemp Charge): Protects cells during charging
  //     - Threshold: 50°C (opens CHG FET if any cell thermistor reads >= 50°C)
  //     - Delay: 5 seconds (must sustain for 5s before tripping)
  //     - Recovery: default (auto-recovers when temp drops to OTC Recovery temp)
  //
  //   OTD (Overtemp Discharge): Protects cells during discharge
  //     - Threshold: 60°C (opens DSG FET if any cell thermistor reads >= 60°C)
  //     - Delay: 5 seconds
  //
  //   OTF (Overtemp FET): Protects the MOSFETs from thermal damage
  //     - Threshold: 80°C (opens ALL FETs if CFETOFF thermistor reads >= 80°C)
  //     - Delay: 5 seconds
  //
  //   Register format: I1 (signed byte) in °C directly. 50 = 50°C.
  //   Addresses verified from TRM Table 13-47 (page 202-203):
  //     OTC: 0x929A (Thresh), 0x929B (Delay), 0x929C (Recovery)
  //     OTD: 0x929D (Thresh), 0x929E (Delay), 0x929F (Recovery)
  //     OTF: 0x92A0 (Thresh), 0x92A1 (Delay), 0x92A2 (Recovery)
  bms.writeByteToMemory(0x929A, 50);   // OTC Threshold = 50°C
  bms.writeByteToMemory(0x929B, 5);    // OTC Delay = 5s
  // 0x929C: OTC Recovery → leave at default (50°C)
  bms.writeByteToMemory(0x929D, 60);   // OTD Threshold = 60°C
  bms.writeByteToMemory(0x929E, 5);    // OTD Delay = 5s
  // 0x929F: OTD Recovery → leave at default (55°C)
  bms.writeByteToMemory(0x92A0, 80);   // OTF Threshold = 80°C
  bms.writeByteToMemory(0x92A1, 5);    // OTF Delay = 5s
  // 0x92A2: OTF Recovery → leave at default (90°C)
  delay(50);

  Serial.println("[BMS-INIT] Temp protections ENABLED: OTC=50C, OTD=60C, OTF=80C (5s delay each)");
  Serial.println("[BMS-INIT] Open Wire detection ENABLED (HWDF in Prot_C)");

  // 4.2 Permanent Failure & Autonomous Recovery Configuration
  Serial.println("[BMS-INIT] Configuring PF and Recovery Logic (TOSF, LD)");
  
  // TOSF: Top of Stack Fault (3V threshold, 5s delay) — unchanged
  bms.writeByteToMemory(0x92C3, 0x01); // Enabled PF D[TOSF]
  bms.writeIntToMemory(0x92D1, 300);   // PF TOS Threshold = 3V (300 * 10mV)
  bms.writeByteToMemory(0x92D2, 5);    // PF TOS Delay = 5s

  // Load Detect (Autonomous recovery from SCD/OCD latch faults)
  // CORRECTED ADDRESSES — old code wrote to 0x92A0 which is OTF Threshold!
  // TRM addresses: 0x92B4 (Active Time), 0x92B5 (Retry Delay), 0x92B6 (Timeout)
  bms.writeByteToMemory(0x92B4, 2);    // LD Active Time = 2 seconds
  // 0x92B5, 0x92B6: leave at defaults (50s retry, 1hr timeout)
  
  // Pre-Charge (PCHG) Logic — unchanged
  bms.writeIntToMemory(0x930B, 2500);  // Precharge Start Voltage = 2.5V
  bms.writeIntToMemory(0x930D, 3000);  // Precharge Stop Voltage = 3.0V
```

**Also update the verification readback (lines 124-134)** to add temperature threshold verification:

```cpp
  // ----- READBACK VERIFICATION -----
  byte* pfEnD = bms.readDataMemory(0x92C3);
  Serial.printf("[BMS-INIT] PF Enabled D (0x92C3): 0x%02X (expect 0x01)\n", pfEnD ? pfEnD[0] : 0xFF);

  byte* protB = bms.readDataMemory(0x9262);
  Serial.printf("[BMS-INIT] Prot B (0x9262): 0x%02X (expect 0xB0)\n", protB ? protB[0] : 0xFF);
  byte* protC = bms.readDataMemory(0x9263);
  Serial.printf("[BMS-INIT] Prot C (0x9263): 0x%02X (expect 0x80)\n", protC ? protC[0] : 0xFF);

  byte* otcT = bms.readDataMemory(0x929A);
  byte* otdT = bms.readDataMemory(0x929D);
  byte* otfT = bms.readDataMemory(0x92A0);
  Serial.printf("[BMS-INIT] OTC=%d°C OTD=%d°C OTF=%d°C (expect 50/60/80)\n",
                otcT ? (int8_t)otcT[0] : -99,
                otdT ? (int8_t)otdT[0] : -99,
                otfT ? (int8_t)otfT[0] : -99);
```

---

### 1.5 Dashboard Changes in `web_api.h`

The existing Protection tab already parses Safety Status C bits, including HWDF (bit 7). The `SS_C_BITS` array at line 843 already defines `{b:7,n:'HWDF',d:'HW Diag Fail'}`. When open wire triggers, this bit will show red in the Protection tab automatically.

**One addition needed** — show the HWDF (Open Wire) fault in the dashboard's quick-view fault flags. In the `updateUI()` function, find the fault flags section (around line 591) and add one line:

**Current (line 591):**
```javascript
    addF('OTF', d.temp_otf); addF('OTINT', d.temp_oti); addF('OTD', d.temp_otd); addF('OTC', d.temp_otc);
```

**New (add after it):**
```javascript
    addF('OTF', d.temp_otf); addF('OTINT', d.temp_oti); addF('OTD', d.temp_otd); addF('OTC', d.temp_otc);
    addF('WIRE', d.ssC & 0x80);
```

This adds an "WIRE" red badge to the dashboard when Open Wire is detected.

---

<a name="feature-2"></a>
## Feature 2 — DCIR Measurement

### 2.1 Background & Physics

**What is DCIR?**
Direct Current Internal Resistance. Every lithium-ion cell has internal resistance that causes a voltage drop (or rise) when current flows:

```
V_measured = OCV ± (I × R_internal)
```

**Why it matters for SOH:**
As a cell ages, its internal resistance increases. A fresh cell might have 27 mΩ, while a degraded cell (80% SOH) might have 35 mΩ. By measuring DCIR periodically, we can track degradation.

**How we measure it (load-step method):**

```
1. Record cell voltages while no current flows      → V_before
2. Wait for charger to connect (current step)
3. Wait 1 second for voltage to settle
4. Record cell voltages under load                    → V_after
5. Calculate: R = |V_before - V_after| / |I_before - I_after|
```

**The 13S3P correction:**
Each BQ76952 voltage channel measures a **group of 3 parallel cells**. The measured group resistance is `R_cell / 3` because three resistors in parallel give 1/3 the resistance. To get the single-cell DCIR for comparison with our lookup table:

```
R_single_cell = R_measured_group × 3
```

**Lookup Table Reference (from our RPT data analysis, fresh cell at 100% SOH):**

| SOC% | Expected DCIR (single cell) | BQ76952 will read (÷3 for group) |
|------|---------------------------|----------------------------------|
| 30% | 24.4 mΩ | 8.1 mΩ |
| 40% | 25.7 mΩ | 8.6 mΩ |
| 50% | 27.0 mΩ | 9.0 mΩ |
| 60% | 25.3 mΩ | 8.4 mΩ |

**SOC Gate:** We only measure DCIR when SOC is between 25% and 65%. Outside this range, the measurements are unreliable (resistance inflates at low SOC due to charge-transfer kinetics).

---

### 2.2 State Machine Design

A non-blocking state machine that runs inside `loop()` without using `delay()`:

```
IDLE ─────────────→ ARMED ──────────→ CAPTURE ──────→ COOLDOWN ────→ IDLE
     |ΔI| > 2000mA        1s elapsed          compute         30s elapsed
     SOC 25-65%                                results
```

**State Descriptions:**

| State | What it does | Duration | Serial Output |
|-------|-------------|----------|---------------|
| **IDLE** | Every 500ms, silently saves current cell voltages and current as "before" baseline. Monitors for current step > 2A. Checks SOC gate. | Indefinite | *(none)* |
| **ARMED** | Locks the "before" snapshot. Waits 1 second for ohmic + charge-transfer resistance to settle (ignoring slow diffusion). | 1000 ms | `[DCIR] ARMED! dI=3200 mA, SOC=42.1%` |
| **CAPTURE** | Reads current voltages as "after" snapshot. Calculates `|ΔV|/|ΔI|×3` for each of the 13 cells. Computes average. Saves to history buffer. | Instant | Full per-cell table |
| **COOLDOWN** | Blocks new measurements for 30 seconds to prevent spam from fluctuating loads. | 30000 ms | `[DCIR] Ready for next measurement.` |

---

### 2.3 Exact Code Changes in `sender.cpp`

#### Change 1: Add DCIR globals

**Location:** Insert after line 132 (after `float g_soh_pct` declaration)

```cpp
// ==================== DCIR MEASUREMENT ====================
enum DcirState { DCIR_IDLE, DCIR_ARMED, DCIR_CAPTURE, DCIR_COOLDOWN };
DcirState dcirState = DCIR_IDLE;
unsigned long dcirStateTime = 0;
unsigned long dcirBaselineTime = 0;

// Snapshots — "before" and "after" the current step
unsigned int dcir_before_V[16] = {0};  // Cell voltages before load (mV)
int          dcir_before_I     = 0;    // Pack current before load (mA, from cc1_raw)
unsigned int dcir_after_V[16]  = {0};  // Cell voltages after load (mV)
int          dcir_after_I      = 0;    // Pack current after load (mA)

// Results — computed from snapshots
float dcir_results_mOhm[16] = {0};    // Per-cell DCIR in mΩ (already ×3 for 3P correction)
float dcir_avg_mOhm         = 0.0f;   // Average across all valid cells
float dcir_delta_I_mA       = 0.0f;   // The |ΔI| that triggered measurement
float dcir_soc_at_test      = 0.0f;   // EKF SOC when measurement was taken
bool  dcir_valid             = false;  // True if >= 10 cells gave sane readings
uint8_t dcir_measurement_count = 0;   // Total measurements since boot

// History ring buffer (last 10 measurements, for dashboard display)
#define DCIR_HIST_MAX 10
struct DcirRecord {
  unsigned long timestamp_ms;
  float soc;
  float delta_I_mA;
  float avg_mOhm;
  float cells_mOhm[16];
};
DcirRecord dcirHistory[DCIR_HIST_MAX];
uint8_t dcirHistHead = 0;
uint8_t dcirHistCount = 0;
```

#### Change 2: Add `updateDCIR()` function

**Location:** Insert before `void setup()` (around line 782)

```cpp
// ==================== DCIR STATE MACHINE ====================
void updateDCIR() {
  // Configuration constants
  const int    DCIR_CURRENT_THRESHOLD = 2000;    // mA — minimum |ΔI| to trigger
  const unsigned long DCIR_SETTLE_MS  = 1000;    // 1 second settling time
  const unsigned long DCIR_COOLDOWN_MS = 30000;  // 30 second cooldown between measurements
  const float  DCIR_SOC_MIN = 25.0f;             // Don't measure below this SOC
  const float  DCIR_SOC_MAX = 65.0f;             // Don't measure above this SOC

  switch (dcirState) {

    case DCIR_IDLE: {
      // Continuously update the "before" baseline every 500ms
      // This acts like a dashcam — always recording, always overwriting
      if (millis() - dcirBaselineTime >= 500) {
        dcirBaselineTime = millis();
        for (int i = 0; i < MB_CONNECTED_CELLS; i++)
          dcir_before_V[i] = cellVoltages[i];
        dcir_before_I = cc1_raw;  // CC1 = low-noise, high-precision current
      }

      // SOC gate: only measure in the 25-65% sweet spot
      if (soc_ekf < DCIR_SOC_MIN || soc_ekf > DCIR_SOC_MAX) break;

      // Detect current step: charger plug/unplug, load connect/disconnect
      int delta = abs(cc1_raw - dcir_before_I);
      if (delta > DCIR_CURRENT_THRESHOLD) {
        // Freeze the baseline and transition to ARMED
        dcirState = DCIR_ARMED;
        dcirStateTime = millis();
        dcir_soc_at_test = soc_ekf;
        Serial.printf("[DCIR] ARMED! Current step: dI=%d mA, SOC=%.1f%%\n",
                      delta, soc_ekf);
      }
      break;
    }

    case DCIR_ARMED: {
      // Wait for voltage to settle under the new load
      if (millis() - dcirStateTime >= DCIR_SETTLE_MS) {
        dcirState = DCIR_CAPTURE;
        Serial.println("[DCIR] Settling complete. Capturing snapshot...");
      }
      break;
    }

    case DCIR_CAPTURE: {
      // Take the "after" snapshot
      for (int i = 0; i < MB_CONNECTED_CELLS; i++)
        dcir_after_V[i] = cellVoltages[i];
      dcir_after_I = cc1_raw;

      float deltaI = (float)abs(dcir_after_I - dcir_before_I);
      dcir_delta_I_mA = deltaI;

      // Abort if current step disappeared during settling
      if (deltaI < 500.0f) {
        Serial.println("[DCIR] ABORTED: Current step vanished during settling.");
        dcirState = DCIR_COOLDOWN;
        dcirStateTime = millis();
        break;
      }

      // Compute per-cell DCIR
      float sum = 0.0f;
      int   valid_count = 0;

      Serial.println("[DCIR] ========== MEASUREMENT COMPLETE ==========");
      Serial.printf("[DCIR] dI = %.0f mA  |  SOC = %.1f%%\n", deltaI, dcir_soc_at_test);
      Serial.printf("[DCIR] %-6s %-10s %-10s %-10s %-12s\n",
                    "Cell", "V_before", "V_after", "dV(mV)", "R_cell(mOhm)");
      Serial.println("[DCIR] --------------------------------------------------");

      for (int i = 0; i < MB_CONNECTED_CELLS; i++) {
        float dV = (float)dcir_before_V[i] - (float)dcir_after_V[i]; // mV
        float R_group = (fabsf(dV) / deltaI) * 1000.0f;  // mΩ (group of 3P)
        float R_cell  = R_group * 3.0f;                    // ×3 = single cell
        dcir_results_mOhm[i] = R_cell;

        Serial.printf("[DCIR] C%-5d %-10u %-10u %+-10.1f %-12.2f\n",
                      i + 1, dcir_before_V[i], dcir_after_V[i], dV, R_cell);

        // Sanity check: valid readings should be 1-200 mΩ
        if (R_cell > 1.0f && R_cell < 200.0f) {
          sum += R_cell;
          valid_count++;
        }
      }

      dcir_avg_mOhm = (valid_count > 0) ? (sum / valid_count) : 0.0f;
      dcir_valid = (valid_count >= 10); // At least 10 of 13 cells must be valid
      dcir_measurement_count++;

      Serial.println("[DCIR] --------------------------------------------------");
      Serial.printf("[DCIR] AVERAGE: %.2f mOhm/cell (%d valid cells)\n",
                    dcir_avg_mOhm, valid_count);
      Serial.println("[DCIR] ==========================================");

      // Save to circular history buffer
      DcirRecord &rec = dcirHistory[dcirHistHead];
      rec.timestamp_ms = millis();
      rec.soc = dcir_soc_at_test;
      rec.delta_I_mA = deltaI;
      rec.avg_mOhm = dcir_avg_mOhm;
      for (int i = 0; i < 16; i++) rec.cells_mOhm[i] = dcir_results_mOhm[i];
      dcirHistHead = (dcirHistHead + 1) % DCIR_HIST_MAX;
      if (dcirHistCount < DCIR_HIST_MAX) dcirHistCount++;

      dcirState = DCIR_COOLDOWN;
      dcirStateTime = millis();
      break;
    }

    case DCIR_COOLDOWN: {
      if (millis() - dcirStateTime >= DCIR_COOLDOWN_MS) {
        dcirState = DCIR_IDLE;
        Serial.println("[DCIR] Cooldown complete. Ready for next measurement.");
      }
      break;
    }
  }
}
```

#### Change 3: Call `updateDCIR()` in `loop()`

**Location:** Insert at line ~1027, before `delay(2);`

```cpp
  // DCIR State Machine (non-blocking, runs every loop iteration)
  updateDCIR();
```

---

### 2.4 Dashboard + JSON API Changes in `web_api.h`

#### Change 1: Add DCIR card to dashboard HTML

**Location:** Insert after the Balancing Diagnostics card closing `</div>` (around line 256), before the BQ76952 Status card

```html
  <div class="card">
    <div class="card-head" style="display:flex; justify-content:space-between; align-items:center;">
      <div><h2>DCIR Measurement</h2><div class="desc">Internal resistance per cell (load-step, SOC 30-60%)</div></div>
      <button onclick="downloadDcirLog()" style="padding:4px 10px;font-size:10px;font-weight:600;border:1px solid #e0e3e8;border-radius:4px;background:#fff;color:#5f6672;cursor:pointer;">&#x2B07; CSV</button>
    </div>
    <div class="card-body">
      <div style="display:flex;gap:12px;flex-wrap:wrap;margin-bottom:12px">
        <div class="cell-box" style="flex:1;min-width:90px"><div class="cell-label">STATUS</div><div class="cell-val" id="dcirState" style="font-size:14px;color:#9ca3ae">IDLE</div></div>
        <div class="cell-box" style="flex:1;min-width:90px"><div class="cell-label">AVG DCIR</div><div class="cell-val" id="dcirAvg" style="color:#7c3aed">--<span class="cell-unit">m&Omega;</span></div></div>
        <div class="cell-box" style="flex:1;min-width:90px"><div class="cell-label">&Delta;I</div><div class="cell-val" id="dcirDI">--<span class="cell-unit">mA</span></div></div>
        <div class="cell-box" style="flex:1;min-width:90px"><div class="cell-label">SOC@TEST</div><div class="cell-val" id="dcirSoc">--<span class="cell-unit">%</span></div></div>
        <div class="cell-box" style="flex:1;min-width:90px"><div class="cell-label">COUNT</div><div class="cell-val" id="dcirCnt">0</div></div>
      </div>
      <div class="cell-label" style="margin-bottom:4px">PER-CELL DCIR (m&Omega;/cell, green &lt;30, yellow 30-35, red &gt;35)</div>
      <div class="g4" style="gap:6px" id="dcirGrid"></div>
      <div id="dcirHistBox" style="margin-top:12px;display:none">
        <div class="cell-label" style="margin-bottom:4px">MEASUREMENT HISTORY (auto-recorded)</div>
        <table style="width:100%;border-collapse:collapse;font-size:10px;border:1px solid #e0e3e8">
          <thead><tr style="background:#f7f8fa;color:#9ca3ae"><th style="padding:2px 4px;border:1px solid #e0e3e8">#</th><th style="padding:2px 4px;border:1px solid #e0e3e8">Time</th><th style="padding:2px 4px;border:1px solid #e0e3e8">SOC</th><th style="padding:2px 4px;border:1px solid #e0e3e8">&Delta;I</th><th style="padding:2px 4px;border:1px solid #e0e3e8">Avg m&Omega;</th></tr></thead>
          <tbody id="dcirHistTb"></tbody>
        </table>
      </div>
    </div>
  </div>
```

#### Change 2: Add DCIR JavaScript

**Location:** Insert before the `setInterval` line (around line 1089)

```javascript
// ==================== DCIR UI ====================
const dG=document.getElementById('dcirGrid');
for(let i=0;i<CELLS;i++) dG.innerHTML+='<div class="cell-box" style="text-align:center;padding:4px 6px" id="dC'+i+'"><div class="cell-label">C'+(i+1)+'</div><div style="font-size:14px;font-weight:700" id="dV'+i+'">--</div></div>';
let dcirLog=[];
function updateDCIRUI(d){
  if(d.dcir_st===undefined)return;
  const sn=['IDLE','ARMED','SETTLING','COOLDOWN'];
  const sc=['#9ca3ae','#d97706','#2563eb','#16a34a'];
  const el=document.getElementById('dcirState');
  if(el){el.textContent=sn[d.dcir_st];el.style.color=sc[d.dcir_st];}
  document.getElementById('dcirCnt').textContent=d.dcir_n||'0';
  if(d.dcir_ok){
    document.getElementById('dcirAvg').innerHTML=d.dcir_avg.toFixed(1)+'<span class="cell-unit">m\u03A9</span>';
    document.getElementById('dcirDI').innerHTML=d.dcir_dI.toFixed(0)+'<span class="cell-unit">mA</span>';
    document.getElementById('dcirSoc').innerHTML=d.dcir_soc.toFixed(1)+'<span class="cell-unit">%</span>';
    const c=d.dcir_c||[];
    for(let i=0;i<CELLS;i++){
      const e=document.getElementById('dV'+i);
      if(e&&c[i]!==undefined){
        e.textContent=c[i].toFixed(1);
        e.style.color=c[i]>35?'#dc2626':c[i]>30?'#d97706':'#16a34a';
      }
    }
    // Auto-log new measurements
    if(d.dcir_n>dcirLog.length){
      dcirLog.push({t:new Date().toLocaleTimeString(),soc:d.dcir_soc,dI:d.dcir_dI,avg:d.dcir_avg,c:c.slice(0,CELLS)});
      const hB=document.getElementById('dcirHistBox'),hT=document.getElementById('dcirHistTb');
      if(hB&&hT){
        hB.style.display='block';
        let h='';
        dcirLog.forEach((r,i)=>{
          h+='<tr><td style="padding:2px 4px;border:1px solid #e0e3e8">'+(i+1)+'</td>';
          h+='<td style="padding:2px 4px;border:1px solid #e0e3e8">'+r.t+'</td>';
          h+='<td style="padding:2px 4px;border:1px solid #e0e3e8">'+r.soc.toFixed(1)+'</td>';
          h+='<td style="padding:2px 4px;border:1px solid #e0e3e8">'+r.dI.toFixed(0)+'</td>';
          h+='<td style="padding:2px 4px;border:1px solid #e0e3e8;font-weight:700;color:#7c3aed">'+r.avg.toFixed(2)+'</td></tr>';
        });
        hT.innerHTML=h;
      }
    }
  }
}
function downloadDcirLog(){
  if(!dcirLog.length){alert('No DCIR data yet.');return;}
  let csv='#,Time,SOC_%,Delta_I_mA,Avg_mOhm';
  for(let i=1;i<=CELLS;i++) csv+=',Cell_'+i+'_mOhm';
  csv+='\n';
  dcirLog.forEach((r,i)=>{
    csv+=(i+1)+','+r.t+','+r.soc.toFixed(1)+','+r.dI.toFixed(0)+','+r.avg.toFixed(2);
    r.c.forEach(v=>csv+=','+v.toFixed(2));
    csv+='\n';
  });
  const a=document.createElement('a');
  a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));
  a.download='dcir_'+new Date().toISOString().slice(0,19).replace(/:/g,'-')+'.csv';
  a.click();
}
```

#### Change 3: Call `updateDCIRUI(d)` inside `updateUI(d)` 

**Location:** Insert before the `} catch(e)` at line ~835

```javascript
  updateDCIRUI(d);
```

#### Change 4: Add DCIR + Open Wire fields to JSON API in `handleApiData()`

**Location:** Insert before `json += "}";` at line 1253

```cpp
  // DCIR Measurement
  json += ",\"dcir_st\":" + String((int)dcirState);
  json += ",\"dcir_ok\":" + String(dcir_valid ? 1 : 0);
  json += ",\"dcir_avg\":" + String(dcir_avg_mOhm, 2);
  json += ",\"dcir_dI\":" + String(dcir_delta_I_mA, 0);
  json += ",\"dcir_soc\":" + String(dcir_soc_at_test, 1);
  json += ",\"dcir_n\":" + String(dcir_measurement_count);
  json += ",\"dcir_c\":[";
  for (int i = 0; i < 16; i++) {
    json += String(dcir_results_mOhm[i], 2);
    if (i < 15) json += ",";
  }
  json += "]";
```

#### Change 5: Add Open Wire flag to dashboard fault display

**Location:** In `updateUI()`, after the temperature fault flags (line ~591)

Add after the `addF('OTC', d.temp_otc);` line:
```javascript
    addF('WIRE', d.ssC & 0x80);
```

---

<a name="feature-3"></a>
## Feature 3 — EKF & Current Calibration (Post-Upload Analysis)

This will be done as a separate phase AFTER we upload and test Features 1 & 2. We will:

1. Analyze the CSV at `C:\Users\mahmoud hamdy\Downloads\Data_Base_readings.csv`
2. Identify the EKF hallucination during the 10A discharge event
3. Measure current sensor noise floor at idle
4. Propose EKF tuning (Q/R matrices, sample time) and CC3 filter changes

**This is NOT part of the code changes above.** It requires data analysis first, then a separate firmware update.

---

<a name="verification"></a>
## Verification Checklist

After uploading, check the following:

### Serial Monitor
- [ ] `[BMS-INIT] Temp protections ENABLED: OTC=50C, OTD=60C, OTF=80C`
- [ ] `[BMS-INIT] Open Wire detection ENABLED`
- [ ] `[BMS-INIT] Prot B (0x9262): 0xB0`
- [ ] `[BMS-INIT] Prot C (0x9263): 0x80`
- [ ] `[BMS-INIT] OTC=50°C OTD=60°C OTF=80°C`
- [ ] No false Open Wire trips (HWDF should stay 0)

### Dashboard
- [ ] "ALL CLEAR" still shows (no false faults)
- [ ] DCIR card visible with "IDLE" status
- [ ] Temp readings are normal (below 50°C)

### DCIR Test (requires charger > 2A, SOC 30-60%)
- [ ] Plug charger → DCIR shows "ARMED" then "SETTLING" then "COOLDOWN"
- [ ] Per-cell values appear in 20-40 mΩ range
- [ ] All 13 cells within ±20% of each other
- [ ] Unplug charger → second measurement triggers
- [ ] "Download CSV" button exports valid file
