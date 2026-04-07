#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stddef.h>

// ==================== NETWORK CONFIG ====================
const char *const WIFI_SSID = "Philo";
const char *const WIFI_PASSWORD = "1234567891";

const uint32_t WIFI_TIMEOUT_MS = 15000;
const uint32_t WIFI_RETRY_INTERVAL_MS = 10000;

// ==================== MQTT CONFIG ====================
const char *const MQTT_BROKER = "172.20.10.2";
const uint16_t MQTT_PORT = 1883;
const char *const MQTT_TOPIC = "bms/data";
const char *const MQTT_CLIENT_ID = "wbms-master";
const uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000;

// ==================== BMS CONFIG ====================
#define CONNECTED_CELLS 3

const uint8_t RECEIVER_ADDRESS[] = {0x3C, 0x8A, 0x1F, 0x0C, 0xD5, 0x28};

const uint8_t SENDER_ADDRESSES[][6] = {
    {0x08, 0xD1, 0xF9, 0x27, 0xAF, 0x28},
};
const int NUM_SENDERS = sizeof(SENDER_ADDRESSES) / sizeof(SENDER_ADDRESSES[0]);

// ==================== DATA STRUCTURE ====================
typedef struct
{
  unsigned int v[16];
  unsigned int v_stack;
  unsigned int v_pack;
  int current;
  float chip_temp;
  float temp1;
  float temp2;
  float temp3;
  float charge;
  uint32_t charge_time;
  bool isCharging;
  bool isDischarging;
  char message[50];
} __attribute__((packed)) DeviceMessage;

#endif