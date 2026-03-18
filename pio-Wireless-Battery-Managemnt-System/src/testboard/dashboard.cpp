#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "BQ76952.h"
#include "tb_config.h"

// ==================== CUSTOM SUBCOMMANDS (match bq_node.cpp) ====================
#define SUBCMD_LED_ON  0x00FE
#define SUBCMD_LED_OFF 0x00FF

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
bool ledState = false;
bq_protection_t protStatus;
bq_temp_t tempStatus;
uint16_t balancingMask = 0;

unsigned long lastRead = 0;
const uint32_t READ_INTERVAL_MS = 500;
uint32_t txCount = 0;

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
  for (int i = 0; i < 16; i++)
  {
    if (i < TB_CONNECTED_CELLS)
    {
      cellVoltages[i] = bms.getCellVoltage(i + 1);
      addLog(0x14 + i * 2, cellVoltages[i], false, cellVoltages[i] > 0);
    }
    else
    {
      cellVoltages[i] = 0;
    }
  }

  vStack = bms.getCellVoltage(17);
  addLog(0x34, vStack, false, true);

  vPack = bms.getCellVoltage(18);
  addLog(0x36, vPack, false, true);

  bmsCurrent = bms.getCurrent();
  addLog(0x3A, (uint16_t)(int16_t)bmsCurrent, false, true);

  chipTemp = bms.getInternalTemp();
  addLog(0x68, (uint16_t)(chipTemp * 10), false, true);

  temp1 = bms.getThermistorTemp(TS1);
  addLog(0x70, (uint16_t)(temp1 * 10), false, true);

  temp2 = bms.getThermistorTemp(TS2);
  addLog(0x72, (uint16_t)(temp2 * 10), false, true);

  temp3 = bms.getThermistorTemp(TS3);
  addLog(0x74, (uint16_t)(temp3 * 10), false, true);

  charge = bms.getAccumulatedCharge();
  chargeTime = bms.getAccumulatedChargeTime();

  isCharging = bms.isCharging();
  isDischarging = bms.isDischarging();
  addLog(0x7F, (isCharging ? 0x01 : 0) | (isDischarging ? 0x04 : 0), false, true);

  protStatus = bms.getProtectionStatus();
  addLog(0x03, 0, false, true);

  tempStatus = bms.getTemperatureStatus();
  addLog(0x05, 0, false, true);

  balancingMask = bms.GetCellBalancingBitmask();
  addLog(0x83, balancingMask, false, true);
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
        <div class="cell-box"><div class="cell-label">SOC (estimated)</div><div class="cell-val" id="pSoc">--<span class="cell-unit">%</span></div><div class="pbar"><div class="pbar-fill" id="pSocBar" style="width:0%;background:#16a34a"></div></div></div>
        <div class="cell-box"><div class="cell-label">POWER</div><div class="cell-val" id="pPower">--<span class="cell-unit">W</span></div></div>
      </div>
      <div class="g4">
        <div><div class="kv-label">Accum. Charge</div><div class="kv-val" id="kvCharge">--<span class="kv-unit">Ah</span></div></div>
        <div><div class="kv-label">Charge Time</div><div class="kv-val" id="kvChargeTime">--<span class="kv-unit">s</span></div></div>
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
</div>
<div class="divider">
  <div class="section-title">Device Controls</div>
  <div class="g3">
    <div class="card">
      <div class="card-head"><h2>I2C Verification</h2><div class="desc">Toggle the BQ node's LED via I2C to prove communication</div></div>
      <div class="card-body">
        <div class="led-card">
          <div class="led-indicator" id="ledIndicator"></div>
          <div style="flex:1">
            <div style="font-size:11px;font-weight:600;margin-bottom:4px">Remote LED (BQ Node GPIO 2)</div>
            <div style="font-size:9px;color:#9ca3ae;margin-bottom:8px">Sends subcommand 0x00FE / 0x00FF over I2C</div>
            <div style="display:flex;gap:6px"><button class="btn primary" onclick="sendCmd('ledOn')">LED ON</button><button class="btn danger" onclick="sendCmd('ledOff')">LED OFF</button></div>
          </div>
        </div>
        <div style="font-size:9px;color:#9ca3ae;margin-top:8px">If the LED on the other ESP toggles, I2C is working.</div>
      </div>
    </div>
    <div class="card">
      <div class="card-head"><h2>FET Control</h2><div class="desc">Charge/discharge MOSFET switching via I2C subcommands</div></div>
      <div class="card-body">
        <div class="fet-row"><div><div style="font-size:11px;font-weight:500">Charge FET</div><div style="font-size:9px;color:#9ca3ae">Subcmd 0x0094 (off) / 0x0096 (all on)</div></div><div style="display:flex;align-items:center;gap:8px"><span class="badge" id="fetChgBadge">--</span><button class="toggle off" id="fetChgToggle" onclick="toggleFet('chg')"><div class="toggle-dot"></div></button></div></div>
        <div class="fet-row"><div><div style="font-size:11px;font-weight:500">Discharge FET</div><div style="font-size:9px;color:#9ca3ae">Subcmd 0x0093 (off) / 0x0096 (all on)</div></div><div style="display:flex;align-items:center;gap:8px"><span class="badge" id="fetDsgBadge">--</span><button class="toggle off" id="fetDsgToggle" onclick="toggleFet('dsg')"><div class="toggle-dot"></div></button></div></div>
        <div id="fetWarn" style="display:none" class="warn-box">Both FETs off — pack is isolated</div>
      </div>
    </div>
    <div class="card">
      <div class="card-head"><h2>Reset &amp; Utilities</h2><div class="desc">Hardware reset and charge counter control</div></div>
      <div class="card-body">
        <div style="display:flex;flex-direction:column;gap:8px">
          <div style="display:flex;align-items:center;justify-content:space-between"><div><div style="font-size:11px">Reset BMS IC</div><div style="font-size:9px;color:#9ca3ae">Subcmd 0x0012</div></div><button class="btn danger" onclick="sendCmd('reset')">Reset</button></div>
          <div style="border-top:1px solid #ebedf0;padding-top:8px;display:flex;align-items:center;justify-content:space-between"><div><div style="font-size:11px">Reset Charge Counter</div><div style="font-size:9px;color:#9ca3ae">Subcmd 0x0082</div></div><button class="btn danger" onclick="sendCmd('resetCharge')">Reset</button></div>
          <div style="border-top:1px solid #ebedf0;padding-top:8px;display:flex;align-items:center;justify-content:space-between"><div><div style="font-size:11px">All FETs ON</div><div style="font-size:9px;color:#9ca3ae">Subcmd 0x0096</div></div><button class="btn primary" onclick="sendCmd('allFetsOn')">Enable</button></div>
          <div style="border-top:1px solid #ebedf0;padding-top:8px;display:flex;align-items:center;justify-content:space-between"><div><div style="font-size:11px;color:#dc2626;font-weight:600">All FETs OFF</div><div style="font-size:9px;color:#9ca3ae">Subcmd 0x0095</div></div><button class="btn danger" onclick="sendCmd('allFetsOff')">Kill</button></div>
        </div>
      </div>
    </div>
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
const regNames={0x14:'Cell1',0x16:'Cell2',0x18:'Cell3',0x34:'VStack',0x36:'VPack',0x3A:'CC2_Cur',0x68:'IntTemp',0x70:'TS1',0x72:'TS2',0x74:'TS3',0x7F:'FET_Stat'};
const cg=document.getElementById('cellGrid');
for(let i=0;i<CELLS;i++){cg.innerHTML+=`<div class="cell-box" id="cbox${i}"><div class="cell-label">CELL ${i+1}</div><div class="cell-val" id="cv${i}">--<span class="cell-unit">V</span></div></div>`;}
let logOpen=true,ledOn=false,startTime=Date.now(),logs=[];
let vHist=Array.from({length:CELLS},()=>[]),curHist=[],tHist=[[],[],[]];
const HMAX=120;
function switchTab(n){document.getElementById('dashTab').style.display=n===0?'block':'none';document.getElementById('plotsTab').style.display=n===1?'block':'none';document.getElementById('tab0').className='tab'+(n===0?' active':'');document.getElementById('tab1').className='tab'+(n===1?' active':'');if(n===1)drawCharts();}
function toggleLog(){logOpen=!logOpen;document.getElementById('logPanel').style.height=logOpen?'160px':'30px';document.getElementById('logBody').style.display=logOpen?'block':'none';document.getElementById('logArrow').innerHTML=logOpen?'&#x25BE;':'&#x25B8;';}
function clearLog(){logs=[];renderLog();}
function renderLog(){const tb=document.getElementById('logTbody');let h='';for(const l of logs.slice(-40)){h+=`<tr><td style="color:#9ca3ae">${l.ts}</td><td class="${l.wr?'log-write':'log-read'}">${l.wr?'WRITE':'READ'}</td><td>0x${l.addr.toString(16).toUpperCase().padStart(2,'0')}</td><td style="color:#5f6672">${regNames[l.addr]||'?'}</td><td>0x${l.val.toString(16).toUpperCase().padStart(4,'0')}</td><td class="${l.ok?'log-ack':'log-nack'}">${l.ok?'ACK':'NACK'}</td></tr>`;}tb.innerHTML=h;const lb=document.getElementById('logBody');lb.scrollTop=lb.scrollHeight;}
function tempColor(v){return v>45?'#dc2626':v>35?'#d97706':'#16a34a';}
function updateUI(d){
  const volts=d.v.slice(0,CELLS).map(mv=>mv/1000);
  const minV=Math.min(...volts),maxV=Math.max(...volts);
  const delta=((maxV-minV)*1000).toFixed(0);
  const pack=volts.reduce((a,b)=>a+b,0);
  const soc=Math.min(100,Math.max(0,((minV-3.0)/(4.2-3.0))*100));
  for(let i=0;i<CELLS;i++){document.getElementById('cv'+i).innerHTML=volts[i].toFixed(3)+'<span class="cell-unit">V</span>';document.getElementById('cbox'+i).className='cell-box'+(volts[i]<2.7||volts[i]>4.1?' warn':'');document.getElementById('cv'+i).style.color=(volts[i]<2.7||volts[i]>4.1)?'#dc2626':'#1a1d23';}
  document.getElementById('kvPack').innerHTML=pack.toFixed(2)+'<span class="kv-unit">V</span>';
  document.getElementById('kvMin').innerHTML=minV.toFixed(3)+'<span class="kv-unit">V</span>';
  document.getElementById('kvMax').innerHTML=maxV.toFixed(3)+'<span class="kv-unit">V</span>';
  const de=document.getElementById('kvDelta');de.innerHTML=delta+'<span class="kv-unit">mV</span>';de.style.color=parseInt(delta)>30?'#d97706':'#16a34a';
  const cur=d.current;const ce=document.getElementById('pCurrent');ce.innerHTML=(cur>0?'+':'')+cur+'<span class="cell-unit">mA</span>';ce.style.color=cur<0?'#ea580c':cur>0?'#2563eb':'#9ca3ae';
  const cs=document.getElementById('pCurState');cs.textContent=cur>0?'CHARGING':cur<0?'DISCHARGING':'IDLE';cs.style.color=cur>0?'#2563eb':cur<0?'#ea580c':'#9ca3ae';
  document.getElementById('pSoc').innerHTML=soc.toFixed(0)+'<span class="cell-unit">%</span>';document.getElementById('pSoc').style.color=soc<20?'#dc2626':soc<40?'#d97706':'#16a34a';
  const sb=document.getElementById('pSocBar');sb.style.width=soc+'%';sb.style.background=soc<20?'#dc2626':soc<40?'#d97706':'#16a34a';
  document.getElementById('pPower').innerHTML=(pack*Math.abs(cur)/1000).toFixed(2)+'<span class="cell-unit">W</span>';
  document.getElementById('kvCharge').innerHTML=d.charge.toFixed(1)+'<span class="kv-unit">Ah</span>';
  document.getElementById('kvChargeTime').innerHTML=d.chargeTime+'<span class="kv-unit">s</span>';
  document.getElementById('kvStack').innerHTML=d.vStack+'<span class="kv-unit">mV</span>';
  document.getElementById('kvPackMv').innerHTML=d.vPack+'<span class="kv-unit">mV</span>';
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
  const bf=document.getElementById('balFlags');
  let bh='';for(let i=0;i<CELLS;i++){const active=(d.balancing>>i)&1;bh+=`<span class="badge ${active?'badge-amber':'badge-gray'}">C${i+1} ${active?'BAL':'--'}</span>`;}bf.innerHTML=bh;
  document.getElementById('sbConn').innerHTML='&#x25CF; Connected';document.getElementById('sbConn').style.color='#16a34a';
  for(let i=0;i<CELLS;i++){vHist[i].push(volts[i]);if(vHist[i].length>HMAX)vHist[i].shift();}
  curHist.push(cur);if(curHist.length>HMAX)curHist.shift();
  tHist[0].push(d.temp1);tHist[1].push(d.temp2);tHist[2].push(d.temp3);for(let i=0;i<3;i++){if(tHist[i].length>HMAX)tHist[i].shift();}
}
function fetchData(){fetch('/api/data').then(r=>r.json()).then(d=>{updateUI(d);const el=Math.floor((Date.now()-startTime)/1000);document.getElementById('sbElapsed').textContent=Math.floor(el/60)+':'+String(el%60).padStart(2,'0');document.getElementById('sbTx').textContent='Tx: '+d.txCount;document.getElementById('logTxCount').textContent='('+d.txCount+')';}).catch(()=>{document.getElementById('sbConn').innerHTML='&#x25CF; Disconnected';document.getElementById('sbConn').style.color='#dc2626';});}
function fetchLog(){fetch('/api/log').then(r=>r.json()).then(entries=>{const now=new Date();for(const e of entries){logs.push({ts:now.toLocaleTimeString()+'.'+String(now.getMilliseconds()).padStart(3,'0'),addr:e.reg,val:e.value,wr:e.isWrite,ok:e.ok});}if(logs.length>200)logs=logs.slice(-200);renderLog();}).catch(()=>{});}
function sendCmd(action){if(action==='ledOn')ledOn=true;if(action==='ledOff')ledOn=false;document.getElementById('ledIndicator').className='led-indicator '+(ledOn?'on':'off');fetch('/api/cmd?action='+action).then(()=>setTimeout(fetchData,200));}
function toggleFet(which){if(which==='chg'){sendCmd(document.getElementById('fetChgToggle').classList.contains('on')?'chgOff':'allFetsOn');}else{sendCmd(document.getElementById('fetDsgToggle').classList.contains('on')?'dsgOff':'allFetsOn');}}
function drawChart(svgId,title,lines,h){const svg=document.getElementById(svgId);if(!svg)return;const allVals=lines.flatMap(l=>l.data);if(allVals.length===0)return;const minV=Math.min(...allVals),maxV=Math.max(...allVals),range=maxV-minV||1;const w=700,p={t:26,r:12,b:24,l:48},pw=w-p.l-p.r,ph=h-p.t-p.b;const toX=(i,len)=>p.l+(i/(len-1))*pw,toY=(v)=>p.t+ph-((v-minV)/range)*ph;let s=`<rect x="${p.l}" y="${p.t}" width="${pw}" height="${ph}" fill="#f7f8fa" rx="3"/>`;for(let i=0;i<=4;i++){const v=minV+(range*i)/4,y=toY(v);s+=`<line x1="${p.l}" y1="${y}" x2="${p.l+pw}" y2="${y}" stroke="#e0e3e8" stroke-width="0.5"/><text x="${p.l-6}" y="${y+3}" fill="#9ca3ae" font-size="8" text-anchor="end" font-family="inherit">${v.toFixed(lines[0].dec||1)}</text>`;}['60s','45s','30s','15s','now'].forEach((l,i)=>{s+=`<text x="${p.l+(pw*i)/4}" y="${h-4}" fill="#9ca3ae" font-size="8" text-anchor="middle" font-family="inherit">${l}</text>`;});lines.forEach(line=>{if(line.data.length<2)return;s+=`<polyline points="${line.data.map((v,i)=>`${toX(i,line.data.length)},${toY(v)}`).join(' ')}" fill="none" stroke="${line.color}" stroke-width="1.5" stroke-linejoin="round" opacity="0.85"/>`;});s+=`<text x="${p.l+4}" y="${p.t-8}" fill="#5f6672" font-size="10" font-weight="600" font-family="inherit">${title}</text>`;let lx=p.l+pw-lines.length*72;lines.forEach((l,i)=>{const x=lx+i*72;s+=`<line x1="${x}" y1="${p.t-11}" x2="${x+12}" y2="${p.t-11}" stroke="${l.color}" stroke-width="2"/><text x="${x+16}" y="${p.t-8}" fill="#5f6672" font-size="8" font-family="inherit">${l.label}</text>`;});svg.innerHTML=s;}
function drawCharts(){drawChart('chartVoltage','Cell Voltages (V)',[{label:'Cell 1',color:'#2563eb',data:vHist[0]||[],dec:3},{label:'Cell 2',color:'#16a34a',data:vHist[1]||[],dec:3},{label:'Cell 3',color:'#d97706',data:vHist[2]||[],dec:3}],200);drawChart('chartCurrent','Pack Current (mA)',[{label:'Current',color:'#ea580c',data:curHist,dec:0}],160);drawChart('chartTemp','Temperatures (\u00B0C)',[{label:'TS1',color:'#dc2626',data:tHist[0]||[],dec:1},{label:'TS2',color:'#7c3aed',data:tHist[1]||[],dec:1},{label:'TS3',color:'#0891b2',data:tHist[2]||[],dec:1}],160);}
setInterval(()=>{fetchData();fetchLog();},600);setInterval(drawCharts,2000);fetchData();
</script>
</body>
</html>
)rawliteral";

// ==================== WEB HANDLERS ====================
void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

void handleApiData()
{
  String json = "{\"v\":[";
  for (int i = 0; i < 16; i++) { json += String(cellVoltages[i]); if (i < 15) json += ","; }
  json += "],\"vStack\":" + String(vStack) + ",\"vPack\":" + String(vPack);
  json += ",\"current\":" + String(bmsCurrent);
  json += ",\"charge\":" + String(charge, 1) + ",\"chargeTime\":" + String(chargeTime);
  json += ",\"chipTemp\":" + String(chipTemp, 1);
  json += ",\"temp1\":" + String(temp1, 1) + ",\"temp2\":" + String(temp2, 1) + ",\"temp3\":" + String(temp3, 1);
  json += ",\"isCharging\":" + String(isCharging ? "true" : "false");
  json += ",\"isDischarging\":" + String(isDischarging ? "true" : "false");
  json += ",\"ledState\":" + String(ledState ? "true" : "false");
  json += ",\"prot_sc\":" + String(protStatus.bits.SC_DCHG ? "true" : "false");
  json += ",\"prot_oc2\":" + String(protStatus.bits.OC2_DCHG ? "true" : "false");
  json += ",\"prot_oc1\":" + String(protStatus.bits.OC1_DCHG ? "true" : "false");
  json += ",\"prot_occ\":" + String(protStatus.bits.OC_CHG ? "true" : "false");
  json += ",\"prot_ov\":" + String(protStatus.bits.CELL_OV ? "true" : "false");
  json += ",\"prot_uv\":" + String(protStatus.bits.CELL_UV ? "true" : "false");
  json += ",\"temp_otf\":" + String(tempStatus.bits.OVERTEMP_FET ? "true" : "false");
  json += ",\"temp_oti\":" + String(tempStatus.bits.OVERTEMP_INTERNAL ? "true" : "false");
  json += ",\"temp_otd\":" + String(tempStatus.bits.OVERTEMP_DCHG ? "true" : "false");
  json += ",\"temp_otc\":" + String(tempStatus.bits.OVERTEMP_CHG ? "true" : "false");
  json += ",\"temp_uti\":" + String(tempStatus.bits.UNDERTEMP_INTERNAL ? "true" : "false");
  json += ",\"temp_utd\":" + String(tempStatus.bits.UNDERTEMP_DCHG ? "true" : "false");
  json += ",\"temp_utc\":" + String(tempStatus.bits.UNDERTEMP_CHG ? "true" : "false");
  json += ",\"balancing\":" + String(balancingMask);
  json += ",\"txCount\":" + String(txCount) + "}";
  server.send(200, "application/json", json);
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
    json += ",\"isWrite\":" + String(i2cLog[pos].isWrite ? "true" : "false");
    json += ",\"ok\":" + String(i2cLog[pos].ok ? "true" : "false") + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleApiCmd()
{
  String action = server.arg("action");
  if (action == "allFetsOn")       { bms.setFET(ALL, ON);  addLog(0x3E, 0x0096, true, true); }
  else if (action == "allFetsOff") { bms.setFET(ALL, OFF); addLog(0x3E, 0x0095, true, true); }
  else if (action == "chgOff")     { bms.setFET(CHG, OFF); addLog(0x3E, 0x0094, true, true); }
  else if (action == "dsgOff")     { bms.setFET(DCH, OFF); addLog(0x3E, 0x0093, true, true); }
  else if (action == "resetCharge"){ bms.ResetAccumulatedCharge(); addLog(0x3E, 0x0082, true, true); }
  else if (action == "reset")      { bms.reset();           addLog(0x3E, 0x0012, true, true); }
  else if (action == "ledOn")      { bms.CommandOnlysubCommand(SUBCMD_LED_ON);  ledState = true;  addLog(0x3E, SUBCMD_LED_ON, true, true); }
  else if (action == "ledOff")     { bms.CommandOnlysubCommand(SUBCMD_LED_OFF); ledState = false; addLog(0x3E, SUBCMD_LED_OFF, true, true); }
  else { server.send(400, "text/plain", "Unknown"); return; }
  server.send(200, "text/plain", "OK");
}

// ==================== SETUP ====================
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[TB-Dash] === BQ76952 Test Board Dashboard ===");

  bms.begin(TB_I2C_SDA, TB_I2C_SCL);
  bms.setDebug(true);
  Serial.printf("[TB-Dash] I2C Master SDA=%d SCL=%d\n", TB_I2C_SDA, TB_I2C_SCL);

  // Configure BQ76952 to monitor the correct number of cells
  bms.setConnectedCells(TB_CONNECTED_CELLS);
  Serial.printf("[TB-Dash] Configured BQ76952 for %d cells\n", TB_CONNECTED_CELLS);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(TB_AP_SSID, TB_AP_PASSWORD);
  Serial.printf("[TB-Dash] AP: %s  IP: %s\n", TB_AP_SSID, WiFi.softAPIP().toString().c_str());

  server.on("/", handleRoot);
  server.on("/api/data", handleApiData);
  server.on("/api/log", handleApiLog);
  server.on("/api/cmd", handleApiCmd);
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
  delay(2);
}
