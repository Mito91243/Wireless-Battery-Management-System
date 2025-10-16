from typing import List, Optional
import datetime
from fastapi import FastAPI, HTTPException, Query
from pydantic import BaseModel
from datetime import datetime 
from fastapi.middleware.cors import CORSMiddleware
import random

app = FastAPI(title="ESP32 Data Backend", version="1.0.0")

class ESP32Data(BaseModel):
    senderIndex: int
    sensorValue: float
    buttonState: bool
    message: str

class SensorReading(BaseModel):
    senderIndex: int
    sensorValue: float
    buttonState: bool
    message: str
    timestamp: str

sensor_readings: List[SensorReading] = []


app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/")
async def root():
    return {"message": "Hello World"}

@app.post("/v1/sensor-data")
async def receive_sensor_data(data: ESP32Data):
    try:
        new_reading = SensorReading(
            senderIndex=data.senderIndex,
            sensorValue=data.sensorValue,
            buttonState=data.buttonState,
            message=data.message,
            timestamp=datetime.now().replace(microsecond=0).isoformat()
        )
        sensor_readings.append(new_reading)

        if len(sensor_readings) > 1000:
            sensor_readings.pop(0)

        return {
            "status": "success",
            "message": "Data received successfully",
            "received_at": new_reading.timestamp,
            "Data Recieved": {
                "Sender Index":data.senderIndex,
                "Sensor Value":data.sensorValue,
                "Button State":data.buttonState,
                "Message": data.message
            }

        }

    except Exception as e:
        print(f"Error processing data: {e}")
        raise HTTPException(status_code=500, detail="Internal server error")

@app.get("/v1/pack-data/latest")
async def get_latest_data():
    battery_packs = []

    for name, pack_id, base_status in [
        ("Pack Alpha", "BP001", "safe"),
        ("Pack Beta", "BP002", "caution"),
    ]:
        soc = random.randint(50, 100)          # State of charge (50–100%)
        soh = random.randint(90, 100)          # State of health (90–100%)
        voltage = round(random.uniform(45.0, 54.0), 2)  # Volts
        current = round(random.uniform(5.0, 15.0), 2)   # Amps
        temp = round(random.uniform(25.0, 40.0), 2)     # Celsius

        # 13S4P = 52 cells
        cells = []
        for i in range(52):
            cell_voltage = round(random.uniform(3.2, 4.2), 2)
            cell_status = "caution" if cell_voltage < 3.3 or cell_voltage > 4.1 else "safe"
            cells.append({"value": f"{cell_voltage:.2f}", "status": cell_status})

        battery_packs.append({
            "name": name,
            "id": pack_id,
            "status": base_status if random.random() > 0.2 else "caution",  # sometimes flip
            "soc": soc,
            "soh": soh,
            "voltage": str(voltage),
            "current": str(current),
            "temp": str(temp),
            "config": "13S4P (52 cells)",
            "series": "13",
            "parallel": "4",
            "cells": cells
        })

    return {
        "status": "success",
        "timestamp": datetime.now().replace(microsecond=0).isoformat(),
        "packs": battery_packs
    }




@app.get("/health")
async def health_check():
    return {
        "status": "Healthy",
        "total_readings": len(sensor_readings),
        "timestamp": datetime.now().isoformat()
    }
