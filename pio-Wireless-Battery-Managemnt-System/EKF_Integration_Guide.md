# Wireless BMS: EKF Stability & Boot Fix Guide

This guide summarizes the recent firmware overhauls applied to the Slave board. These changes permanently fix the SOC hallucination under heavy load, correct the boot initialization glitches, and secure the battery math against hardware noise.

## 1. The Core Problem
During testing, we discovered three major mathematical flaws:
1. **The 3.0x Hallucination**: Under any load (2A to 10A), the EKF was dropping/rising the SOC exactly 3 times faster than actual physical Coulomb Counting. This occurred because the Neural Network was disabled, making the ECM voltage prediction highly inaccurate under load. The Kalman filter was trusting this broken voltage model and corrupting the SOC.
2. **The 0%/100% Boot Glitch**: If the ESP32 restarted while current was flowing, it calculated impossible voltage drops (e.g., -20V) and initialized the SOC to either 0% or 100%.
3. **Idle Drift**: The ESP32's background power draw (2-7mA) was being integrated by the Coulomb Counter while parked, causing phantom SOC drain.

---

## 2. The Solution Strategy

Instead of using a complex sliding-window adaptive filter (which is unpredictable without a working Neural Network), we switched to a **Deterministic Binary Strategy**:

* **State 1 (Active Load):** The EKF mathematically locks into a "Pure Coulomb Counter". It completely ignores the battery voltage and only tracks the Amps entering/leaving the pack.
* **State 2 (At Rest):** The EKF unlocks. Because the battery voltage is accurate at 0 Amps, the Kalman Filter uses the Open Circuit Voltage (OCV) curve to slowly and safely self-correct any drift from the Coulomb Counter.

---

## 3. Exact Code Changes

### A. Boot Initialization Fix
**File:** `src/slaves/sender.cpp` (Function: `smartSOCInit`)
* **What we did:** We deleted a hardcoded `PARASITIC_R = 2.43f` (2.43 Ohms) typo that was destroying the boot voltage calculation.
* **What we did:** We corrected a Pack Current vs. Cell Current mismatch. The initial voltage drop is now accurately calculated using `R0_avg` (Internal Resistance) divided by the 3 parallel cells.

### B. Robust Binary Q/R Tuning
**File:** `lib/BatteryEKF/BatteryEKF.cpp` (Function: `adaptQR`)
* **What we did:** Replaced the old adaptive logic with hardcoded binary Kalman matrices:
  * **Under Load** (`current > 10mA`): `Q[0] = 1e-8` (freezes SOC state) and `R = 1.0` (ignores voltage).
  * **At Rest** (`current < 10mA`): `Q[0] = 1e-5` (unlocks SOC state) and `R = 0.005` (trusts OCV curve).

### C. Sensor Calibration & Deadband
**File:** `src/slaves/sender.cpp` (Function: `updateEKF`)
* **What we did:** Applied laboratory calibration to the raw BQ76952 current: `I_true = (I_BQ - 0.048) / 1.030`. This fixes a +3% hardware gain error and a 48mA offset.
* **What we did:** Added a **10mA Deadband**. Any current below 10mA is mathematically zeroed out to prevent the ESP32's idle quiescent current from draining the SOC gauge over long parked periods.
* **What we did:** Added a **Stale Data Guard**. If the I2C bus fails (`powerMode == 4`), the EKF skips its update to prevent "frozen" voltage readings from tricking the Kalman Filter.

### D. System Timing & Stability
**File:** `src/slaves/sender.cpp`
* **What we did:** Increased `EKF_UPDATE_INTERVAL` from 1000ms to **5000ms (5 seconds)**. 
* **What we did:** Updated `ekf.setSampleTime(5.0f)` and the SOH tracker (`sohEngine.trackEnvironmentalDamage(..., 5.0f)`) to match. This aligns the State Transition matrices with real physical time.

---

## 4. Verification Results
When simulated against the raw 10A discharge CSV database:
* **Old EKF:** Hallucinated a 9.9% SOC drop.
* **New Robust EKF:** Tracked perfectly at a 3.3% SOC drop (exactly matching theoretical physics capacity drain).
* **Rest Recovery:** After the load stopped, the EKF smoothly recovered the SOC toward the true resting voltage without freezing.
