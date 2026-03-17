#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "BQ76952.h"
#include "config.h"

// ==================== SENDER-SPECIFIC CONFIG ====================
const int FALLBACK_CHANNEL = 6;
const int I2C_SDA_PIN = 21; // Standard I2C SDA on ESP32 DevKit V1
const int I2C_SCL_PIN = 22; // Standard I2C SCL on ESP32 DevKit V1

DeviceMessage outgoingData;
BQ76952 bms;

void collectBMSData()
{
  for (int i = 0; i < 16; i++)
  {
    outgoingData.v[i] = (i < CONNECTED_CELLS) ? bms.getCellVoltage(i + 1) : 0;
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
  strncpy(outgoingData.message, "BMS_ACTIVE", sizeof(outgoingData.message));
}

void collectfakeBMSData()
{ // for testing purposes ***
  for (int i = 0; i < 16; i++)
  {
    if (i < CONNECTED_CELLS)
    {
      outgoingData.v[i] = random(3000, 4201);
    }
    else
    {
      outgoingData.v[i] = 0;
    }
  }
  outgoingData.v_stack = 0;
  for (int i = 0; i < CONNECTED_CELLS; i++)
  {
    outgoingData.v_stack += outgoingData.v[i];
  }
  outgoingData.v_pack = outgoingData.v_stack - random(50, 200);
  outgoingData.current = random(-500, 501);
  outgoingData.charge = (float)random(1000, 50000) / 10.0;
  outgoingData.charge_time += 1;
  outgoingData.chip_temp = (float)random(200, 451) / 10.0;
  outgoingData.temp1 = (float)random(200, 451) / 10.0;
  outgoingData.temp2 = (float)random(200, 451) / 10.0;
  outgoingData.temp3 = (float)random(200, 451) / 10.0;
  outgoingData.isCharging = random(0, 2);
  outgoingData.isDischarging = !outgoingData.isCharging;
  strncpy(outgoingData.message, "FAKE_DATA_TEST", sizeof(outgoingData.message));
}

int scanForChannel()
{
  Serial.println("[WiFi] Scanning for AP to detect channel...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++)
  {
    if (String(WiFi.SSID(i)) == WIFI_SSID)
    {
      int ch = WiFi.channel(i);
      Serial.printf("[WiFi] Found AP '%s' on channel %d\n", WIFI_SSID, ch);
      WiFi.scanDelete();
      return ch;
    }
  }
  WiFi.scanDelete();
  Serial.printf("[WiFi] AP '%s' not found, using fallback channel %d\n", WIFI_SSID, FALLBACK_CHANNEL);
  return FALLBACK_CHANNEL;
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("\r[ESP-NOW] Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "✓ SUCCESS" : "✗ FAILED");
}

void setup()
{
  Serial.begin(115200);
  // BMS init — talks to real BQ76952 (or tb_bq_node simulator) over I2C
  bms.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  bms.reset();
  delay(100);
  bms.setConnectedCells(CONNECTED_CELLS);
  // bms.writeByteToMemory(DA_Configuration, 0x06); // uncomment for real BQ76952

  WiFi.mode(WIFI_STA);
  Serial.printf("[INFO] Sender MAC: %s\n", WiFi.macAddress().c_str());
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
  collectBMSData(); // reads from BQ76952 (or tb_bq_node simulator) over I2C
  // collectfakeBMSData(); // swap back for testing without I2C hardware

  // Debug: print I2C readings
  Serial.printf("[I2C] C1=%u C2=%u C3=%u Stack=%u Cur=%d Temp=%.1f\n",
                outgoingData.v[0], outgoingData.v[1], outgoingData.v[2],
                outgoingData.v_stack, outgoingData.current, outgoingData.chip_temp);

  esp_now_send(RECEIVER_ADDRESS, (uint8_t *)&outgoingData, sizeof(outgoingData));

  delay(500);
}
