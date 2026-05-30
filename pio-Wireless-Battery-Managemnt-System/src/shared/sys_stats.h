#pragma once
// ==============================================================
// sys_stats.h — clean boot-time system/memory report (shared)
// ==============================================================
// Call printBootStats("Master") / printBootStats("Slave") once at the very
// start of setup() (after Serial.begin). Heap numbers are the cleanest baseline
// right at boot, before WiFi/web/EKF allocate. Left-aligned (no right border)
// so variable-width fields like the chip model never knock it out of shape.
// ==============================================================

#include <Arduino.h>

inline const char *resetReasonStr()
{
  switch (esp_reset_reason())
  {
    case ESP_RST_POWERON:   return "power-on / reset button";
    case ESP_RST_EXT:       return "external pin";
    case ESP_RST_SW:        return "software restart";
    case ESP_RST_PANIC:     return "crash (panic)";
    case ESP_RST_INT_WDT:   return "interrupt watchdog";
    case ESP_RST_TASK_WDT:  return "task watchdog";
    case ESP_RST_WDT:       return "other watchdog";
    case ESP_RST_DEEPSLEEP: return "deep-sleep wake";
    case ESP_RST_BROWNOUT:  return "brownout (low voltage)";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "unknown";
  }
}

inline void printBootStats(const char *role)
{
  uint32_t heap = ESP.getHeapSize();
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t psram = ESP.getPsramSize();

  Serial.println();
  Serial.printf("========== wBMS %s boot report ==========\n", role);
  Serial.printf("  Chip      : %s rev %d, %d core(s) @ %d MHz\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getChipCores(), ESP.getCpuFreqMHz());
  Serial.printf("  SDK       : %s\n", ESP.getSdkVersion());
  Serial.printf("  Flash     : %u KB\n", ESP.getFlashChipSize() / 1024);
  Serial.printf("  Sketch    : %u KB used, %u KB free (OTA)\n",
                ESP.getSketchSize() / 1024, ESP.getFreeSketchSpace() / 1024);
  Serial.printf("  Heap      : %u KB total, %u KB free (%u%% used)\n",
                heap / 1024, freeHeap / 1024,
                heap ? (unsigned)(100 - (freeHeap * 100ULL / heap)) : 0);
  Serial.printf("  Heap marks: %u KB min-free, %u KB largest block\n",
                ESP.getMinFreeHeap() / 1024, ESP.getMaxAllocHeap() / 1024);
  if (psram)
    Serial.printf("  PSRAM     : %u KB total, %u KB free\n",
                  psram / 1024, ESP.getFreePsram() / 1024);
  else
    Serial.println("  PSRAM     : none");
  Serial.printf("  Reset     : %s\n", resetReasonStr());
  Serial.println("==========================================");
  Serial.println();
}
