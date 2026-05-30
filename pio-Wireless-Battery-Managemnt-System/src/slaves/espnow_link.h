#pragma once
// ==============================================================
// espnow_link.h — ESP-NOW client + online/offline mode switch
// ==============================================================
// This is the "merge glue" that turns the standalone dashboard into the
// production slave firmware described in guide.md Part 1:
//
//   MODE_ONLINE  : WiFi STA on the master's channel. Packs the live BMS
//                  reading into the 152-byte DeviceMessage and ships it to
//                  the master over ESP-NOW every SEND_INTERVAL_MS. Listens
//                  for the master's heartbeat to know the cloud is alive.
//
//   MODE_OFFLINE : master/cloud unreachable -> raise a local SoftAP, serve
//                  the full dashboard + a captive-portal redirect, and keep
//                  the EKF/SOH running. Periodically hop back to the master's
//                  channel to probe for recovery.
//
// MUST be #included AFTER the dashboard globals (cellVoltages[], vStack,
// vPack, cc1_raw, chipTemp, temp1/2/3, software_charge_Ah, chargeTime,
// isCharging, isDischarging) and the `server` WebServer object are defined,
// because packDeviceMessage() reads them directly.
// ==============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <DNSServer.h>
#include "config.h"     // DeviceMessage, MasterHeartbeat, MACs, timing (shared)
#include "tb_config.h"  // MB_AP_PASSWORD

// ---- external dashboard state we serialize for the cloud ----
extern unsigned int cellVoltages[16];
extern unsigned int vStack;
extern unsigned int vPack;
extern int cc1_raw;        // low-noise CC1 current, 1 mA units (clean for cloud EKF, issues B2)
extern float chipTemp;
extern float temp1, temp2, temp3; // temp2 = CFETOFF on this board (issues B4 decision)
extern float software_charge_Ah;  // BQ PASSQ accumulator in Ah
extern uint32_t chargeTime;
extern bool isCharging;
extern bool isDischarging;
extern WebServer server;

// ==================== LINK STATE ====================
enum LinkMode { LINK_ONLINE, LINK_OFFLINE };
LinkMode linkMode = LINK_ONLINE;

DeviceMessage espnowOut;
DNSServer dnsServer;

char pairingCode[7] = "000000";   // last 3 MAC bytes (e.g. "9A00FF")
char slaveApSsid[32] = "wBMS-Slave";

// Failure counter is written from the ESP-NOW send callback (WiFi task core),
// read from loop() — guard with a spinlock, same pattern as the master queue.
volatile int consecutiveFailures = 0;
portMUX_TYPE espnowMux = portMUX_INITIALIZER_UNLOCKED;

// Heartbeat state (written from the receive callback on the WiFi task).
volatile unsigned long lastHeartbeatMs = 0;
volatile bool          lastUplinkUp = false;
volatile bool          heartbeatEverSeen = false;
volatile uint8_t       masterChannel = FALLBACK_CHANNEL;

unsigned long onlineEnteredMs = 0;
unsigned long lastSendMs = 0;
unsigned long lastHopMs = 0;
int goodHops = 0;

// ==================== CHANNEL SCAN ====================
// Locate the master's discovery AP (MASTER_AP_SSID) and return its channel.
int scanForMasterChannel() {
  Serial.printf("[LINK] Scanning for master AP '%s'...\n", MASTER_AP_SSID);
  int n = WiFi.scanNetworks();
  int found = -1;
  for (int i = 0; i < n; i++) {
    if (String(WiFi.SSID(i)) == MASTER_AP_SSID) {
      found = WiFi.channel(i);
      break;
    }
  }
  WiFi.scanDelete();
  if (found >= 0) {
    Serial.printf("[LINK] Found master on channel %d\n", found);
    return found;
  }
  Serial.printf("[LINK] Master AP not found, using fallback channel %d\n", FALLBACK_CHANNEL);
  return FALLBACK_CHANNEL;
}

// ==================== ESP-NOW CALLBACKS ====================
void espnowOnSent(const uint8_t *mac, esp_now_send_status_t status) {
  portENTER_CRITICAL_ISR(&espnowMux);
  if (status == ESP_NOW_SEND_SUCCESS)
    consecutiveFailures = 0;
  else
    consecutiveFailures++;
  portEXIT_CRITICAL_ISR(&espnowMux);
}

// Master -> slave heartbeat. Carries the master's REAL uplink status, so the
// slave can detect "cloud down" even when the master node still ACKs (A3).
void espnowOnRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != (int)sizeof(MasterHeartbeat)) return;
  MasterHeartbeat hb;
  memcpy(&hb, data, sizeof(hb));
  if (hb.magic != MASTER_HB_MAGIC) return;

  lastHeartbeatMs = millis();
  lastUplinkUp = (hb.uplinkUp != 0);
  masterChannel = hb.channel;       // master tells us its channel -> cheap re-tune, no rescan
  heartbeatEverSeen = true;
}

// ==================== PEER MANAGEMENT ====================
void espnowSetPeerChannel(uint8_t ch) {
  esp_now_del_peer(RECEIVER_ADDRESS);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, RECEIVER_ADDRESS, 6);
  peer.channel = ch;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

// ==================== STRUCT PACKING ====================
// Map the rich dashboard reading down to the 15-ish fields that fit the
// 152-byte ESP-NOW struct (guide Part 3). The local dashboard still serves
// the full ~60-field JSON; only this subset goes to the cloud.
void packDeviceMessage() {
  for (int i = 0; i < 16; i++) espnowOut.v[i] = cellVoltages[i];
  espnowOut.v_stack = vStack;
  espnowOut.v_pack = vPack;
  espnowOut.current = cc1_raw;            // clean CC1 (1 mA units), no deadband -> better cloud SOC (B2)
  espnowOut.chip_temp = chipTemp;
  espnowOut.temp1 = temp1;
  espnowOut.temp2 = temp2;                // CFETOFF, aligned to dashboard (Q2 decision)
  espnowOut.temp3 = temp3;
  espnowOut.charge = software_charge_Ah;  // Ah (struct documents Ah; dashboard holds mAh)
  espnowOut.charge_time = chargeTime;
  espnowOut.isCharging = isCharging;
  espnowOut.isDischarging = isDischarging;
  snprintf(espnowOut.message, sizeof(espnowOut.message), "BMS:%s", pairingCode);
}

// ==================== MODE TRANSITIONS ====================
void enterOnline(uint8_t ch) {
  Serial.printf("[LINK] -> ONLINE (channel %d)\n", ch);
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  espnowSetPeerChannel(ch);
  portENTER_CRITICAL(&espnowMux);
  consecutiveFailures = 0;
  portEXIT_CRITICAL(&espnowMux);
  lastHeartbeatMs = millis();   // grace: don't immediately re-trip on stale heartbeat
  onlineEnteredMs = millis();
  lastSendMs = 0;
  linkMode = LINK_ONLINE;
}

void enterOffline() {
  Serial.println("[LINK] -> OFFLINE: raising local AP + dashboard");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(slaveApSsid, MB_AP_PASSWORD);
  // Captive-portal redirect: answer every DNS query with our own IP so the
  // phone's "sign in to network" probe opens the dashboard automatically.
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.printf("[LINK] AP '%s' (pw '%s') at http://%s\n",
                slaveApSsid, MB_AP_PASSWORD, WiFi.softAPIP().toString().c_str());
  linkMode = LINK_OFFLINE;
  lastHopMs = millis();
  goodHops = 0;
}

// ==================== SETUP ====================
void espnowSetup() {
  WiFi.mode(WIFI_STA);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(pairingCode, sizeof(pairingCode), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  snprintf(slaveApSsid, sizeof(slaveApSsid), "wBMS-Slave-%s", pairingCode); // unique per unit (issues C2)
  Serial.printf("[LINK] MAC %s  Pairing %s\n", WiFi.macAddress().c_str(), pairingCode);

  uint8_t ch = scanForMasterChannel();
  masterChannel = ch;
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[LINK] ESP-NOW init failed; restarting");
    ESP.restart();
  }
  esp_now_register_send_cb(espnowOnSent);
  esp_now_register_recv_cb(espnowOnRecv);
  espnowSetPeerChannel(ch);

  linkMode = LINK_ONLINE;
  onlineEnteredMs = millis();
  lastHeartbeatMs = millis();
}

// ==================== ONLINE SERVICE ====================
void serviceOnline() {
  // 1. Ship the live reading at the configured cadence.
  if (millis() - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = millis();
    packDeviceMessage();
    esp_now_send(RECEIVER_ADDRESS, (uint8_t *)&espnowOut, sizeof(espnowOut));
  }

  // 2. Decide whether to fall back. Give a short grace window after going
  //    online so a single stale value can't bounce us straight back out.
  if (millis() - onlineEnteredMs < 3000) return;

  int fails;
  portENTER_CRITICAL(&espnowMux);
  fails = consecutiveFailures;
  portEXIT_CRITICAL(&espnowMux);

  bool nodeDown = (fails >= FAILURE_THRESHOLD);
  // The heartbeat-based triggers only apply once we've actually heard the
  // master at least once — otherwise a master running OLD firmware (no
  // heartbeat) would falsely look "offline" forever. ACK failure still covers
  // that case.
  bool hbStale  = heartbeatEverSeen && (millis() - lastHeartbeatMs > HEARTBEAT_TIMEOUT_MS);
  bool cloudDown = heartbeatEverSeen && !hbStale && !lastUplinkUp;

  if (nodeDown || hbStale || cloudDown) {
    Serial.printf("[LINK] fallback trigger: nodeDown=%d hbStale=%d cloudDown=%d (fails=%d)\n",
                  nodeDown, hbStale, cloudDown, fails);
    enterOffline();
  }
}

// ==================== OFFLINE RECOVERY HOP ====================
// Briefly leave the AP, tune to the master's channel, send a probe and listen
// for a heartbeat. Requires RECOVERY_GOOD_HOPS consecutive good hops (with the
// cloud actually up) before tearing down the AP. The AP blinks for ~RECOVERY_
// LISTEN_MS each hop; the dashboard JS retries silently to hide it (A6).
void doRecoveryHop() {
  uint8_t ch = heartbeatEverSeen ? masterChannel : (uint8_t)scanForMasterChannel();

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  espnowSetPeerChannel(ch);

  unsigned long preHb = lastHeartbeatMs;
  packDeviceMessage();
  esp_now_send(RECEIVER_ADDRESS, (uint8_t *)&espnowOut, sizeof(espnowOut)); // probe

  unsigned long t0 = millis();
  while (millis() - t0 < RECOVERY_LISTEN_MS) delay(10); // recv cb runs on WiFi task

  bool heardFreshHb = (lastHeartbeatMs != preHb);
  bool recovered = heardFreshHb && lastUplinkUp; // node up AND cloud up (A3)

  if (recovered) {
    goodHops++;
    Serial.printf("[LINK] recovery hop OK (%d/%d) ch=%d\n", goodHops, RECOVERY_GOOD_HOPS, ch);
    if (goodHops >= RECOVERY_GOOD_HOPS) {
      enterOnline(ch);
      return;
    }
  } else {
    goodHops = 0;
  }

  // Not recovered yet -> go back to serving the AP.
  WiFi.mode(WIFI_AP);
  WiFi.softAP(slaveApSsid, MB_AP_PASSWORD);
  dnsServer.start(53, "*", WiFi.softAPIP());
  lastHopMs = millis();
}

// ==================== OFFLINE SERVICE ====================
void serviceOffline() {
  dnsServer.processNextRequest();
  server.handleClient();
  if (millis() - lastHopMs >= RECOVERY_HOP_INTERVAL_MS) {
    doRecoveryHop();
  }
}

// ==================== LOOP ENTRY ====================
void espnowLoop() {
  if (linkMode == LINK_ONLINE)
    serviceOnline();
  else
    serviceOffline();
}
