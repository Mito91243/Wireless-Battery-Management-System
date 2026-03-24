#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "BQ76952.h"
#include "tb_config.h"
#include <BatteryEKF.h>
#include <Wire.h>
#include <math.h>

// ==================== OBJECTS ====================
BQ76952 bms;
WebServer server(80);

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
bool fetEn = false;
bool ledState = false;
bq_protection_t protStatus;
bq_temperature_t tempStatus;
uint16_t balancingMask = 0;
uint16_t manufStatus = 0; // Sealing and ConfigUpdate status
uint8_t powerMode = 0;      // 0=Active, 1=Sleep, 2=DeepSleep, 3=Shutdown
uint8_t pendingPwrMode = 0; // Tracks if we just sent a Sleep/DeepSleep command
unsigned long pwrCommandTime = 0;

unsigned long lastRead = 0;
const uint32_t READ_INTERVAL_MS = 500;
uint32_t txCount = 0;

// ==================== EKF DATA ====================
BatteryEKF ekf(1.0f);  // 1 second sample time (was 10s)
float soc_ekf = 0.0f;
float soc_uncertainty = 0.0f;
float voltage_error_ekf = 0.0f;
float initial_ekf_soc = -1.0f; // Stores the boot SOC from the EKF
float software_charge_Ah = 0.0f; // Accurate CC integration
unsigned long last_cc_update = 0;
unsigned long lastEKFUpdate = 0;
const unsigned long EKF_UPDATE_INTERVAL = 1000; // 1 second update (was 10s)

// Helper function for smart SOC initialization
float smartSOCInit(float V_measured, float I_current) {
    const float R0_avg = 0.018f; 
    const float PARASITIC_R = 2.43f;
    
    // Strip both internal resistance AND the massive testboard wiring drop!
    float total_R = R0_avg;
    if (fabsf(I_current) > 0.05f) total_R += PARASITIC_R;
    
    float V_corrected = V_measured - (total_R * I_current);
    
    // Invert OCV table for accurate initialization
    float soc_init = ekf.invertOCV_Discharge(V_corrected);
    
    Serial.printf("[EKF] Smart Init (OCV lookup): V=%.3fV (raw=%.3fV), I=%.2fA -> SOC~%.1f%%\n", V_corrected, V_measured, I_current, soc_init);
    return constrain(soc_init, 0.0f, 100.0f);
}

void updateEKF() {
    float current_A = (float)bmsCurrent / 1000.0f; // Re-enabled real current reading!
    float voltage_V = (float)cellVoltages[0] / 1000.0f; // Use cell 1 for EKF
    ekf.update(current_A, voltage_V);
    soc_ekf = ekf.getSOC();
    soc_uncertainty = ekf.getSOCUncertainty();
    voltage_error_ekf = ekf.getVoltageError() * 1000.0f;
    Serial.printf("[EKF] I=%.3fA, V=%.3fV -> SOC=%.1f%% (unc=%.2f%%), Verr=%.1fmV\n",
                  current_A, voltage_V, soc_ekf, soc_uncertainty, voltage_error_ekf);
}

// Manual override cooldown — prevents sync code from fighting user commands
unsigned long chgManualOverrideUntil = 0;
unsigned long dsgManualOverrideUntil = 0;
#define MANUAL_OVERRIDE_MS 3000  // 3 second grace period

// ==================== CELL BALANCING LOGIC ====================
void runBalancing() {
    float current_A = (float)bmsCurrent / 1000.0f;
    float corr_mv = (fabs(current_A) > 0.05f) ? (current_A * 2.43f * 1000.0f) : 0.0f;

    uint16_t mask = 0;
    float minV = 5000.0f, maxV = 0.0f;
    float correctedVolts[16];
    
    for (int i = 0; i < TB_CONNECTED_CELLS; i++) {
        // Fix: Subtract IR drop to get battery OCV (Corrected per User Review)
        correctedVolts[i] = (float)cellVoltages[i] - corr_mv;
        if (correctedVolts[i] < minV) minV = correctedVolts[i];
        if (correctedVolts[i] > maxV) maxV = correctedVolts[i];
    }

    // DEBUG: Print balancing decision logic every 2 seconds
    // Polling loop removed to prevent blocking (as it worked flawlessly before without it)
    static unsigned long lastBalLog = 0;
    if (millis() - lastBalLog > 2000) {
        lastBalLog = millis();
        Serial.printf("[BAL] I=%.3fA, Vmax=%.0fmV, Vmin=%.0fmV, Delta=%.0fmV\n", 
                      current_A, maxV, minV, (maxV - minV));
    }

    // Production balancing thresholds (from User Review)
    // Lowered for testing visibility so you can see balancing trigger! (from 20 -> 5)
    const float BAL_START_THRESHOLD_MV = 5.0f; 
    const float BAL_TARGET_DELTA_MV = 10.0f;     // Target final delta

    // Logic: Start balancing if any cell > 3.6V and delta > threshold
    if (maxV > 3600.0f && (maxV - minV) > BAL_START_THRESHOLD_MV) {
        for (int i = 0; i < TB_CONNECTED_CELLS; i++) {
            // Balance any cell that is more than the target delta above the minimum
            if (correctedVolts[i] > (minV + BAL_TARGET_DELTA_MV)) {
                if (i == TB_CONNECTED_CELLS - 1) mask |= (1 << 15); // VC16
                else mask |= (1 << i); 
            }
        }
    }

    // Force write every 5 seconds even if mask hasn't changed (Heartbeat)
    static unsigned long lastForceWrite = 0;
    bool force = (millis() - lastForceWrite > 5000);

    static uint16_t lastMask = 0xFFFF;
    if (mask != lastMask || force) {
        if (mask > 0 || lastMask > 0) { // Only log if something is actually happening
            Serial.printf("[BAL] Update Mask: 0x%04X %s\n", mask, force ? "(Heartbeat)" : "");
        }
        bms.setBalancingMask(mask);
        lastMask = mask;
        lastForceWrite = millis();
    }
}

// ==================== I2C LOG ====================
#define LOG_SIZE 50
struct I2CLogEntry
{
  uint32_t timestamp;
  uint8_t reg;
  uint16_t value;
  bool isWrite;
  bool ok;
};
I2CLogEntry i2cLog[LOG_SIZE];
int logHead = 0;
int logCount = 0;

void addLog(uint8_t reg, uint16_t value, bool isWrite, bool ok)
{
  i2cLog[logHead].timestamp = millis();
  i2cLog[logHead].reg = reg;
  i2cLog[logHead].value = value;
  i2cLog[logHead].isWrite = isWrite;
  i2cLog[logHead].ok = ok;
  logHead = (logHead + 1) % LOG_SIZE;
  if (logCount < LOG_SIZE) logCount++;
  txCount++;
}

// ==================== READ BMS DATA VIA I2C ====================
void readBMSData()
{
  unsigned int batStat = bms.directCommandRead(DIR_CMD_BAT_STATUS);
  bool isSleep = (batStat & 0x8000) != 0;
  if (batStat == 0) powerMode = 2; else if (isSleep) powerMode = 1; else powerMode = 0;
  if (batStat != 0) pendingPwrMode = 0; 
  if (powerMode == 2) { return; }

  // Latency Fix: ONLY read connected cells (3) instead of 16!
  for (int i = 0; i < TB_CONNECTED_CELLS; i++)
  {
      if (i == TB_CONNECTED_CELLS - 1) cellVoltages[i] = bms.getCellVoltage(16);
      else cellVoltages[i] = bms.getCellVoltage(i + 1);
  }
  for (int i = TB_CONNECTED_CELLS; i < 16; i++) cellVoltages[i] = 0;

  vStack = bms.getCellVoltage(17);
  vPack = bms.getCellVoltage(18);
  bmsCurrent = bms.getCurrent();
  chipTemp = bms.getInternalTemp();
  temp1 = bms.getThermistorTemp(TS1);
  temp2 = bms.getThermistorTemp(TS2);
  temp3 = bms.getThermistorTemp(TS3);

  charge = bms.getAccumulatedCharge();
  unsigned long now_cc = millis();
  if (last_cc_update != 0) {
      float dt_h = (now_cc - last_cc_update) / 3600000.0f;
      // We integrate actual current to completely bypass BQ CC scaling bugs
      software_charge_Ah += ((float)bmsCurrent / 1000.0f) * dt_h;
  }
  last_cc_update = now_cc;
  chargeTime = bms.getAccumulatedChargeTime();
  
  // RESTORED DEBUG LOG: Print real BQ76952 hardware state every poll cycle
  uint16_t fetStat = bms.directCommandRead(0x7F);
  Serial.printf("[DEBUG] BatStat=0x%04X FET_Stat=0x%02X | CHG=%d DSG=%d | CFETOFF_pin=%d DFETOFF_pin=%d\n",
                batStat, fetStat,
                (fetStat & 0x01) ? 1 : 0, (fetStat & 0x04) ? 1 : 0,
                digitalRead(TB_PIN_CFETOFF), digitalRead(TB_PIN_DFETOFF));

  fetEn = (batStat & 0x0800) != 0;
  protStatus = bms.getProtectionStatus();
  tempStatus = bms.getTemperatureStatus();
}


// ==================== HTML PAGE ====================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>BQ76952 Test Board</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'IBM Plex Mono','SF Mono','Consolas',monospace;font-size:12px;background:#f3f4f6;color:#1a1d23;height:100vh;display:flex;flex-direction:column;overflow:hidden}
::-webkit-scrollbar{width:5px}::-webkit-scrollbar-track{background:transparent}::-webkit-scrollbar-thumb{background:#d1d5db;border-radius:3px}
input:focus{outline:1px solid #2563eb;border-color:#2563eb!important}
button:hover{filter:brightness(0.95)}
.header{display:flex;align-items:center;gap:12px;padding:6px 16px;background:#fff;border-bottom:1px solid #e0e3e8}
.header h1{font-size:13px;font-weight:700}
.header .sub{font-size:10px;color:#9ca3ae}
.conn{font-size:11px;color:#16a34a;font-weight:500}
.tabs{display:flex;border-bottom:1px solid #e0e3e8;background:#fff;padding:0 12px}
.tab{padding:8px 16px;cursor:pointer;font-size:11px;font-weight:400;color:#9ca3ae;border-bottom:2px solid transparent}
.tab.active{font-weight:600;color:#2563eb;border-bottom-color:#2563eb}
.content{flex:1;overflow:auto;padding:12px}
.card{border:1px solid #e0e3e8;border-radius:5px;background:#fff;margin-bottom:12px}
.card-head{padding:8px 12px;border-bottom:1px solid #ebedf0;background:#f7f8fa;border-radius:5px 5px 0 0}
.card-head h2{font-size:10px;font-weight:700;color:#5f6672;text-transform:uppercase;letter-spacing:0.8px}
.card-head .desc{font-size:10px;color:#9ca3ae;margin-top:1px}
.card-body{padding:12px}
.g2{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.g3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px}
.g4{display:grid;grid-template-columns:1fr 1fr 1fr 1fr;gap:10px}
@media(max-width:768px){.g2,.g3{grid-template-columns:1fr}}
.cell-box{padding:8px 10px;background:#f7f8fa;border-radius:4px;border:1px solid #ebedf0}
.cell-box.warn{border-color:#fecaca}
.cell-label{font-size:9px;color:#9ca3ae;font-weight:500}
.cell-val{font-size:22px;font-weight:700;font-variant-numeric:tabular-nums;line-height:1.2}
.cell-unit{font-size:10px;color:#9ca3ae;margin-left:2px}
.kv-label{font-size:9px;color:#9ca3ae;font-weight:500;text-transform:uppercase;letter-spacing:0.4px}
.kv-val{font-size:17px;font-weight:700;font-variant-numeric:tabular-nums;line-height:1.3}
.kv-unit{font-size:9px;font-weight:400;color:#9ca3ae;margin-left:2px}
.badge{font-size:9px;padding:2px 7px;border-radius:3px;font-weight:600;letter-spacing:0.2px;display:inline-block}
.badge-green{background:#dcfce7;color:#16a34a}
.badge-red{background:#fee2e2;color:#dc2626}
.badge-amber{background:#fef3c7;color:#d97706}
.badge-gray{background:#f3f4f6;color:#9ca3ae}
.badge-outline{background:transparent;border:1px solid}
.btn{padding:5px 12px;border-radius:4px;border:1px solid #e0e3e8;background:#fff;color:#5f6672;font-size:11px;font-weight:500;cursor:pointer;font-family:inherit;transition:all 0.1s}
.btn:hover{background:#f7f8fa}
.btn.primary{background:#eff6ff;border-color:#bfdbfe;color:#2563eb}
.btn.danger{border-color:#fecaca;color:#dc2626}
.btn.danger:hover{background:#fef2f2}
.btn.active{background:#eff6ff;border-color:#2563eb;color:#2563eb;font-weight:600}
.btn.sm{padding:3px 8px;font-size:10px}
.toggle{width:34px;height:18px;border-radius:9px;cursor:pointer;position:relative;border:none;padding:0;transition:background 0.15s}
.toggle.on{background:#16a34a}.toggle.off{background:#d1d5db}
.toggle-dot{width:14px;height:14px;border-radius:50%;background:#fff;position:absolute;top:2px;transition:left 0.15s;box-shadow:0 1px 2px rgba(0,0,0,0.15)}
.toggle.on .toggle-dot{left:18px}.toggle.off .toggle-dot{left:2px}
.fet-row{display:flex;align-items:center;justify-content:space-between;padding:8px 0}
.fet-row+.fet-row{border-top:1px solid #ebedf0}
.pbar{height:4px;background:#e5e7eb;border-radius:2px;margin-top:4px}
.pbar-fill{height:4px;border-radius:2px;transition:width 0.3s}
.temp-box{padding:8px 10px;background:#f7f8fa;border-radius:4px;border:1px solid #ebedf0;text-align:center}
.temp-val{font-size:20px;font-weight:700;font-variant-numeric:tabular-nums}
.log-panel{background:#fff;border-top:1px solid #e0e3e8;display:flex;flex-direction:column;flex-shrink:0;transition:height 0.15s}
.log-header{display:flex;align-items:center;justify-content:space-between;padding:5px 16px;cursor:pointer;min-height:30px;flex-shrink:0}
.log-body{flex:1;overflow:auto;font-size:10px;padding:0 16px 6px}
.log-body table{width:100%;border-collapse:collapse}
.log-body th{text-align:left;padding:2px 8px;color:#9ca3ae;font-weight:500;font-size:9px;border-bottom:1px solid #e0e3e8;position:sticky;top:0;background:#fff}
.log-body td{padding:1px 8px}
.log-read{color:#2563eb;font-weight:600}.log-write{color:#16a34a;font-weight:600}
.log-ack{color:#16a34a;font-weight:600}.log-nack{color:#dc2626;font-weight:600}
.statusbar{display:flex;gap:16px;padding:3px 16px;background:#fff;border-top:1px solid #e0e3e8;font-size:10px;color:#9ca3ae}
.divider{border-top:2px solid #e0e3e8;padding-top:12px;margin-top:4px}
.section-title{font-size:11px;font-weight:700;color:#5f6672;margin-bottom:10px;text-transform:uppercase;letter-spacing:0.5px}
.led-card{display:flex;align-items:center;gap:16px;padding:12px;background:#f7f8fa;border-radius:6px;border:2px dashed #e0e3e8}
.led-indicator{width:24px;height:24px;border-radius:50%;border:2px solid #d1d5db;transition:all 0.3s}
.led-indicator.on{background:#16a34a;border-color:#16a34a;box-shadow:0 0 12px rgba(22,163,74,0.5)}
.led-indicator.off{background:#f3f4f6}
.warn-box{margin-top:6px;padding:5px 8px;border-radius:4px;background:#fffbeb;border:1px solid #fde68a;color:#d97706;font-size:10px;font-weight:500}
.chart-wrap{border:1px solid #e0e3e8;border-radius:5px;background:#fff;padding:8px 10px;margin-bottom:12px}
</style>
</head>
<body>
<div class="header">
  <h1>BQ76952</h1>
  <span class="sub">3-Cell Test Board</span>
  <div style="flex:1"></div>
  <span class="conn" id="connStatus">&#x25CF; I2C &middot; 0x08</span>
</div>
<div class="tabs">
  <div class="tab active" onclick="switchTab(0)" id="tab0">Dashboard</div>
  <div class="tab" onclick="switchTab(1)" id="tab1">Plots</div>
</div>
<div class="content" id="mainContent">
<div id="dashTab">
<div class="g2">
  <div class="card">
    <div class="card-head"><h2>Cell Voltages</h2><div class="desc">Individual cell monitoring with pack summary</div></div>
    <div class="card-body">
      <div id="cellGrid" class="g3" style="margin-bottom:12px"></div>
      <div class="g4">
        <div><div class="kv-label">Pack</div><div class="kv-val" style="color:#2563eb" id="kvPack">--<span class="kv-unit">V</span></div></div>
        <div><div class="kv-label">Min</div><div class="kv-val" id="kvMin">--<span class="kv-unit">V</span></div></div>
        <div><div class="kv-label">Max</div><div class="kv-val" id="kvMax">--<span class="kv-unit">V</span></div></div>
        <div><div class="kv-label">Delta</div><div class="kv-val" id="kvDelta">--<span class="kv-unit">mV</span></div></div>
      </div>
    </div>
  </div>
  <div class="card">
    <div class="card-head"><h2>Pack Status</h2><div class="desc">Current, power, SOC and charge accumulation</div></div>
    <div class="card-body">
      <div class="g3" style="margin-bottom:12px">
        <div class="cell-box"><div class="cell-label">CURRENT</div><div class="cell-val" id="pCurrent">--<span class="cell-unit">mA</span></div><div style="font-size:9px;font-weight:500;margin-top:1px" id="pCurState">IDLE</div></div>
        <div class="cell-box"><div class="cell-label">BQ SOC (est)</div><div class="cell-val" id="pSoc">--<span class="cell-unit">%</span></div><div class="pbar"><div class="pbar-fill" id="pSocBar" style="width:0%;background:#16a34a"></div></div></div>
        <div class="cell-box"><div class="cell-label">EKF SOC</div><div class="cell-val" id="kvSOC_EKF" style="color:#2563eb">--<span class="cell-unit">%</span></div><div style="font-size:9px;color:#9ca3ae">±<span id="kvSOC_Unc">--</span>% unc</div></div>
        <div class="cell-box"><div class="cell-label">POWER</div><div class="cell-val" id="pPower">--<span class="cell-unit">W</span></div></div>
      </div>
      <div class="g4">
        <div><div class="kv-label">Accum. Charge</div><div class="kv-val" id="kvCharge">--<span class="kv-unit">mAh</span></div></div>
        <div><div class="kv-label" id="kvTimeL">Active Time</div><div class="kv-val" id="kvChargeTime">--<span class="kv-unit">s</span></div></div>
        <div><div class="kv-label">Stack V</div><div class="kv-val" id="kvStack">--<span class="kv-unit">mV</span></div></div>
        <div><div class="kv-label">Pack V</div><div class="kv-val" id="kvPackMv">--<span class="kv-unit">mV</span></div></div>
      </div>
    </div>
  </div>
</div>
<div class="g2">
  <div class="card">
    <div class="card-head"><h2>Temperatures</h2><div class="desc">External thermistors and BQ76952 die temperature</div></div>
    <div class="card-body">
      <div class="g4">
        <div class="temp-box"><div class="cell-label">TS1</div><div class="temp-val" id="tTs1">--</div></div>
        <div class="temp-box"><div class="cell-label">TS2</div><div class="temp-val" id="tTs2">--</div></div>
        <div class="temp-box"><div class="cell-label">TS3</div><div class="temp-val" id="tTs3">--</div></div>
        <div class="temp-box"><div class="cell-label">Die</div><div class="temp-val" id="tChip">--</div></div>
      </div>
    </div>
  </div>
  <div class="card">
    <div class="card-head"><h2>Safety &amp; Protection</h2><div class="desc">FET status and active fault flags</div></div>
    <div class="card-body">
      <div style="margin-bottom:10px"><div class="cell-label" style="margin-bottom:4px">FET STATUS</div>
        <div style="display:flex;gap:6px"><span class="badge" id="badgeChg">CHG: --</span><span class="badge" id="badgeDsg">DSG: --</span><span class="badge badge-gray badge-outline">ALERT: CLEAR</span></div>
      </div>
      <div><div class="cell-label" style="margin-bottom:4px">PROTECTION FLAGS</div>
        <div style="display:flex;gap:4px;flex-wrap:wrap" id="protFlags"><span class="badge badge-green">OV OK</span><span class="badge badge-green">UV OK</span><span class="badge badge-green">OC OK</span><span class="badge badge-green">SC OK</span><span class="badge badge-green">OT OK</span><span class="badge badge-green">UT OK</span></div>
      </div>
      <div style="margin-top:8px"><div class="cell-label" style="margin-bottom:4px">CELL BALANCING</div>
        <div style="display:flex;gap:4px;flex-wrap:wrap" id="balFlags"></div>
      </div>
    </div>
  </div>
  <div class="card">
    <div class="card-head"><h2>BQ76952 Status</h2><div class="desc">Device sealing and power management</div></div>
    <div class="card-body">
      <div class="g2" style="margin-bottom:12px">
        <div class="cell-box"><div class="cell-label">SEALING</div><div class="cell-val" id="statSeal" style="font-size:14px">--</div></div>
        <div class="cell-box"><div class="cell-label">POWER MODE</div><div class="cell-val" id="statPwr" style="font-size:14px">--</div></div>
      </div>
      <div style="display:flex;gap:6px;flex-wrap:wrap">
        <button class="btn sm" onclick="sendCmd('pwrSleep')">Sleep</button>
        <button class="btn sm" onclick="sendCmd('pwrDeep')">Deep Sleep</button>
        <button class="btn sm" onclick="sendCmd('fetMasterToggle')">FET Master Enable</button>
        <button class="btn sm primary" onclick="sendCmd('pwrWake')">Wake (I2C)</button>
      </div>
      <div class="warn-box" style="margin-top:8px; border-style:dashed; color:#5f6672; background:#f9fafb" id="pwrNote">
        <b>NOTE:</b> If the board is in <b>Deep Sleep</b> or <b>Shutdown</b>, you MUST press the physical <b>WAKE</b> button on the hardware to reactivate communication.
      </div>
    </div>
  </div>
</div>
<div class="divider">
  <div class="section-title">Device Controls</div>
  <div class="g3">
    <div class="card">
      <div class="card-head"><h2>FET Control</h2><div class="desc">Charge/discharge MOSFET switching via I2C subcommands</div></div>
      <div class="card-body">
        <div class="fet-row"><div><div style="font-size:11px;font-weight:500">Charge FET</div><div style="font-size:9px;color:#9ca3ae">0x0094 + 0x0001=ON / 0x0002=OFF</div></div><div style="display:flex;align-items:center;gap:8px"><span class="badge" id="fetChgBadge">--</span><button class="toggle off" id="fetChgToggle" onclick="toggleFet('chg')"><div class="toggle-dot"></div></button></div></div>
        <div class="fet-row"><div><div style="font-size:11px;font-weight:500">Discharge FET</div><div style="font-size:9px;color:#9ca3ae">0x0093 + 0x0001=ON / 0x0002=OFF</div></div><div style="display:flex;align-items:center;gap:8px"><span class="badge" id="fetDsgBadge">--</span><button class="toggle off" id="fetDsgToggle" onclick="toggleFet('dsg')"><div class="toggle-dot"></div></button></div></div>
        <div id="fetWarn" style="display:none" class="warn-box">Both FETs off — pack is isolated</div>
      </div>
    </div>
    <div class="card">
      <div class="card-head"><h2>Reset &amp; Utilities</h2><div class="desc">Hardware reset and charge counter control</div></div>
      <div class="card-body">
        <div style="display:flex;flex-direction:column;gap:8px">
          <div style="display:flex;align-items:center;justify-content:space-between"><div><div style="font-size:11px">Reset BMS IC</div><div style="font-size:9px;color:#9ca3ae">Subcmd 0x0012</div></div><button class="btn danger" onclick="sendCmd('reset')">Reset</button></div>
          <div style="border-top:1px solid #ebedf0;padding-top:8px;display:flex;align-items:center;justify-content:space-between"><div><div style="font-size:11px">Reset EKF & CC</div><div style="font-size:9px;color:#9ca3ae">O0x0082 + Voltage Init</div></div><button class="btn danger" onclick="fetch('/api/ekf_reset').then(()=>setTimeout(fetchData,200))">Sync</button></div>
          <!-- Removed FET_EN Status block as device defaults to FULLACCESS autonomous mode -->
          <div style="border-top:1px solid #ebedf0;padding-top:8px;display:flex;align-items:center;justify-content:space-between"><div><div style="font-size:11px">All FETs Auto-ON</div><div style="font-size:9px;color:#9ca3ae">Subcmd 0x009E</div></div><button class="btn primary" onclick="sendCmd('allFetsOn')">Enable</button></div>
          <div style="border-top:1px solid #ebedf0;padding-top:8px;display:flex;align-items:center;justify-content:space-between"><div><div style="font-size:11px;color:#dc2626;font-weight:600">All FETs OFF</div><div style="font-size:9px;color:#9ca3ae">Subcmd 0x009D</div></div><button class="btn danger" onclick="sendCmd('allFetsOff')">Kill</button></div>
        </div>
      </div>
    </div>
<div id="plotsTab" style="display:none">
  <div style="display:flex;gap:6px;align-items:center;margin-bottom:12px"><span style="font-size:10px;color:#9ca3ae;font-weight:500">Window:</span><button class="btn sm active">60s</button><button class="btn sm">5min</button></div>
  <div class="chart-wrap"><svg id="chartVoltage" viewBox="0 0 700 200" style="width:100%;display:block"></svg></div>
  <div class="chart-wrap"><svg id="chartCurrent" viewBox="0 0 700 160" style="width:100%;display:block"></svg></div>
  <div class="chart-wrap"><svg id="chartTemp" viewBox="0 0 700 160" style="width:100%;display:block"></svg></div>
</div>
</div>
<div class="log-panel" id="logPanel" style="height:160px">
  <div class="log-header" onclick="toggleLog()">
    <span style="font-size:11px;font-weight:600;color:#5f6672"><span id="logArrow">&#x25BE;</span> I2C Log <span style="font-weight:400;color:#9ca3ae" id="logTxCount">(0)</span></span>
    <div style="display:flex;gap:4px"><button onclick="event.stopPropagation();clearLog()" style="background:none;border:1px solid #e0e3e8;border-radius:3px;padding:1px 6px;font-size:9px;color:#9ca3ae;cursor:pointer;font-family:inherit">Clear</button></div>
  </div>
  <div class="log-body" id="logBody"><table><thead><tr><th>Time</th><th>Dir</th><th>Addr</th><th>Reg</th><th>Data</th><th></th></tr></thead><tbody id="logTbody"></tbody></table></div>
</div>
<div class="statusbar">
  <span style="color:#16a34a" id="sbConn">&#x25CF; Connected</span>
  <span>I2C 0x08</span>
  <span id="sbElapsed">0:00</span>
  <span id="sbTx">Tx: 0</span>
  <div style="flex:1"></div>
  <span>wBMS Test Board v0.1</span>
</div>
<script>
const CELLS=3;
const regNames={0x03:'SafetyAlert_A',0x05:'SafetyFault_B',0x12:'BatStatus',0x14:'Cell1',0x16:'Cell2',0x18:'Cell3',0x34:'VStack',0x36:'VPack',0x3A:'CC2_Cur',0x3E:'Subcmd',0x40:'Resp_Buf',0x68:'IntTemp',0x70:'TS1',0x72:'TS2',0x74:'TS3',0x7F:'FET_Stat',0x83:'CBActive'};
const cg=document.getElementById('cellGrid');
for(let i=0;i<CELLS;i++){cg.innerHTML+=`<div class="cell-box" id="cbox${i}"><div style="display:flex;justify-content:space-between"><div class="cell-label">CELL ${i+1}</div><div id="bal${i}" class="badge badge-red" style="display:none;font-size:8px;padding:1px 3px">BAL</div></div><div class="cell-val" id="cv${i}">--<span class="cell-unit">V</span></div></div>`;}
let logOpen=true,ledOn=false,startTime=Date.now(),logs=[];
let vHist=Array.from({length:CELLS},()=>[]),curHist=[],tHist=[[],[],[]];
const HMAX=120;
function switchTab(n){document.getElementById('dashTab').style.display=n===0?'block':'none';document.getElementById('plotsTab').style.display=n===1?'block':'none';document.getElementById('tab0').className='tab'+(n===0?' active':'');document.getElementById('tab1').className='tab'+(n===1?' active':'');if(n===1)drawCharts();}
function toggleLog(){logOpen=!logOpen;document.getElementById('logPanel').style.height=logOpen?'160px':'30px';document.getElementById('logBody').style.display=logOpen?'block':'none';document.getElementById('logArrow').innerHTML=logOpen?'&#x25BE;':'&#x25B8;';}
function clearLog(){logs=[];renderLog();}
function renderLog(){const tb=document.getElementById('logTbody');let h='';for(const l of logs.slice(-40)){h+=`<tr><td style="color:#9ca3ae">${l.ts}</td><td class="${l.wr?'log-write':'log-read'}">${l.wr?'WRITE':'READ'}</td><td>0x${l.addr.toString(16).toUpperCase().padStart(2,'0')}</td><td style="color:#5f6672">${regNames[l.addr]||'?'}</td><td>0x${l.val.toString(16).toUpperCase().padStart(4,'0')}</td><td class="${l.ok?'log-ack':'log-nack'}">${l.ok?'ACK':'NACK'}</td></tr>`;}tb.innerHTML=h;const lb=document.getElementById('logBody');lb.scrollTop=lb.scrollHeight;}
function tempColor(v){return v>45?'#dc2626':v>35?'#d97706':'#16a34a';}
function updateUI(d){
  try {
  const volts=d.v.slice(0,CELLS).map(mv=>mv/1000);
  const minV=Math.min(...volts),maxV=Math.max(...volts);
  const delta=((maxV-minV)*1000).toFixed(0);
  const pack=volts.reduce((a,b)=>a+b,0);
  const soc = d.cc_soc !== undefined ? d.cc_soc : Math.min(100,Math.max(0,((minV-3.0)/(4.2-3.0))*100));
  let activeBal = "";
  for(let i=0;i<CELLS;i++){
    const isBal = (d.bal & (1 << i)) || (i == CELLS-1 && (d.bal & (1 << 15)));
    document.getElementById('bal'+i).style.display = isBal ? 'block' : 'none';
    if(isBal) activeBal += `<span class="badge badge-red">CELL ${i+1}</span>`;
    document.getElementById('cv'+i).innerHTML=volts[i].toFixed(3)+'<span class="cell-unit">V</span>';
    document.getElementById('cbox'+i).className='cell-box'+(volts[i]<2.7||volts[i]>4.1?' warn':'');
    document.getElementById('cv'+i).style.color=(volts[i]<2.7||volts[i]>4.1)?'#dc2626':'#1a1d23';
  }
  document.getElementById('balFlags').innerHTML = activeBal || '<span class="badge badge-green">NONE</span>';
  
  // Update Sealing & Power Status
  const sec = (d.manStat >> 4) & 0x03;
  document.getElementById('statSeal').innerHTML = (sec === 3 ? "SEALED" : (sec === 2 ? "UNSEALED" : "FULL ACCESS"));
  document.getElementById('statSeal').style.color = (sec === 3 ? "#16a34a" : "#dc2626");
  
  const pModes = ["ACTIVE", "SLEEP", "DEEP SLEEP", "SHUTDOWN"];
  let curMode = pModes[d.pwr] || "OFFLINE";
  if (d.pendingPwr && d.pendingPwr > 0) curMode = "ENTERING " + pModes[d.pendingPwr] + "...";
  document.getElementById('statPwr').innerHTML = curMode;
  document.getElementById('statPwr').style.color = (d.pendingPwr > 1 || d.pwr > 1) ? "#dc2626" : (d.pwr === 1 ? "#d97706" : "#2563eb");
  
  document.getElementById('kvPack').innerHTML=pack.toFixed(2)+'<span class="kv-unit">V</span>';
  document.getElementById('kvMin').innerHTML=minV.toFixed(3)+'<span class="kv-unit">V</span>';
  document.getElementById('kvMax').innerHTML=maxV.toFixed(3)+'<span class="kv-unit">V</span>';
  const de=document.getElementById('kvDelta');de.innerHTML=delta+'<span class="kv-unit">mV</span>';de.style.color=parseInt(delta)>30?'#d97706':'#16a34a';
  const cur=d.current;const ce=document.getElementById('pCurrent');ce.innerHTML=(cur>0?'+':'')+cur+'<span class="cell-unit">mA</span>';ce.style.color=cur<0?'#ea580c':cur>0?'#2563eb':'#9ca3ae';
  const cs=document.getElementById('pCurState');cs.textContent=cur>0?'CHARGING':cur<0?'DISCHARGING':'IDLE';cs.style.color=cur>0?'#2563eb':cur<0?'#ea580c':'#9ca3ae';
  document.getElementById('pSoc').innerHTML=soc.toFixed(0)+'<span class="cell-unit">%</span>';document.getElementById('pSoc').style.color=soc<20?'#dc2626':soc<40?'#d97706':'#16a34a';
  const sb=document.getElementById('pSocBar');sb.style.width=soc+'%';sb.style.background=soc<20?'#dc2626':soc<40?'#d97706':'#16a34a';
  document.getElementById('pPower').innerHTML=(pack*Math.abs(cur)/1000).toFixed(2)+'<span class="cell-unit">W</span>';
  document.getElementById('kvCharge').innerHTML=d.charge.toFixed(1)+'<span class="kv-unit">mAh</span>';
  const tr = document.getElementById('kvTimeL');
  tr.textContent = cur>0?'Charge Time':cur<0?'Dsg Time':'Idle Time';
  document.getElementById('kvChargeTime').innerHTML=d.chargeTime+'<span class="kv-unit">s</span>';
  document.getElementById('kvStack').innerHTML=d.vStack+'<span class="kv-unit">mV</span>';
  document.getElementById('kvPackMv').innerHTML=d.vPack+'<span class="kv-unit">mV</span>';
  if (d.soc_ekf !== undefined) {
    document.getElementById('kvSOC_EKF').innerHTML = d.soc_ekf.toFixed(1) + '<span class="cell-unit">%</span>';
    document.getElementById('kvSOC_Unc').textContent = d.soc_uncertainty.toFixed(2);
  }
  [{id:'tTs1',v:d.temp1},{id:'tTs2',v:d.temp2},{id:'tTs3',v:d.temp3},{id:'tChip',v:d.chipTemp}].forEach(t=>{const el=document.getElementById(t.id);el.textContent=t.v.toFixed(1)+'\u00B0C';el.style.color=tempColor(t.v);});
  const bc=document.getElementById('badgeChg');bc.textContent='CHG: '+(d.isCharging?'ON':'OFF');bc.className='badge '+(d.isCharging?'badge-green':'badge-red');
  const bd=document.getElementById('badgeDsg');bd.textContent='DSG: '+(d.isDischarging?'ON':'OFF');bd.className='badge '+(d.isDischarging?'badge-green':'badge-red');
  document.getElementById('fetChgToggle').className='toggle '+(d.isCharging?'on':'off');document.getElementById('fetChgBadge').textContent=d.isCharging?'ON':'OFF';document.getElementById('fetChgBadge').className='badge '+(d.isCharging?'badge-green':'badge-red');
  document.getElementById('fetDsgToggle').className='toggle '+(d.isDischarging?'on':'off');document.getElementById('fetDsgBadge').textContent=d.isDischarging?'ON':'OFF';document.getElementById('fetDsgBadge').className='badge '+(d.isDischarging?'badge-green':'badge-red');
  document.getElementById('fetWarn').style.display=(!d.isCharging&&!d.isDischarging)?'block':'none';
  document.getElementById('ledIndicator').className='led-indicator '+(ledOn?'on':'off');
  const pf=document.getElementById('protFlags');
  const flags=[{k:'prot_ov',l:'OV'},{k:'prot_uv',l:'UV'},{k:'prot_oc1',l:'OC1'},{k:'prot_oc2',l:'OC2'},{k:'prot_occ',l:'OCC'},{k:'prot_sc',l:'SC'},{k:'temp_otf',l:'OT-FET'},{k:'temp_oti',l:'OT-INT'},{k:'temp_otd',l:'OT-DSG'},{k:'temp_otc',l:'OT-CHG'},{k:'temp_uti',l:'UT-INT'},{k:'temp_utd',l:'UT-DSG'},{k:'temp_utc',l:'UT-CHG'}];
  pf.innerHTML=flags.map(f=>`<span class="badge ${d[f.k]?'badge-red':'badge-green'}">${f.l} ${d[f.k]?'FAULT':'OK'}</span>`).join('');
  // (Balancing UI updated in the main cell loop above)
  document.getElementById('sbConn').innerHTML='&#x25CF; Connected';document.getElementById('sbConn').style.color='#16a34a';
  for(let i=0;i<CELLS;i++){vHist[i].push(volts[i]);if(vHist[i].length>HMAX)vHist[i].shift();}
  curHist.push(cur);if(curHist.length>HMAX)curHist.shift();
  tHist[0].push(d.temp1);tHist[1].push(d.temp2);tHist[2].push(d.temp3);for(let i=0;i<3;i++){if(tHist[i].length>HMAX)tHist[i].shift();}
  } catch(e) { console.error("UI Update Error:", e); }
}
function fetchData(){fetch('/api/data').then(r=>r.json()).then(d=>{updateUI(d);const el=Math.floor((Date.now()-startTime)/1000);document.getElementById('sbElapsed').textContent=Math.floor(el/60)+':'+String(el%60).padStart(2,'0');document.getElementById('sbTx').textContent='Tx: '+d.txCount;document.getElementById('logTxCount').textContent='('+d.txCount+')';}).catch(e=>{console.error("Fetch Data Error:", e);document.getElementById('sbConn').innerHTML='&#x25CF; Disconnected';document.getElementById('sbConn').style.color='#dc2626';});}
function fetchLog(){fetch('/api/log').then(r=>r.json()).then(entries=>{const now=new Date();for(const e of entries){logs.push({ts:now.toLocaleTimeString()+'.'+String(now.getMilliseconds()).padStart(3,'0'),addr:e.reg,val:e.value,wr:e.isWrite,ok:e.ok});}if(logs.length>200)logs=logs.slice(-200);renderLog();}).catch(()=>{});}
function sendCmd(action){
  if(action==='ledOn')ledOn=true; if(action==='ledOff')ledOn=false;
  if(action==='pwrSleep') document.getElementById('sbConn').innerHTML='&#x25CF; SLEEPING...';
  if(action==='pwrDeep') document.getElementById('sbConn').innerHTML='&#x25CF; DEEP SLEEP...';
  fetch('/api/cmd?action='+action).then(()=>setTimeout(fetchData,100));
}
function toggleFet(which){if(which==='chg'){sendCmd(document.getElementById('fetChgToggle').classList.contains('on')?'chgOff':'chgOn');}else{sendCmd(document.getElementById('fetDsgToggle').classList.contains('on')?'dsgOff':'dsgOn');}}
function drawChart(svgId,title,lines,h){const svg=document.getElementById(svgId);if(!svg)return;const allVals=lines.flatMap(l=>l.data);if(allVals.length===0)return;const minV=Math.min(...allVals),maxV=Math.max(...allVals),range=maxV-minV||1;const w=700,p={t:26,r:12,b:24,l:48},pw=w-p.l-p.r,ph=h-p.t-p.b;const toX=(i,len)=>p.l+(i/(len-1))*pw,toY=(v)=>p.t+ph-((v-minV)/range)*ph;let s=`<rect x="${p.l}" y="${p.t}" width="${pw}" height="${ph}" fill="#f7f8fa" rx="3"/>`;for(let i=0;i<=4;i++){const v=minV+(range*i)/4,y=toY(v);s+=`<line x1="${p.l}" y1="${y}" x2="${p.l+pw}" y2="${y}" stroke="#e0e3e8" stroke-width="0.5"/><text x="${p.l-6}" y="${y+3}" fill="#9ca3ae" font-size="8" text-anchor="end" font-family="inherit">${v.toFixed(lines[0].dec||1)}</text>`;}['60s','45s','30s','15s','now'].forEach((l,i)=>{s+=`<text x="${p.l+(pw*i)/4}" y="${h-4}" fill="#9ca3ae" font-size="8" text-anchor="middle" font-family="inherit">${l}</text>`;});lines.forEach(line=>{if(line.data.length<2)return;s+=`<polyline points="${line.data.map((v,i)=>`${toX(i,line.data.length)},${toY(v)}`).join(' ')}" fill="none" stroke="${line.color}" stroke-width="1.5" stroke-linejoin="round" opacity="0.85"/>`;});s+=`<text x="${p.l+4}" y="${p.t-8}" fill="#5f6672" font-size="10" font-weight="600" font-family="inherit">${title}</text>`;let lx=p.l+pw-lines.length*72;lines.forEach((l,i)=>{const x=lx+i*72;s+=`<line x1="${x}" y1="${p.t-11}" x2="${x+12}" y2="${p.t-11}" stroke="${l.color}" stroke-width="2"/><text x="${x+16}" y="${p.t-8}" fill="#5f6672" font-size="8" font-family="inherit">${l.label}</text>`;});svg.innerHTML=s;}
function drawCharts(){drawChart('chartVoltage','Cell Voltages (V)',[{label:'Cell 1',color:'#2563eb',data:vHist[0]||[],dec:3},{label:'Cell 2',color:'#16a34a',data:vHist[1]||[],dec:3},{label:'Cell 3',color:'#d97706',data:vHist[2]||[],dec:3}],200);drawChart('chartCurrent','Pack Current (mA)',[{label:'Current',color:'#ea580c',data:curHist,dec:0}],160);drawChart('chartTemp','Temperatures (\u00B0C)',[{label:'TS1',color:'#dc2626',data:tHist[0]||[],dec:1},{label:'TS2',color:'#7c3aed',data:tHist[1]||[],dec:1},{label:'TS3',color:'#0891b2',data:tHist[2]||[],dec:1}],160);}
setInterval(()=>{fetchData();fetchLog();},1000);setInterval(drawCharts,1000);fetchData();
</script>
</body>
</html>
)rawliteral";

// ==================== WEB HANDLERS ====================
void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

void handleApiData()
{
  float current_A = (float)bmsCurrent / 1000.0f;
  float parasitic_correction_mv = (fabsf(current_A) > 0.05f) ? (current_A * 2.43f * 1000.0f) : 0.0f;
  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  
  server.sendContent("{\"v\":[");
  for (int i = 0; i < 16; i++) { 
      float v = (i < TB_CONNECTED_CELLS) ? ((float)cellVoltages[i] - parasitic_correction_mv) : (float)cellVoltages[i];
      server.sendContent(String(v, 0)); 
      if (i < 15) server.sendContent(","); 
  }
  server.sendContent("],\"vStack\":" + String(vStack));
  server.sendContent(",\"vPack\":" + String(vPack));
  server.sendContent(",\"current\":" + String(bmsCurrent));
  server.sendContent(",\"charge\":" + String(software_charge_Ah * 1000.0f, 1));
  server.sendContent(",\"chargeTime\":" + String(chargeTime));
  server.sendContent(",\"chipTemp\":" + String(chipTemp, 1));
  server.sendContent(",\"temp1\":" + String(temp1, 1));
  server.sendContent(",\"temp2\":" + String(temp2, 1));
  server.sendContent(",\"temp3\":" + String(temp3, 1));
  server.sendContent(",\"isCharging\":" + String(isCharging ? 1 : 0));
  server.sendContent(",\"isDischarging\":" + String(isDischarging ? 1 : 0));
  server.sendContent(",\"fetEn\":" + String(fetEn ? 1 : 0));
  server.sendContent(",\"ledState\":" + String(ledState ? 1 : 0));
  server.sendContent(",\"prot_sc\":" + String(protStatus.bits.SC_DCHG ? 1 : 0));
  server.sendContent(",\"prot_oc2\":" + String(protStatus.bits.OC2_DCHG ? 1 : 0));
  server.sendContent(",\"prot_oc1\":" + String(protStatus.bits.OC1_DCHG ? 1 : 0));
  server.sendContent(",\"prot_occ\":" + String(protStatus.bits.OC_CHG ? 1 : 0));
  server.sendContent(",\"prot_ov\":" + String(protStatus.bits.CELL_OV ? 1 : 0));
  server.sendContent(",\"prot_uv\":" + String(protStatus.bits.CELL_UV ? 1 : 0));
  server.sendContent(",\"temp_otf\":" + String(tempStatus.bits.OVERTEMP_FET ? 1 : 0));
  server.sendContent(",\"temp_oti\":" + String(tempStatus.bits.OVERTEMP_INTERNAL ? 1 : 0));
  server.sendContent(",\"temp_otd\":" + String(tempStatus.bits.OVERTEMP_DCHG ? 1 : 0));
  server.sendContent(",\"temp_otc\":" + String(tempStatus.bits.OVERTEMP_CHG ? 1 : 0));
  server.sendContent(",\"temp_uti\":" + String(tempStatus.bits.UNDERTEMP_INTERNAL ? 1 : 0));
  server.sendContent(",\"temp_utd\":" + String(tempStatus.bits.UNDERTEMP_DCHG ? 1 : 0));
  server.sendContent(",\"temp_utc\":" + String(tempStatus.bits.UNDERTEMP_CHG ? 1 : 0));
  
  float cc_soc = 0.0f;
  if (initial_ekf_soc >= 0.0f) {
      cc_soc = constrain(initial_ekf_soc + (software_charge_Ah / 3.0f) * 100.0f, 0.0f, 100.0f);
  }
  server.sendContent(",\"cc_soc\":" + String(cc_soc, 1));
  server.sendContent(",\"soc_ekf\":" + String(soc_ekf, 1));
  server.sendContent(",\"soc_uncertainty\":" + String(soc_uncertainty, 2));
  server.sendContent(",\"vErr\":" + String(voltage_error_ekf, 1));
  server.sendContent(",\"bal\":" + String(balancingMask));
  server.sendContent(",\"manStat\":" + String(manufStatus));
  server.sendContent(",\"pwr\":" + String(powerMode));
  server.sendContent(",\"txCount\":" + String(txCount));
  server.sendContent(",\"pendingPwr\":" + String(pendingPwrMode));
  server.sendContent("}");
  server.sendContent(""); // Final empty chunk
}

void handleApiLog()
{
  String json = "[";
  int count = min(logCount, 10);
  int idx = (logHead - count + LOG_SIZE) % LOG_SIZE;
  for (int i = 0; i < count; i++)
  {
    int pos = (idx + i) % LOG_SIZE;
    if (i > 0) json += ",";
    json += "{\"reg\":" + String(i2cLog[pos].reg);
    json += ",\"value\":" + String(i2cLog[pos].value);
    json += ",\"isWrite\":" + String(i2cLog[pos].isWrite ? "1" : "0");
    json += ",\"ok\":" + String(i2cLog[pos].ok ? "1" : "0") + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
  
  // Prevent sending the same log entries to the UI on the next poll
  logCount = 0;
}

void handleApiCmd()
{
  String action = server.arg("action");
  if (action == "chgOn") {
    Serial.println("=== CHG ON (Hybrid) ===");
    chgManualOverrideUntil = millis() + MANUAL_OVERRIDE_MS;
    isCharging = true;
    digitalWrite(TB_PIN_CFETOFF, LOW); // Pin LOW allows ON
    bms.CommandOnlysubCommand(0x0096);  // Subcmd 0x0096 turns All Fets ON
    delay(50);
  }
  else if (action == "chgOff") {
    Serial.println("=== CHG OFF (Hybrid) ===");
    chgManualOverrideUntil = millis() + MANUAL_OVERRIDE_MS;
    isCharging = false;
    digitalWrite(TB_PIN_CFETOFF, HIGH); // Pin HIGH forces OFF
    bms.CommandOnlysubCommand(0x0094);   // Subcmd 0x0094 turns CHG OFF
    delay(50);
  }
  else if (action == "dsgOn") {
    Serial.println("=== DSG ON (Hybrid) ===");
    dsgManualOverrideUntil = millis() + MANUAL_OVERRIDE_MS;
    isDischarging = true;
    digitalWrite(TB_PIN_DFETOFF, LOW); // Pin LOW allows ON
    bms.CommandOnlysubCommand(0x0096);  // Subcmd 0x0096 turns All Fets ON
    delay(50);
  }
  else if (action == "dsgOff") {
    Serial.println("=== DSG OFF (Hybrid) ===");
    dsgManualOverrideUntil = millis() + MANUAL_OVERRIDE_MS;
    isDischarging = false;
    digitalWrite(TB_PIN_DFETOFF, HIGH); // Pin HIGH forces OFF
    bms.CommandOnlysubCommand(0x0093);   // Subcmd 0x0093 turns DSG OFF
    delay(50);
  }
  else if (action == "resetCharge"){ bms.ResetAccumulatedCharge(); addLog(0x3E, 0x0082, true, true); }
  else if (action == "reset")      { bms.reset();           addLog(0x3E, 0x0012, true, true); }
  else if (action == "pwrSleep")   { pendingPwrMode = 1; bms.setPowerMode(1); }
  else if (action == "pwrDeep")    { pendingPwrMode = 2; bms.setPowerMode(2); }
  else if (action == "pwrWake")    { 
    pendingPwrMode = 0;
    // Sequential I2C activity to force wake from Sleep
    bms.directCommandRead(0x12);
    delay(10);
    bms.getManufacturingStatus();
    delay(10);
    Serial.println("[TB] Wake command sent.");
  }
  else if (action == "fetMasterToggle") { bms.CommandOnlysubCommand(0x0022); Serial.println("[TB] FET Master Toggle (0x0022)"); }
  else if (action == "allFetsOn")  { bms.CommandOnlysubCommand(0x0096); }
  else if (action == "allFetsOff") { bms.CommandOnlysubCommand(0x0095); }
  else { server.send(400, "text/plain", "Unknown"); return; }
  server.send(200, "text/plain", "OK");
}

void handleApiEKFReset() {
    if (server.method() != HTTP_POST && server.method() != HTTP_GET) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    float V_cell = (float)cellVoltages[0] / 1000.0f;
    float I_current = (float)bmsCurrent / 1000.0f;
    float soc_est = smartSOCInit(V_cell, I_current);
    
    initial_ekf_soc = soc_est;
    software_charge_Ah = 0.0f; // Reset our robust software CC!
    bms.ResetAccumulatedCharge(); 
    ekf.begin(soc_est);
    
    String response = "{\"status\":\"ok\",\"new_soc\":" + String(soc_est) + "}";
    server.send(200, "application/json", response);
    
    Serial.printf("[EKF] Manual Reset to %.1f%%\n", soc_est);
}

// ==================== SETUP ====================
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[TB-Dash] === BQ76952 Test Board Dashboard ===");

  Wire.begin(TB_I2C_SDA, TB_I2C_SCL, 400000); // Back to 400kHz
  bms.begin(TB_I2C_SDA, TB_I2C_SCL); 
  bms.setDebug(true);
  
  // Set EKF Configuration (User Review Recommendations)
  ekf.setParasiticResistance(2.43f);
  ekf.setTrustGuardThreshold(0.120f); // Guard against pulse noise >120mV
  ekf.setMaxGainClamp(0.5f);        // 0.5% max SOC change per loop
  
  Serial.printf("[TB-Dash] I2C Master SDA=%d SCL=%d\n", TB_I2C_SDA, TB_I2C_SCL);

  bms.reset(); // RESTORED: Ensuring a clean boot state for testing
  delay(100);  // Wait for reset to complete
  
  bms.unseal(); // Required to write to Data Memory
  delay(50);
  
  // Power Config (0x9234): Clear bit 0 (SLEEP) to disable autonomous sleep
  // Sleep Current (0x9237): Set to 0 to disable sleep threshold
  bms.writeIntToMemory(0x9234, 0x2980); 
  delay(20);
  bms.writeIntToMemory(0x9237, 0); 
  delay(20);
  
  // Re-apply fundamental config (as reset clears it)
  bms.setConnectedCells(TB_CONNECTED_CELLS); 
  
  Serial.println("[TB-Dash] BQ76952 Reset & Auto-Sleep Disabled.");
  delay(100);

  // The BQ76952 defaults to FULLACCESS mode (BatStatus=0x8184 is normal!).
  // So we do not need to send UNSEAL keys.


  // 2. Force EXIT config mode if stuck
  Serial.println("[TB-Dash] Forcing EXIT_CONFIG_UPDATE...");
  bms.CommandOnlysubCommand(0x0092);
  delay(200);

  // 3. Check initial status
  unsigned int batStat = bms.directCommandRead(0x12);
  Serial.printf("[TB-Dash] Initial BatStatus=0x%04X\n", batStat);

  // 4. Configure cell count
  Serial.printf("[TB-Dash] Configuring for %d cells...\n", TB_CONNECTED_CELLS);
  bms.setConnectedCells(TB_CONNECTED_CELLS);
  delay(100);

  // 5. Enter CONFIG_UPDATE mode to write settings
  Serial.println("[TB-Dash] Entering CONFIG_UPDATE mode...");
  bms.CommandOnlysubCommand(0x0090);
  delay(100);

  // 6. Write protection thresholds
  Serial.println("[TB-Dash] Writing protection thresholds...");
  bms.writeByteToMemory(0x9275, 59);  // CUV ~3.0V
  bms.writeByteToMemory(0x9278, 83);  // COV ~4.2V
  bms.writeByteToMemory(0x9280, 2);   // OCC 4mV
  bms.writeByteToMemory(0x9282, 4);   // OCD1 8mV
  delay(50);

  // 7. Enable FET control
  Serial.println("[TB-Dash] Enabling FET control (FET Options)...");
  bms.writeByteToMemory(0x9308, 0x0F);  // Enable all FET functions
  delay(50);
  
  // 7.1 Disable Autonomous Balancing to allow Host-Controlled mask (0x0083)
  Serial.println("[TB-Dash] Disabling Autonomous Balancing (Host Only Mode)...");
  bms.writeByteToMemory(0x9335, 0x00); 
  delay(50);

  // 8. Exit CONFIG_UPDATE mode
  Serial.println("[TB-Dash] Exiting CONFIG_UPDATE mode...");
  bms.CommandOnlysubCommand(0x0092);
  delay(200);

  // 9. Verify Configuration
  batStat = bms.directCommandRead(0x12);
  Serial.printf("[TB-Dash] Final BatStatus=0x%04X (SEC1:0=%d)\n", 
                batStat, (batStat >> 8) & 0x03);

  // IMPORTANT: Ensure Autonomous FET Control is enabled (FET_EN bit 11 in BatStatus).
  // If it's disabled, the BQ76952 completely ignores the CFETOFF/DFETOFF hardware pins!
  bool currentFetEn = (batStat & 0x0800) != 0;
  if (!currentFetEn) {
    Serial.println("[TB-Dash] FET_EN is 0! Sending 0x0022 to enable autonomous control so pins work...");
    bms.CommandOnlysubCommand(0x0022); // Toggle FET_EN
    delay(50);
  } else {
    Serial.println("[TB-Dash] FET_EN is already 1. Autonomous mode active.");
  }

  // Removed fetEnResult logic

  WiFi.mode(WIFI_AP);
  WiFi.softAP(TB_AP_SSID, TB_AP_PASSWORD);
  Serial.printf("[TB-Dash] AP: %s  IP: %s\n", TB_AP_SSID, WiFi.softAPIP().toString().c_str());

  // Initialize EKF
  readBMSData(); // Refresh cell voltages once for init
  float V_init = (float)cellVoltages[0] / 1000.0f;
  float I_init = (float)bmsCurrent / 1000.0f;
  float SOC_init = smartSOCInit(V_init, I_init);
  initial_ekf_soc = SOC_init; // Store for Coulomb Counter
  ekf.setNNEnabled(true);
  ekf.setAdaptiveTuning(true);
  ekf.begin(SOC_init);
  Serial.printf("[EKF] Initialized with SOC=%.1f%%\n", SOC_init);

  pinMode(TB_PIN_CFETOFF, OUTPUT);
  pinMode(TB_PIN_DFETOFF, OUTPUT);
  
  // Hardware Logic: HIGH = OFF, LOW = ON (Force OFF by default)
  digitalWrite(TB_PIN_CFETOFF, HIGH); 
  digitalWrite(TB_PIN_DFETOFF, HIGH);
  isCharging = false;
  isDischarging = false; // Force OFF via hardware by default until toggled

  server.on("/", handleRoot);
  server.on("/api/data", handleApiData);
  server.on("/api/log", handleApiLog);
  server.on("/api/cmd", handleApiCmd);
  server.on("/api/ekf_reset", handleApiEKFReset);
  server.begin();

  Serial.printf("[TB-Dash] Connect to '%s' pw '%s' -> http://%s\n", TB_AP_SSID, TB_AP_PASSWORD, WiFi.softAPIP().toString().c_str());
}

// ==================== LOOP ====================
void loop()
{
  server.handleClient();
  if (millis() - lastRead >= READ_INTERVAL_MS)
  {
    lastRead = millis();
    readBMSData();
  }
  if (millis() - lastEKFUpdate >= EKF_UPDATE_INTERVAL)
  {
    lastEKFUpdate = millis();
    updateEKF();
    runBalancing(); // Run balancing check every second
  }
  delay(2);
}
