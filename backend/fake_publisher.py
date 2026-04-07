"""
Simulates the master ESP32 publishing BMS data to MQTT.
Run this to test the backend + frontend locally without hardware.

Usage: python fake_publisher.py
"""

import json
import time
import random
import paho.mqtt.client as mqtt

BROKER = "localhost"
PORT = 1883
TOPIC = "bms/data"

client = mqtt.Client(client_id="fake-master", callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
client.connect(BROKER, PORT)
client.loop_start()

print(f"Publishing fake BMS data to {BROKER}:{PORT} topic '{TOPIC}'")
print("Press Ctrl+C to stop\n")

charge_time = 0

try:
    while True:
        # Generate realistic 13-cell voltages (mV)
        base_voltage = random.randint(3500, 3900)
        cells = {}
        for i in range(1, 14):  # 13 connected cells
            cells[f"v{i}"] = base_voltage + random.randint(-100, 100)
        # Cells 14-16 are unconnected (omitted like the real master does)

        v_stack = sum(cells.values())
        charge_time += 1

        payload = {
            "senderIndex": 1,
            **cells,
            "vStack": v_stack,
            "vPack": v_stack - random.randint(50, 200),
            "current": random.randint(-500, 500),
            "charge": round(random.uniform(100, 5000), 1),
            "chargeTime": charge_time,
            "chipTemp": round(random.uniform(25, 40), 1),
            "temp1": round(random.uniform(22, 38), 1),
            "temp2": round(random.uniform(22, 38), 1),
            "temp3": round(random.uniform(22, 38), 1),
            "isCharging": random.choice([True, False]),
            "isDischarging": random.choice([True, False]),
            "message": "FAKE_DATA_TEST",
        }

        client.publish(TOPIC, json.dumps(payload))
        print(f"[{time.strftime('%H:%M:%S')}] vStack={v_stack} current={payload['current']}mA temp={payload['chipTemp']}C")
        time.sleep(0.5)  # Same rate as real slave (2 Hz)

except KeyboardInterrupt:
    print("\nStopped.")
    client.disconnect()
