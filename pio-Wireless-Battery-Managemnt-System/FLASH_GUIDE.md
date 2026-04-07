# Flash Guide

## Prerequisites
- PlatformIO CLI installed (`pip install platformio`)
- Two ESP32 DevKit V1 boards
- Check your COM ports and update `ports.conf`

## Get MAC Addresses
```
pip install esptool
python scripts/get_mac.py
```

---

## Main Project (Wireless BMS)

Slave reads BMS data, sends via ESP-NOW to Master, which publishes to MQTT.

### Wiring
- BQ76952 IC connected to Slave ESP32 via I2C (SDA=GPIO21, SCL=GPIO22)
- No wires between the two ESPs (wireless via ESP-NOW)

### Flash
```
pio run -e slave -t upload --upload-port COM3
pio run -e master -t upload --upload-port COM6
```

### Monitor
```
pio device monitor -e slave --port COM3
pio device monitor -e master --port COM6
```

### Config
Edit `src/shared/config.h` for WiFi credentials, MQTT broker, MAC addresses, cell count.

---

## Test Board (I2C BQ76952 Simulator + Dashboard)

One ESP simulates the BQ76952 as an I2C slave. The other runs a web dashboard and reads data over I2C.

### Wiring (3 jumper wires between ESPs)
| Signal | Both ESPs |
|--------|-----------|
| SDA    | GPIO 21   |
| SCL    | GPIO 22   |
| GND    | GND       |

### Flash
```
pio run -e tb_bq_node -t upload --upload-port COM6
pio run -e tb_dashboard -t upload --upload-port COM3
```

### Monitor
```
pio device monitor -e tb_bq_node --port COM6
pio device monitor -e tb_dashboard --port COM3
```

### Use
1. Connect phone/laptop to WiFi **`wBMS-TestBoard`** (password: `wbms1234`)
2. Open **http://192.168.4.1**
3. Click **LED ON** to verify I2C — the other ESP's LED should light up

### Config
Edit `src/testboard/tb_config.h` for I2C pins, AP name, cell count.
