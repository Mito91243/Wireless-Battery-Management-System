#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "BQ76952.h"
#include "config.h"

DeviceMessage outgoingData;
BQ76952 bms;

// ==================== PAIRING CODE ====================
char pairingCode[7] = "000000"; // Last 3 bytes of MAC as hex (e.g. "27AF28")

// ==================== HEARTBEAT / FALLBACK AP ====================
volatile int consecutiveFailures = 0;
portMUX_TYPE failureMux = portMUX_INITIALIZER_UNLOCKED;
bool fallbackAPActive = false;

void startFallbackAP()
{
  if (fallbackAPActive)
    return;
  Serial.println("[Heartbeat] Master unreachable -- starting fallback AP");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(FALLBACK_AP_SSID, FALLBACK_AP_PASS);
  fallbackAPActive = true;
  Serial.printf("[Heartbeat] AP '%s' active at %s\n",
                FALLBACK_AP_SSID, WiFi.softAPIP().toString().c_str());
}

void stopFallbackAP()
{
  if (!fallbackAPActive)
    return;
  Serial.println("[Heartbeat] Master reachable again -- stopping fallback AP");
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  fallbackAPActive = false;
}

void collectBMSData()
{
  for (int i = 0; i < 16; i++)
  {
    if (i < CONNECTED_CELLS) {
      if (CONNECTED_CELLS < 16 && i == CONNECTED_CELLS - 1) {
        // Top cell wired to VC16 in reduced-cell configurations
        outgoingData.v[i] = bms.getCellVoltage(16);
      } else {
        // Sequential from VC1 (or all 16 when CONNECTED_CELLS == 16)
        outgoingData.v[i] = bms.getCellVoltage(i + 1);
      }
    } else {
      outgoingData.v[i] = 0;
    }
  }
  outgoingData.v_stack = bms.getCellVoltage(17);
  outgoingData.v_pack = bms.getCellVoltage(18);
  outgoingData.current = bms.getCurrent();
  outgoingData.charge = bms.getAccumulatedCharge();
  outgoingData.charge_time = bms.getAccumulatedChargeTime();
  outgoingData.chip_temp = bms.getInternalTemp();
  outgoingData.temp1 = bms.getThermistorTemp(TS1);
  outgoingData.temp2 = bms.getThermistorTemp(TS2);
  outgoingData.temp3 = bms.getThermistorTemp(TS3);
  outgoingData.isCharging = bms.isCharging();
  outgoingData.isDischarging = bms.isDischarging();
  snprintf(outgoingData.message, sizeof(outgoingData.message), "BMS:%s", pairingCode);
}


int scanForChannel()
{
  Serial.printf("[WiFi] Scanning for master AP '%s' to detect channel...\n", MASTER_AP_SSID);
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++)
  {
    if (String(WiFi.SSID(i)) == MASTER_AP_SSID)
    {
      int ch = WiFi.channel(i);
      Serial.printf("[WiFi] Found master AP '%s' on channel %d\n", MASTER_AP_SSID, ch);
      WiFi.scanDelete();
      return ch;
    }
  }
  WiFi.scanDelete();
  Serial.printf("[WiFi] Master AP '%s' not found, using fallback channel %d\n", MASTER_AP_SSID, FALLBACK_CHANNEL);
  return FALLBACK_CHANNEL;
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  portENTER_CRITICAL(&failureMux);
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    consecutiveFailures = 0;
  }
  else
  {
    consecutiveFailures++;
  }
  portEXIT_CRITICAL(&failureMux);

  Serial.print("\r[ESP-NOW] Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAILED");
}

void setup()
{
  Serial.begin(115200);
  // BMS init — talks to real BQ76952 (or tb_bq_node simulator) over I2C
  bms.setDebug(true); // Enable verbose I2C logging for testing read/write
  bms.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  bms.reset();
  delay(100);
  // Build cell bitmask from CONNECTED_CELLS.
  // Wiring: lower cells on VC1..VC(N-1), top cell on VC16.
  // 16 cells = all channels, otherwise lower (N-1) bits + bit 15.
  uint16_t cellMask = (CONNECTED_CELLS >= 16)
                          ? 0xFFFF
                          : (uint16_t)(((1 << (CONNECTED_CELLS - 1)) - 1) | 0x8000);
  bms.writeIntToMemory(0x9304, cellMask);
  bms.writeByteToMemory(DA_Configuration, 0x06); // Required for real BQ76952

  WiFi.mode(WIFI_STA);
  // Derive pairing code from last 3 bytes of MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(pairingCode, sizeof(pairingCode), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  Serial.printf("[INFO] Sender MAC: %s\n", WiFi.macAddress().c_str());
  Serial.printf("[INFO] Pairing Code: %s\n", pairingCode);
  int channel = scanForChannel();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("[ESP-NOW] ❌ Init Failed");
    ESP.restart();
  }
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, RECEIVER_ADDRESS, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop()
{
  collectBMSData();

  // Debug: print I2C readings
  Serial.printf("[I2C] C1=%u C2=%u C3=%u Stack=%u Cur=%d Temp=%.1f\n",
                outgoingData.v[0], outgoingData.v[1], outgoingData.v[2],
                outgoingData.v_stack, outgoingData.current, outgoingData.chip_temp);

  esp_now_send(RECEIVER_ADDRESS, (uint8_t *)&outgoingData, sizeof(outgoingData));

  // ── Heartbeat check ──
  int failures;
  portENTER_CRITICAL(&failureMux);
  failures = consecutiveFailures;
  portEXIT_CRITICAL(&failureMux);

  if (failures >= FAILURE_THRESHOLD)
  {
    startFallbackAP();
  }
  else if (failures == 0 && fallbackAPActive)
  {
    stopFallbackAP();
  }

  delay(SEND_INTERVAL_MS);
}
