#pragma once
// web_api.h

// ==================== HTML PAGE ====================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Battery System Interface</title>
<link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;500;700&display=swap" rel="stylesheet">
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Outfit',sans-serif;background:linear-gradient(135deg,#0f172a,#1e293b);color:#f8fafc;min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:2rem 1rem}
#fault_banner{display:none;width:100%;max-width:420px;background:rgba(239,68,68,.2);border:1px solid #ef4444;color:#fca5a5;padding:1rem;border-radius:12px;text-align:center;font-weight:500;margin-bottom:1.5rem;animation:pulse 2s infinite}
@keyframes pulse{0%{box-shadow:0 0 0 0 rgba(239,68,68,.4)}70%{box-shadow:0 0 0 10px rgba(239,68,68,0)}100%{box-shadow:0 0 0 0 rgba(239,68,68,0)}}
.glass-card{background:rgba(255,255,255,.05);backdrop-filter:blur(16px);border:1px solid rgba(255,255,255,.1);border-radius:24px;padding:2rem;width:100%;max-width:420px;box-shadow:0 20px 40px rgba(0,0,0,.3);text-align:center;margin-bottom:1.5rem}
.progress-circle{position:relative;width:200px;height:200px;margin:0 auto 1rem}
svg.ring{width:100%;height:100%;transform:rotate(-90deg)}
circle.bg{fill:none;stroke:rgba(255,255,255,.1);stroke-width:8}
circle.progress{fill:none;stroke:#2ecc71;stroke-width:8;stroke-linecap:round;stroke-dasharray:565.48;stroke-dashoffset:565.48;transition:stroke-dashoffset 1s ease-out,stroke 1s ease-out}
.soc-content{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);display:flex;flex-direction:column;align-items:center}
#soc_text{font-size:3.5rem;font-weight:700;line-height:1;text-shadow:0 2px 10px rgba(0,0,0,.5)}
.soc-label{font-size:.8rem;color:#94a3b8;text-transform:uppercase;letter-spacing:2px;margin-top:.5rem}
.current-display{display:flex;align-items:center;justify-content:center;gap:.5rem;margin-bottom:1rem}
#current_text{font-size:1.6rem;font-weight:700;color:#94a3b8}
.current-unit{font-size:.9rem;color:#94a3b8;font-weight:300}
.current-dir{font-size:.7rem;padding:3px 8px;border-radius:6px;font-weight:500;letter-spacing:1px}
.dir-dsg{background:rgba(239,68,68,.2);color:#fca5a5}.dir-chg{background:rgba(34,197,94,.2);color:#86efac}.dir-idle{background:rgba(148,163,184,.2);color:#94a3b8}
#tte_display{font-size:1rem;font-weight:300;color:#cbd5e1;margin-bottom:1.5rem}
#hw_int{background:rgba(34,197,94,.2);color:#86efac;padding:.6rem;border-radius:8px;font-size:.85rem;font-weight:500;margin-bottom:1.2rem;border:1px solid rgba(34,197,94,.3);transition:all .3s}
.stats-row{display:flex;justify-content:center;gap:2rem;margin-bottom:1.5rem}
.stat-item{text-align:center}.stat-label{font-size:.65rem;color:#94a3b8;text-transform:uppercase;letter-spacing:1.5px;margin-bottom:.3rem}.stat-value{font-size:1.2rem;font-weight:700}
.fet-container{display:flex;justify-content:center;gap:2rem;padding-top:1.2rem;border-top:1px solid rgba(255,255,255,.1)}
.fet-item{display:flex;align-items:center;gap:.5rem;font-size:.85rem;color:#cbd5e1;font-weight:500}
.led{width:12px;height:12px;border-radius:50%;transition:all .3s}
.led-g{background:#2ecc71;box-shadow:0 0 10px #2ecc71}.led-b{background:#3b82f6;box-shadow:0 0 10px #3b82f6}.led-off{background:#475569}
.sub-card{background:rgba(255,255,255,.03);border-radius:16px;padding:1.2rem 1.5rem;width:100%;max-width:420px;border:1px solid rgba(255,255,255,.05);margin-bottom:1.5rem}
.card-title{font-size:.75rem;color:#94a3b8;text-transform:uppercase;letter-spacing:1px;margin-bottom:1rem;font-weight:500}
.temp-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:.8rem}
.temp-item{text-align:center;background:rgba(255,255,255,.03);padding:.8rem .5rem;border-radius:12px;border:1px solid rgba(255,255,255,.05)}
.temp-icon{font-size:1.2rem;margin-bottom:.3rem}.temp-val{font-size:1.1rem;font-weight:700}.temp-name{font-size:.65rem;color:#94a3b8;text-transform:uppercase;letter-spacing:1px;margin-top:.2rem}
.perf-row{display:flex;justify-content:space-between;margin-bottom:.4rem;font-size:.82rem;color:#cbd5e1}
.perf-val{font-weight:700}
.chart-box{background:rgba(0,0,0,.25);border-radius:10px;padding:6px;margin-top:.8rem}
#chartPerf{width:100%;height:70px;display:block}
.chart-legend{display:flex;justify-content:center;gap:1.2rem;margin-top:.5rem;font-size:.65rem;color:#64748b}
.legend-item{display:flex;align-items:center;gap:4px}.legend-dot{width:8px;height:8px;border-radius:50%}
/* Advanced Settings */
.adv-toggle{background:none;border:1px solid rgba(255,255,255,.1);color:#94a3b8;padding:.6rem 1.2rem;border-radius:12px;cursor:pointer;font-family:inherit;font-size:.8rem;letter-spacing:1px;text-transform:uppercase;width:100%;max-width:420px;margin-bottom:1rem;transition:all .3s}
.adv-toggle:hover{border-color:rgba(255,255,255,.3);color:#cbd5e1}
#adv_panel{display:none;width:100%;max-width:420px}
.adv-row{display:flex;justify-content:space-between;align-items:center;padding:.8rem 0;border-bottom:1px solid rgba(255,255,255,.05);font-size:.85rem;color:#cbd5e1}
.adv-row:last-child{border-bottom:none}
.adv-btn{background:rgba(255,255,255,.08);border:1px solid rgba(255,255,255,.1);color:#cbd5e1;padding:.4rem .8rem;border-radius:8px;cursor:pointer;font-family:inherit;font-size:.75rem;transition:all .2s}
.adv-btn:hover{background:rgba(255,255,255,.15)}
.adv-btn.danger{border-color:rgba(239,68,68,.3);color:#fca5a5}
.adv-btn.danger:hover{background:rgba(239,68,68,.15)}
/* Reboot Overlay */
#reboot_overlay{display:none;position:fixed;inset:0;background:rgba(15,23,42,.95);z-index:100;flex-direction:column;align-items:center;justify-content:center;backdrop-filter:blur(10px)}
#reboot_overlay.show{display:flex}
.reboot-spinner{width:48px;height:48px;border:3px solid rgba(255,255,255,.1);border-top-color:#3b82f6;border-radius:50%;animation:spin 1s linear infinite;margin-bottom:1.5rem}
@keyframes spin{to{transform:rotate(360deg)}}
.reboot-title{font-size:1.4rem;font-weight:700;margin-bottom:.5rem}
.reboot-sub{color:#94a3b8;font-size:.9rem;margin-bottom:1rem}
#reboot_countdown{font-size:2rem;font-weight:700;color:#3b82f6}
</style>
</head>
<body>
<div id="fault_banner"></div>

<div class="glass-card">
 <div class="progress-circle">
  <svg class="ring" viewBox="0 0 200 200"><circle class="bg" cx="100" cy="100" r="90"/><circle id="soc_ring" class="progress" cx="100" cy="100" r="90"/></svg>
  <div class="soc-content"><div id="soc_text">--%</div><div class="soc-label">Battery Level</div></div>
 </div>
 <div class="current-display"><span id="current_text">0.00</span><span class="current-unit">A</span><span id="current_dir" class="current-dir dir-idle">IDLE</span></div>
 <div id="tte_display">Status: Syncing...</div>
 <div class="stats-row">
  <div class="stat-item"><div class="stat-label">State of Health</div><div class="stat-value" id="soh_text" style="color:#8b5cf6">--%</div></div>
  <div class="stat-item"><div class="stat-label">Pack Voltage</div><div class="stat-value" id="pack_v" style="color:#38bdf8">--V</div></div>
 </div>
 <div class="fet-container">
  <div class="fet-item"><div id="chg_led" class="led led-off"></div>CHG FET</div>
  <div class="fet-item"><div id="dsg_led" class="led led-off"></div>DSG FET</div>
 </div>
</div>

<div class="sub-card">
 <div class="card-title">Thermal Monitor</div>
 <div class="temp-grid">
  <div class="temp-item"><div class="temp-icon">🔋</div><div class="temp-val" id="t_cell" style="color:#2ecc71">--°C</div><div class="temp-name">Cells</div></div>
  <div class="temp-item"><div class="temp-icon">⚡</div><div class="temp-val" id="t_sys" style="color:#f1c40f">--°C</div><div class="temp-name">System</div></div>
  <div class="temp-item"><div class="temp-icon">🛡️</div><div class="temp-val" id="t_die" style="color:#38bdf8">--°C</div><div class="temp-name">Die</div></div>
 </div>
</div>

<div class="sub-card">
 <div class="card-title">System Controller Performance</div>
 <div class="perf-row"><span>Core Logic (EKF)</span><span class="perf-val" id="pEKF" style="color:#2ecc71">-- ms</span></div>
 <div class="perf-row"><span>Hardware Polling (I²C)</span><span class="perf-val" id="pI2C" style="color:#f1c40f">-- ms</span></div>
 <div class="perf-row"><span>Telemetry API</span><span class="perf-val" id="pWeb" style="color:#3b82f6">-- ms</span></div>
 <div class="chart-box"><svg id="chartPerf" viewBox="0 0 400 70"></svg></div>
 <div class="chart-legend">
  <div class="legend-item"><div class="legend-dot" style="background:#2ecc71"></div>EKF</div>
  <div class="legend-item"><div class="legend-dot" style="background:#f1c40f"></div>I²C</div>
  <div class="legend-item"><div class="legend-dot" style="background:#3b82f6"></div>API</div>
 </div>
</div>

<button class="adv-toggle" onclick="document.getElementById('cell_panel').style.display=this.dataset.open?'none':'block';this.dataset.open=this.dataset.open?'':'1';this.innerText=this.dataset.open?'▲ Hide Cell Diagnostics':'▼ Cell Diagnostics'">▼ Cell Diagnostics</button>
<div id="cell_panel" class="sub-card" style="display:none;">
 <div style="display:flex;justify-content:space-between;margin-bottom:.5rem;">
  <div>Max: <span id="cell_max" style="color:#2ecc71;font-weight:600;">--mV</span></div>
  <div>Min: <span id="cell_min" style="color:#ef4444;font-weight:600;">--mV</span></div>
  <div>Delta: <span id="cell_delta_main" style="color:#f1c40f;font-weight:600;">--mV</span></div>
 </div>
 <div id="cell_grid" style="display:grid;grid-template-columns:repeat(auto-fill,minmax(60px,1fr));gap:6px;font-size:0.8rem;text-align:center;">
  <!-- Cells injected via JS -->
 </div>
</div>

<button class="adv-toggle" onclick="document.getElementById('adv_panel').style.display=this.dataset.open?'none':'block';this.dataset.open=this.dataset.open?'':'1';this.innerText=this.dataset.open?'▲ Hide Advanced Settings':'▼ Advanced Settings'" style="margin-top:10px;">▼ Advanced Settings</button>
<div id="adv_panel" class="sub-card" style="display:none;">
 <div class="card-title">Cell Balancing</div>
 <div class="adv-row"><span>Mode</span><span id="bal_mode_text" style="color:#8b5cf6">--</span></div>
 <div class="adv-row" style="align-items:center;">
  <span>Master Control</span>
  <button id="btn_toggle_master" class="adv-btn" onclick="sendCmd('toggleBalMaster')" style="width:auto;margin:0;padding:4px 12px;">--</button>
 </div>
 <div class="adv-row"><span>Cell Delta</span><span id="bal_delta_text" style="font-family:monospace">-- mV</span></div>
 <div class="adv-row"><span>Active Mask</span><span id="bal_mask_text" style="font-family:monospace">0x0000</span></div>
 <div id="host_bal_params">
  <div class="adv-row" style="margin-top:.5rem">
   <span>Start Trigger (mV)</span>
   <input type="number" id="in_bal_trig" style="width:70px;background:rgba(0,0,0,.3);border:1px solid rgba(255,255,255,.2);color:#fff;border-radius:6px;padding:4px;text-align:center;">
  </div>
  <div class="adv-row">
   <span>Delta Trigger (mV)</span>
   <input type="number" id="in_bal_delta" style="width:70px;background:rgba(0,0,0,.3);border:1px solid rgba(255,255,255,.2);color:#fff;border-radius:6px;padding:4px;text-align:center;">
  </div>
  <div style="display:flex;justify-content:flex-end;margin-top:.5rem;margin-bottom:.5rem;">
   <button class="adv-btn" onclick="saveBalConfig()" style="background:rgba(46,204,113,.15);border-color:#2ecc71;color:#86efac;width:100%">Save Config</button>
  </div>
 </div>
 <div style="display:flex;gap:.8rem;margin-top:1rem">
  <button class="adv-btn" onclick="sendCmd('toggleBal')" style="width:100%">Toggle Algorithm Mode</button>
 </div>
 <div class="card-title" style="margin-top:1.5rem">System</div>
 <div style="display:flex;gap:.8rem">
  <button class="adv-btn" onclick="sendCmd('clearFaults')">Clear Faults</button>
  <button class="adv-btn danger" onclick="confirmReset()">Reset System</button>
 </div>
</div>

<div id="reboot_overlay">
 <div class="reboot-spinner"></div>
 <div class="reboot-title">System Rebooting</div>
 <div class="reboot-sub">Please wait while the controller restarts...</div>
 <div id="reboot_countdown">5</div>
</div>

<script>
let hE=[],hI=[],hW=[],MX=60;
function drawChart(id,lines,h){
 let svg=document.getElementById(id);if(!svg)return;
 let all=lines.reduce((a,l)=>a.concat(l.d),[]);if(!all.length)return;
 let mx=Math.max(5,...all),w=400,p=4,pw=w-p*2,ph=h-p*2;
 let toX=(i,n)=>p+(i/Math.max(n-1,1))*pw, toY=v=>p+ph-(v/mx)*ph, s='';
 lines.forEach(l=>{if(l.d.length<2)return;let pts=l.d.map((v,i)=>`${toX(i,l.d.length)},${toY(v)}`).join(' ');
  let lx=toX(l.d.length-1,l.d.length),fx=toX(0,l.d.length);
  s+=`<polygon points="${pts} ${lx},${p+ph} ${fx},${p+ph}" fill="${l.c}" fill-opacity="0.08"/>`;
  s+=`<polyline points="${pts}" fill="none" stroke="${l.c}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" opacity="0.9"/>`;
 });svg.innerHTML=s;
}
function updateUI(d){
 let soc=d.soc||0;
 document.getElementById('soc_text').innerText=soc.toFixed(1)+"%";
 let c=soc>50?"#2ecc71":soc>20?"#f1c40f":"#ef4444";
 let ring=document.getElementById('soc_ring');ring.style.stroke=c;
 ring.style.strokeDashoffset=565.48-(soc/100)*565.48;

 let mA=d.current||0, aA=Math.abs(mA/1000);
 document.getElementById('current_text').innerText=aA.toFixed(2);
 let dir=document.getElementById('current_dir'),ct=document.getElementById('current_text');
 if(d.bq_dsg === "true"){dir.innerText='DISCHARGING';dir.className='current-dir dir-dsg';ct.style.color='#fca5a5'}
 else if(d.bq_chg === "true"){dir.innerText='CHARGING';dir.className='current-dir dir-chg';ct.style.color='#86efac'}
 else{dir.innerText='IDLE';dir.className='current-dir dir-idle';ct.style.color='#94a3b8'}

 if(d.tte<900){let h=Math.floor(d.tte/60),m=Math.floor(d.tte%60);document.getElementById('tte_display').innerText=`Time Remaining: ${h}h ${m}m`}
 else document.getElementById('tte_display').innerText="Status: Resting / Fully Charged";

 document.getElementById('soh_text').innerText=d.soh+"%";
 if(d.vPack!==undefined)document.getElementById('pack_v').innerText=(d.vPack/100).toFixed(1)+"V";

 document.getElementById('cell_max').innerText=(d.maxV||0)+"mV";
 document.getElementById('cell_min').innerText=(d.minV||0)+"mV";
 document.getElementById('cell_delta_main').innerText=((d.maxV||0)-(d.minV||0))+"mV";
 if(d.cells && Array.isArray(d.cells)) {
   let cg=document.getElementById('cell_grid');
   let h="";
   d.cells.forEach((cv,i)=>{
     let c="rgba(255,255,255,.1)", tc="#cbd5e1";
     if(cv===d.maxV && cv>0) { c="rgba(46,204,113,.2)"; tc="#86efac"; }
     else if(cv===d.minV && cv>0) { c="rgba(239,68,68,.2)"; tc="#fca5a5"; }
     h+=`<div style="background:${c};padding:4px;border-radius:4px;border:1px solid rgba(255,255,255,.05);">C${i+1}<br><b style="color:${tc}">${cv}</b></div>`;
   });
   cg.innerHTML=h;
 }

 document.getElementById('chg_led').className=d.fet_chg?"led led-g":"led led-off";
 document.getElementById('dsg_led').className=d.fet_dsg?"led led-b":"led led-off";

 if(d.t_cell!==undefined){let e=document.getElementById('t_cell');e.innerText=d.t_cell.toFixed(1)+"°C";e.style.color=d.t_cell>45?"#ef4444":"#2ecc71"}
 if(d.t_sys!==undefined){let e=document.getElementById('t_sys');e.innerText=d.t_sys.toFixed(1)+"°C";e.style.color=d.t_sys>50?"#ef4444":"#f1c40f"}
 if(d.t_die!==undefined){let e=document.getElementById('t_die');e.innerText=d.t_die.toFixed(1)+"°C";e.style.color=d.t_die>80?"#ef4444":d.t_die>60?"#f1c40f":"#38bdf8"}

 let fb=document.getElementById('fault_banner');
 // 0x38 = Masks OCD1 (0x08), OCD2 (0x10), and SCD (0x20)
 if(d.fault_code&0x38){fb.innerText="⚡ Output Paused: Heavy Load or Short Detected";fb.style.display="block"}
 // 0x01 = CUV (Cell Undervoltage)
 else if(d.fault_code&0x01){fb.innerText="🔋 Battery Depleted. Plug in charger.";fb.style.display="block"}
 // 0x02 = COV (Cell Overvoltage)
 else if(d.fault_code&0x02){fb.innerText="⚠️ Battery Overcharged. Disconnect Charger.";fb.style.display="block"}
 else fb.style.display="none";

 let ek=d.p_ekf||0,ic=d.p_i2c||0,wb=d.p_web||0;
 document.getElementById('pEKF').innerText=ek.toFixed(1)+" ms";
 document.getElementById('pI2C').innerText=ic.toFixed(1)+" ms";
 document.getElementById('pWeb').innerText=wb.toFixed(1)+" ms";
 hE.push(ek);if(hE.length>MX)hE.shift();hI.push(ic);if(hI.length>MX)hI.shift();hW.push(wb);if(hW.length>MX)hW.shift();
 drawChart('chartPerf',[{c:'#2ecc71',d:hE},{c:'#f1c40f',d:hI},{c:'#3b82f6',d:hW}],70);

 // Balancing
 document.getElementById('bal_mode_text').innerText=d.balMode===0?"Autonomous":"Host-Controlled";
 let btm=document.getElementById('btn_toggle_master');
 btm.innerText=d.balMaster?"Disable CB":"Enable CB";
 btm.style.borderColor=d.balMaster?"#ef4444":"#2ecc71";
 btm.style.color=d.balMaster?"#fca5a5":"#86efac";
 btm.style.background=d.balMaster?"rgba(239,68,68,.15)":"rgba(46,204,113,.15)";
 document.getElementById('bal_delta_text').innerText=(d.cellDelta||0)+" mV";
 document.getElementById('bal_mask_text').innerText="0x"+((d.bal||0).toString(16)).toUpperCase().padStart(4,'0');
 
 let paramsDiv=document.getElementById('host_bal_params');
 if(d.balMode===0) { paramsDiv.style.display='none'; } else { paramsDiv.style.display='block'; }
 
 let iTrig=document.getElementById('in_bal_trig'), iDelta=document.getElementById('in_bal_delta');
 if(document.activeElement!==iTrig) iTrig.value=d.balTrig||3400;
 if(document.activeElement!==iDelta) iDelta.value=d.balDelta||5;
}
let pollTimer=null;
function fetchData(){fetch('/api/data').then(r=>r.json()).then(d=>updateUI(d)).catch(e=>console.error("API",e))}
pollTimer=setInterval(fetchData,1000);

function sendCmd(action){
 fetch('/api/cmd?action='+action).then(r=>r.json()).then(d=>{
  if(d.message)showToast(d.message);
 }).catch(e=>console.error(e));
}
function saveBalConfig(){
 let t=document.getElementById('in_bal_trig').value;
 let d=document.getElementById('in_bal_delta').value;
 fetch('/api/cmd?action=setHostBalParams&trigger='+t+'&delta='+d).then(r=>r.json()).then(resp=>{
  if(resp.message)showToast(resp.message);
 }).catch(e=>console.error(e));
}
function confirmReset(){
 if(!confirm("Reset the BMS controller? The system will reboot."))return;
 clearInterval(pollTimer);
 let ov=document.getElementById('reboot_overlay');ov.classList.add('show');
 let cd=document.getElementById('reboot_countdown'),sec=6;
 fetch('/api/cmd?action=reset').catch(()=>{});
 let t=setInterval(()=>{sec--;cd.innerText=sec;if(sec<=0){clearInterval(t);tryReconnect()}},1000);
}
function tryReconnect(){
 document.getElementById('reboot_countdown').innerText="...";
 let att=0,mx=20;
 let iv=setInterval(()=>{att++;
  fetch('/api/data').then(r=>{if(r.ok){clearInterval(iv);document.getElementById('reboot_overlay').classList.remove('show');pollTimer=setInterval(fetchData,1000)}})
  .catch(()=>{if(att>=mx){clearInterval(iv);document.getElementById('reboot_countdown').innerText="Failed. Refresh page."}});
 },1500);
}
function showToast(msg){
 let t=document.createElement('div');
 t.style.cssText='position:fixed;bottom:2rem;left:50%;transform:translateX(-50%);background:rgba(30,41,59,.95);color:#cbd5e1;padding:1rem 1.5rem;border-radius:12px;border:1px solid rgba(255,255,255,.1);font-size:.85rem;z-index:50;max-width:350px;text-align:center;backdrop-filter:blur(10px);animation:fadeIn .3s';
 t.innerText=msg;document.body.appendChild(t);setTimeout(()=>{t.style.opacity='0';t.style.transition='opacity .3s';setTimeout(()=>t.remove(),300)},4000);
}
</script>
</body>
</html>

)rawliteral";

// ==================== WEB HANDLERS ====================
void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleApiData() {
  String json;
  json.reserve(768);
  json += "{";
  // The Unified SOC
  json += "\"soc\":" + String(soc_display, 1) + ",";
  // The Time to Empty
  json += "\"tte\":" + String((int)time_to_empty_mins) + ",";
  // The Rounded SOH
  json += "\"soh\":" + String((int)sohEngine.getSOH()) + ",";
  
  // Calculate TOSF Delta
  long sumCells = 0;
  for (int i = 0; i < TB_CONNECTED_CELLS; i++) {
    sumCells += cellVoltages[i]; // in mV
  }
  long stack_mV = vStack * 10; // vStack is in 10mV units, measured inside the FETs!
  json += "\"tosf\":" + String(abs(stack_mV - sumCells)) + ",";

  // Read-Only FET Status (Extracted from 0x7F Register)
  uint16_t fetStatus = lastFetStat; // Cached in readBMSData
  json += "\"fet_chg\":" + String((fetStatus & 0x01) ? "true" : "false") + ",";
  json += "\"fet_dsg\":" + String((fetStatus & 0x04) ? "true" : "false") + ",";
  
  // BQ76952 Process Indicators (from 0x12 Battery Status)
  json += "\"bq_chg\":" + String((global_batStat & 0x02) ? "true" : "false") + ",";
  json += "\"bq_dsg\":" + String((global_batStat & 0x01) ? "true" : "false") + ",";

  // BQ76952 Protection Faults (Human Readable Translation triggers)
  json += "\"fault_code\":" + String(ssA_val) + ",";

  // Current (mA)
  json += "\"current\":" + String(bmsCurrent) + ",";

  // Pack Voltage (10mV units)
  json += "\"vPack\":" + String(vPack) + ",";

  // Detailed Cell Voltages
  json += "\"minV\":" + String(minCellV) + ",";
  json += "\"maxV\":" + String(maxCellV) + ",";
  json += "\"cells\":[";
  for(int i=0; i<TB_CONNECTED_CELLS; i++) {
    json += String(cellVoltages[i]);
    if(i < TB_CONNECTED_CELLS - 1) json += ",";
  }
  json += "],";

  // Temperatures
  // Cell Temp: average of valid external thermistors (temp2=HDQ, temp3=TS3)
  float t_cell_avg = 0.0f;
  int t_count = 0;
  if (temp2 > -40.0f && temp2 < 100.0f) { t_cell_avg += temp2; t_count++; }
  if (temp3 > -40.0f && temp3 < 100.0f) { t_cell_avg += temp3; t_count++; }
  if (t_count > 0) t_cell_avg /= t_count;
  json += "\"t_cell\":" + String(t_cell_avg, 1) + ",";
  // System Temp: HDQ thermistor (closest to FETs/board)
  json += "\"t_sys\":" + String(temp2, 1) + ",";
  // Die Temp: BQ76952 internal (used for protection)
  json += "\"t_die\":" + String(chipTemp, 1) + ",";

  // Balancing State
  json += "\"balMode\":" + String(currentBalMode) + ",";
  json += "\"balMaster\":" + String(balancingEnabled ? 1 : 0) + ",";
  json += "\"bal\":" + String(balancingMask) + ",";
  json += "\"cellDelta\":" + String(cellBalancingDelta) + ",";
  json += "\"balTrig\":" + String(hostBalTriggerMv) + ",";
  json += "\"balDelta\":" + String(hostBalDeltaMv) + ",";

  // Performance Monitor
  json += "\"p_i2c\":" + String(perf_i2c_us / 1000.0f, 1) + ",";
  json += "\"p_ekf\":" + String(perf_ekf_us / 1000.0f, 1) + ",";
  json += "\"p_web\":" + String(perf_web_us / 1000.0f, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void handleApiLog() {
  String json = "[";
  int count = min(logCount, 10);
  int idx = (logHead - count + LOG_SIZE) % LOG_SIZE;
  for (int i = 0; i < count; i++) {
    int pos = (idx + i) % LOG_SIZE;
    if (i > 0)
      json += ",";
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

void handleApiCmd() {
  String action = server.arg("action");
  if (action == "reset") {
    Serial.println("=== FULL SYSTEM RESET INITIATED ===");
    bms.reset();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"System rebooting... Please wait 5 seconds.\"}");
    delay(1000);
    ESP.restart();
  } else if (action == "toggleBal") {
    int newMode = (currentBalMode == BAL_MODE_AUTONOMOUS) ? BAL_MODE_HOST_ALGO : BAL_MODE_AUTONOMOUS;
    prefs.putInt("bal_mode", newMode);
    Serial.printf("[UI] Balancing mode saved to NVS: %d. Pending restart.\n", newMode);
    String resp = "{\"status\":\"ok\",\"message\":\"Balancing mode updated! Please click Reset System to apply.\"}";
    server.send(200, "application/json", resp);
    return;
  } else if (action == "toggleBalMaster") {
    balancingEnabled = !balancingEnabled;
    prefs.putBool("bal_master", balancingEnabled);
    Serial.printf("[UI] Master Balancing saved to NVS: %d.\n", balancingEnabled);
    if (!balancingEnabled) {
      if (powerMode == 1) { wakeBms(); delay(10); }
      bms.setBalancingMask(0);
    }
    String resp;
    if (currentBalMode == BAL_MODE_AUTONOMOUS) {
      resp = "{\"status\":\"ok\",\"message\":\"Master Balancing saved! Click Reset System to apply in Autonomous mode.\"}";
    } else {
      resp = "{\"status\":\"ok\",\"message\":\"Master Balancing updated instantly!\"}";
    }
    server.send(200, "application/json", resp);
    return;
  } else if (action == "setHostBalParams") {
    if (server.hasArg("trigger")) {
      hostBalTriggerMv = server.arg("trigger").toInt();
      prefs.putUShort("trigger", hostBalTriggerMv);
    }
    if (server.hasArg("delta")) {
      hostBalDeltaMv = server.arg("delta").toInt();
      prefs.putUShort("delta", hostBalDeltaMv);
    }
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Host Balancing Params Updated!\"}");
    return;
  } else if (action == "clearFaults") {
    Serial.println("=== MANUAL FAULT CLEAR ===");
    clearBmsAlarms();
    watchdogFaultLocked = false;
    addLog(0x62, 0x00FF, true, true);
    Serial.println("[WATCHDOG] Faults cleared. ALERT LED unlocked.");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Unknown command.\"}");
    return;
  }
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleApiEKFReset() {
  if (server.method() != HTTP_POST && server.method() != HTTP_GET) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  float V_cell = (float)cellVoltages[1] / 1000.0f;
  float I_current = (float)bmsCurrent / 1000.0f;
  float soc_est = smartSOCInit(V_cell, I_current);

  initial_ekf_soc = soc_est;
  software_charge_Ah = 0.0f; // Reset our robust software CC!
  bms.ResetAccumulatedCharge();
  last_cc_reset_time = millis();
  ekf.begin(soc_est);

  String response = "{\"status\":\"ok\",\"new_soc\":" + String(soc_est) + "}";
  server.send(200, "application/json", response);

  Serial.printf("[EKF] Manual Reset to %.1f%%\n", soc_est);
}
