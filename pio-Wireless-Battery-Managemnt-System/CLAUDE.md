# Wireless Battery Management System (wBMS)

## Project Overview

An embedded IoT system that monitors a multi-cell lithium battery pack in real time using a **master-slave architecture** on ESP32 microcontrollers. A slave ESP32 reads cell data from a **BQ76952** BMS IC over I2C, transmits it wirelessly via **ESP-NOW** to a master ESP32, which relays it to the cloud via **MQTT**.

## Architecture

```
BQ76952 BMS IC ──(I2C)──► Slave ESP32 ──(ESP-NOW)──► Master ESP32 ──(MQTT/WiFi)──► Cloud Broker
  (16-cell monitor)         sender.cpp                  reciever.cpp            64.23.174.210:1883
```

### Data Flow (step by step)

1. **BQ76952 IC** continuously monitors up to 16 lithium cells (voltages, current, temperatures, charge state)
2. **Slave ESP32** (`sender.cpp`) reads BMS data over I2C every 500 ms, packs it into a `DeviceMessage` struct
3. Slave transmits the struct over **ESP-NOW** (peer-to-peer, MAC-addressed, ~250 byte payload)
4. **Master ESP32** (`reciever.cpp`) receives ESP-NOW data in an ISR-like callback, enqueues it into a thread-safe circular queue
5. Master's main loop dequeues messages, serializes to JSON, and publishes to MQTT topic `bms/data`
6. Cloud broker at `64.23.174.210:1883` receives the data for dashboarding/storage

### Node Roles

**Slave (sender.cpp):**
- Reads BMS cell voltages, current, temperatures, charge state from BQ76952 over I2C
- Discovers the master's WiFi channel by scanning for the configured SSID (`scanForChannel()`)
- Sends a packed `DeviceMessage` struct to the master via ESP-NOW every 500 ms
- Has a `collectfakeBMSData()` function for testing without hardware (**currently active** in `loop()`)
- Real BMS init code is commented out in `setup()` — uncomment when hardware is connected
- WiFi mode: `WIFI_STA` (station only, does NOT connect to WiFi — only scans for channel)

**Master (reciever.cpp):**
- Connects to WiFi in `WIFI_AP_STA` mode (dual mode: WiFi client + ESP-NOW receiver)
- Receives ESP-NOW messages in a callback (`OnDataRecv`), enqueues them into a thread-safe circular queue (size 10)
- Main loop dequeues messages and publishes JSON payloads to MQTT broker via `PubSubClient`
- Gracefully degrades to "ESP-NOW only mode" if WiFi connection fails
- Periodically checks WiFi status and reconnects; updates ESP-NOW peer channel if WiFi channel changes
- MQTT reconnection is rate-limited to every 5 seconds (`MQTT_RECONNECT_INTERVAL_MS`)

## Directory Structure

```
├── platformio.ini                # Build config: two environments (slave, master)
├── CLAUDE.md                     # This file
└── src/
    ├── master/
    │   └── reciever.cpp          # Master node (WiFi + ESP-NOW receiver + MQTT publisher)
    ├── slaves/
    │   └── sender.cpp            # Slave node (BMS reader + ESP-NOW transmitter)
    └── shared/
        ├── config.h              # Shared config: WiFi creds, MAC addresses, MQTT config, DeviceMessage struct
        ├── BQ76952.h             # BMS IC driver header (register map, enums, class declaration)
        └── BQ76952.cpp           # BMS IC driver implementation (I2C communication)
```

## Build System

- **Platform:** PlatformIO (Arduino framework on `espressif32`)
- **Board:** `esp32doit-devkit-v1`
- **Two build environments** in `platformio.ini`:
  - `slave` — compiles `sender.cpp` + `BQ76952.cpp`, includes `src/shared`
  - `master` — compiles `reciever.cpp`, includes `src/shared`, depends on `knolleary/PubSubClient@^2.8`
- **Build commands:**
  ```bash
  pio run -e slave                              # Compile slave
  pio run -e master                             # Compile master
  pio run -e slave -t upload --upload-port COMx  # Flash slave
  pio run -e master -t upload --upload-port COMx # Flash master
  pio device monitor -e slave --port COMx        # Serial monitor slave
  pio device monitor -e master --port COMx       # Serial monitor master
  ```
- **Library dependencies:**
  - Master: `knolleary/PubSubClient@^2.8` (MQTT client)
  - Slave: None (BQ76952 driver is in `src/shared/`, not a PlatformIO lib)

## Key Data Structure

```cpp
// Defined in config.h — packed struct sent over ESP-NOW (binary, deterministic layout)
typedef struct {
  unsigned int v[16];        // Cell voltages (mV), indices 0-15
  unsigned int v_stack;      // Stack voltage (sum of cells, mV)
  unsigned int v_pack;       // Pack voltage (after FETs/sense resistor, mV)
  int current;               // Current (mA, signed: negative = discharge)
  float chip_temp;           // BMS IC internal temperature (deg C)
  float temp1, temp2, temp3; // External thermistors TS1/TS2/TS3 (deg C)
  float charge;              // Accumulated charge (Ah)
  uint32_t charge_time;      // Accumulated charge time (s)
  bool isCharging;
  bool isDischarging;
  char message[50];          // Status string (e.g. "BMS_ACTIVE", "FAKE_DATA_TEST")
} __attribute__((packed)) DeviceMessage;
```

**Size note:** This struct is `__attribute__((packed))` for deterministic wire format over ESP-NOW. Do NOT add padding or reorder fields without updating both sender and receiver.

## Hardware & Communication

| Layer | Protocol | Details |
|-------|----------|---------|
| BMS IC <-> Slave | I2C | Address `0x08`, pins GPIO 21 (SDA) / GPIO 22 (SCL) |
| Slave <-> Master | ESP-NOW | MAC-addressed, unencrypted, channel synced to WiFi AP |
| Master <-> Cloud | MQTT | Broker at `64.23.174.210:1883`, topic `bms/data`, client ID `wbms-master` |
| Debug | UART | 115200 baud on both nodes |

## Configuration (config.h)

All configuration constants are in `src/shared/config.h`:

| Parameter | Constant Name | Value | Notes |
|-----------|---------------|-------|-------|
| WiFi SSID | `WIFI_SSID` | `"WEB3496D"` | Used by master to connect, by slave to scan for channel |
| WiFi Password | `WIFI_PASSWORD` | `"ka159321"` | Hardcoded |
| WiFi Timeout | `WIFI_TIMEOUT_MS` | 15000 ms | Initial connection timeout |
| WiFi Retry Interval | `WIFI_RETRY_INTERVAL_MS` | 10000 ms | Reconnection check interval |
| MQTT Broker | `MQTT_BROKER` | `"64.23.174.210"` | Plain MQTT, no TLS |
| MQTT Port | `MQTT_PORT` | 1883 | Standard MQTT port |
| MQTT Topic | `MQTT_TOPIC` | `"bms/data"` | Publish topic for BMS telemetry |
| MQTT Client ID | `MQTT_CLIENT_ID` | `"wbms-master"` | Must be unique per device |
| MQTT Reconnect | `MQTT_RECONNECT_INTERVAL_MS` | 5000 ms | Rate-limit reconnection attempts |
| Connected Cells | `CONNECTED_CELLS` | 13 | Out of 16 supported by BQ76952 |
| Receiver MAC | `RECEIVER_ADDRESS` | `08:D1:F9:27:AF:28` | Master ESP32 |
| Sender MACs | `SENDER_ADDRESSES` | `D0:EF:76:57:CE:B0` | Array, supports multiple senders |
| Num Senders | `NUM_SENDERS` | Computed | `sizeof(SENDER_ADDRESSES) / sizeof(SENDER_ADDRESSES[0])` |
| Fallback Channel | `FALLBACK_CHANNEL` | 6 | Used if slave can't find the WiFi AP (defined in sender.cpp) |

## Master Queue System (reciever.cpp)

The master uses a circular buffer queue with spinlock-based thread safety:

- **Size:** 10 slots (`QUEUE_SIZE`)
- **Thread safety:** `portMUX` spinlocks (`portENTER_CRITICAL` / `portEXIT_CRITICAL`)
- **Why spinlocks:** The ESP-NOW receive callback runs on the WiFi task (different core), while the main loop runs on the Arduino core. A mutex won't work safely in an ISR-like callback context.
- **Overflow behavior:** Drops newest message when full, logs warning
- **`QueuedMessage` struct** wraps `DeviceMessage` with a `senderIndex` (int) and `hasData` flag (bool)

### Queue API

```cpp
bool enqueueMessage(const DeviceMessage &data, int senderIndex); // Returns false if full
bool dequeueMessage(QueuedMessage &msg);                          // Returns false if empty
```

## JSON Payload Format (MQTT publish)

The master builds JSON manually via string concatenation in `buildJsonPayload()`:

```json
{
  "senderIndex": 1,
  "v1": 3650, "v2": 3700,
  "vStack": 47450,
  "vPack": 47300,
  "current": -250,
  "charge": 1234.5,
  "chargeTime": 3600,
  "chipTemp": 32.5,
  "temp1": 28.3, "temp2": 29.1, "temp3": 27.8,
  "isCharging": false,
  "isDischarging": true,
  "message": "BMS_ACTIVE"
}
```

- Cell voltages with value `0` are **omitted** from the JSON
- `senderIndex` is **1-based** in JSON (0-based internally)
- MQTT buffer size is set to 512 bytes (`mqtt.setBufferSize(512)`)

## Master Main Loop Flow

```
loop() {
  maintainWiFi()      // Check WiFi every 10s, reconnect if down, update ESP-NOW channel if changed
  connectMqtt()       // Rate-limited MQTT reconnect (every 5s), skips if WiFi down or already connected
  mqtt.loop()         // PubSubClient keepalive/receive processing
  dequeueMessage()    // Pop one message from circular queue
  publishToMqtt()     // Serialize to JSON, publish to bms/data topic
  delay(10)           // Yield
}
```

## Slave Main Loop Flow

```
loop() {
  collectfakeBMSData()  // Generate random test data (swap to collectBMSData() for real hardware)
  esp_now_send()        // Send DeviceMessage struct to master MAC
  delay(500)            // 2 Hz update rate
}
```

## BQ76952 Driver (BQ76952.h / BQ76952.cpp)

Custom Arduino library (forked from pranjal-joshi/BQ76952Lib by James Fotherby, MIT license).

### I2C Details

- **Device address:** `0x08` (defined as `BQ_I2C_ADDR`)
- **Bus speed:** 400 kHz (set in `begin(SDA, SCL)`)
- **Timeout:** 10 ms per I2C transaction
- **Internal buffer:** `_DataBuffer[36]` — used for subcommand responses and data memory reads

### Key API Methods

| Method | Returns | Description |
|--------|---------|-------------|
| `begin(SDA, SCL)` | void | Initialize I2C with custom pins at 400 kHz |
| `begin()` | void | Initialize I2C with default pins |
| `reset()` | void | Reset the BMS IC (subcommand 0x0012) |
| `setConnectedCells(n)` | void | Configure active cell count (3-16, writes bitmask to 0x9304) |
| `getCellVoltage(1-16)` | unsigned int | Read individual cell voltage (mV); 17 = stack, 18 = pack |
| `getCurrent()` | int | Read current (signed int16, mA) |
| `getInternalTemp()` | float | IC die temperature (deg C, raw/10 - 273.15) |
| `getThermistorTemp(TS1/TS2/TS3)` | float | External thermistor temperature (deg C) |
| `getAccumulatedCharge()` | float | Coulomb counter reading (Ah, integer + fractional) |
| `getAccumulatedChargeTime()` | uint32_t | Charge accumulation time (s), calls getAccumulatedCharge() internally |
| `ResetAccumulatedCharge()` | void | Reset coulomb counter (subcommand 0x0082) |
| `isCharging()` | bool | FET status register bit 0 |
| `isDischarging()` | bool | FET status register bit 2 |
| `setFET(CHG/DCH/ALL, ON/OFF)` | void | Control charge/discharge FETs |
| `getProtectionStatus()` | bq_protection_t | Returns union with OV, UV, OC, SC fault flags |
| `getTemperatureStatus()` | bq_temp_t | Returns union with over/under temperature flags |
| `GetCellBalancingBitmask()` | uint16_t | Bitmask of which cells are currently balancing |
| `GetCellBalancingTimes(buf)` | void | Fills uint32_t[16] with per-cell cumulative balance seconds |
| `writeByteToMemory(addr, val)` | void | Write single byte to BMS config register (enters/exits config mode) |
| `writeIntToMemory(addr, val)` | void | Write 2 bytes to BMS config register |
| `writeFloatToMemory(addr, val)` | void | Write 4 bytes to BMS config register |
| `subCommandWriteData(subcmd, data, len)` | bool | Write subcommand with payload, checksum, and completion polling |
| `setDebug(bool)` | void | Enable/disable serial debug output |

### Low-Level I2C Communication Pattern

```
Direct Commands:     Write 1-byte address -> Read 2 bytes (LSB first)
Subcommands:         Write [0x3E, cmd_lo, cmd_hi] -> Read response from 0x40+
Data Memory Write:   Enter config -> Write [0x3E, addr_lo, addr_hi, data..., zeros, checksum, length] -> Exit config
Data Memory Read:    Subcommand(addr) -> Read 32 bytes from 0x40 -> Verify checksum from 0x60
```

### Register Map Highlights

- **Direct commands:** Cell voltages (`0x14`-`0x32`), stack (`0x34`), pack (`0x36`), current (`0x3A`), internal temp (`0x68`), FET status (`0x7F`)
- **Subcommands:** Reset (`0x0012`), config update enter/exit (`0x0090`/`0x0092`), FET control (`0x0093`-`0x0096`)
- **Data memory:** Protection thresholds (`0x9261`+), FET options (`0x9308`), balancing config (`0x9335`+), pin configs (`0x92FB`+), cell config (`0x9304`), CC gain (`0x91A8`)
- **Thermistor registers:** TS1=`0x70`, TS2=`0x72`, TS3=`0x74`, HDQ=`0x76`, DCHG=`0x78`, DDSG=`0x7A`
- **Protection enums:** `bq76952_scd_thresh` for short-circuit detection (10-500 mV range, 16 levels)

### Protection Status Structs

```cpp
// bq_protection_t — bit flags from Safety Fault A register (0x03)
SC_DCHG, OC2_DCHG, OC1_DCHG, OC_CHG, CELL_OV, CELL_UV

// bq_temp_t — bit flags from Safety Fault B register (0x05)
OVERTEMP_FET, OVERTEMP_INTERNAL, OVERTEMP_DCHG, OVERTEMP_CHG,
UNDERTEMP_INTERNAL, UNDERTEMP_DCHG, UNDERTEMP_CHG
```

## Function Index

### reciever.cpp (Master)

| Function | Line | Purpose |
|----------|------|---------|
| `enqueueMessage()` | ~24 | Add message to circular queue (ISR-safe) |
| `dequeueMessage()` | ~42 | Remove message from circular queue (ISR-safe) |
| `macToString()` | ~68 | Format MAC address as string |
| `initWiFi()` | ~77 | Connect to WiFi in AP_STA mode with timeout |
| `updateESPNowChannel()` | ~101 | Re-register ESP-NOW peers on channel change |
| `maintainWiFi()` | ~119 | Periodic WiFi health check and reconnect |
| `connectMqtt()` | ~137 | Rate-limited MQTT connection/reconnection |
| `buildJsonPayload()` | ~159 | Serialize DeviceMessage to JSON string |
| `publishToMqtt()` | ~188 | Publish JSON to MQTT topic |
| `findSenderIndex()` | ~208 | Look up sender MAC in SENDER_ADDRESSES array |
| `OnDataRecv()` | ~218 | ESP-NOW receive callback (runs on WiFi task core) |
| `setup()` | ~241 | Init WiFi, MQTT, ESP-NOW, register peers |
| `loop()` | ~277 | Main loop: WiFi maintain, MQTT loop, dequeue & publish |

### sender.cpp (Slave)

| Function | Line | Purpose |
|----------|------|---------|
| `collectBMSData()` | ~15 | Read real BMS data via I2C (currently unused) |
| `collectfakeBMSData()` | ~35 | Generate random test data (currently active) |
| `scanForChannel()` | ~66 | Scan WiFi networks to find AP channel |
| `OnDataSent()` | ~85 | ESP-NOW send callback (log success/failure) |
| `setup()` | ~91 | Init serial, scan channel, init ESP-NOW, register peer |
| `loop()` | ~122 | Collect data, send via ESP-NOW, delay 500ms |

## Coding Conventions

- C++ (Arduino-style) with `setup()` / `loop()` entry points
- Section headers use `// ==================== SECTION ====================` comment blocks
- Serial debug output uses `[TAG]` prefixes: `[WiFi]`, `[ESP-NOW]`, `[MQTT]`, `[Queue]`, `[System]`, `[INFO]`
- Unicode status symbols in serial output: checkmark, X, warning, etc.
- Structs use `__attribute__((packed))` for deterministic wire format
- No dynamic memory allocation — all buffers are fixed-size
- Functions follow a clear pattern: init function in `setup()`, processing in `loop()`
- Constants use `UPPER_SNAKE_CASE`, defined as `const` globals (not `#define` for non-register values)
- MAC addresses stored as `uint8_t[6]` arrays
- JSON built via manual String concatenation (no ArduinoJson library)

## Known Issues & TODOs

- **Filename typo:** `reciever.cpp` should be `receiver.cpp`
- **I2C pins:** GPIO 8/9 marked with `// check ESP pins ***` — verify against actual hardware wiring
- **WiFi credentials hardcoded** in `config.h` — consider externalizing for production
- **No TLS** — MQTT broker uses plain TCP port 1883 (consider TLS on port 8883)
- **No MQTT authentication** — `mqtt.connect()` uses client ID only, no username/password
- **Manual JSON construction** — string concatenation in `buildJsonPayload()` is fragile (consider ArduinoJson)
- **No OTA updates** — no over-the-air firmware update support
- **No local persistence** — BMS data is lost if WiFi/MQTT is down (queue only holds 10 messages)
- **BMS init commented out** — `sender.cpp` setup has BMS hardware init commented out, using fake data
- **Single sender configured** — `SENDER_ADDRESSES` array supports multiple but only one is defined
- **No MQTT subscribe** — master only publishes, no command/control channel from cloud to device
- **`setFET` enum mismatch** — enum uses `DCH` but code references `DCHG` in switch case (line ~534 of BQ76952.cpp)
- **`getTemperatureStatus` bug** — `OVERTEMP_FET` and `OVERTEMP_CHG` both read from `BIT_SB_OTC` (line ~639-642 of BQ76952.cpp), `OVERTEMP_FET` should read `BIT_SB_OTF`
- **PubSubClient default max packet** — 256 bytes; overridden to 512 via `setBufferSize()`, verify JSON payloads stay under this limit

## Dependencies & Versions

| Dependency | Version | Used By | Purpose |
|------------|---------|---------|---------|
| espressif32 | (PlatformIO default) | Both | ESP32 Arduino framework |
| PubSubClient | ^2.8 | Master | MQTT client library |
| Wire (built-in) | N/A | Slave | I2C communication with BQ76952 |
| WiFi (built-in) | N/A | Both | WiFi and ESP-NOW |
| esp_now (built-in) | N/A | Both | Peer-to-peer wireless |
| esp_wifi (built-in) | N/A | Both | Low-level WiFi channel control |

## Important Implementation Details

### Thread Safety
- The ESP-NOW callback (`OnDataRecv`) runs on the WiFi task, which may be on a different CPU core than the Arduino `loop()`. All shared state (the message queue) must use `portMUX` spinlocks, not FreeRTOS mutexes.
- Do NOT use `Serial.print` inside critical sections — it can cause deadlocks.

### ESP-NOW Channel Sync
- ESP-NOW requires sender and receiver to be on the same WiFi channel.
- The slave scans for the WiFi AP SSID to discover the channel dynamically.
- If the master's WiFi channel changes (router reassignment), `updateESPNowChannel()` re-registers all peers.
- If the slave can't find the AP, it falls back to channel 6.

### WiFi Mode
- Master uses `WIFI_AP_STA` (both AP and STA) because ESP-NOW requires the WiFi interface active.
- Slave uses `WIFI_STA` and does NOT connect to WiFi — only scans to find the channel, then uses promiscuous mode to set it.

### DeviceMessage Struct Compatibility
- The struct is `__attribute__((packed))` — both sender and receiver MUST use the same struct definition from `config.h`.
- Any change to this struct (adding/removing/reordering fields) requires reflashing BOTH devices.
- The receiver validates incoming size: `if (len != sizeof(DeviceMessage))` — mismatches are logged and dropped.

### MQTT Behavior
- Uses `PubSubClient` which is synchronous and blocking.
- `mqtt.loop()` must be called regularly to maintain the connection (handles keepalive pings).
- If MQTT publish fails, the message is lost (no retry queue beyond the 10-slot ESP-NOW queue).
- Buffer size is 512 bytes — JSON payloads with all 16 cells active approach ~400 bytes.

### Fake Data Ranges (for testing)
- Cell voltages: 3000-4200 mV (random)
- Current: -500 to +500 mA
- Charge: 100.0 to 5000.0 Ah
- Temperatures: 20.0 to 45.0 deg C
- isCharging/isDischarging: mutually exclusive random booleans
- Message: "FAKE_DATA_TEST"
