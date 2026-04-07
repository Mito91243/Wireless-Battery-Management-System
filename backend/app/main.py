from typing import List
from datetime import datetime

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from app.models.database import engine, Base
from app.routes.auth import router as auth_router
from app.routes.packs import router as packs_router

# Import models so Base knows about them
from app.models import models  # noqa: F401

app = FastAPI(title="WBMS Backend", version="1.0.0")


@app.on_event("startup")
def on_startup():
    try:
        Base.metadata.create_all(bind=engine)
    except Exception as e:
        print(f"Warning: Could not create tables on startup: {e}")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include routers
app.include_router(auth_router)
app.include_router(packs_router)


# --- Legacy / utility endpoints ---

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


@app.get("/")
async def root():
    return {"message": "WBMS API"}


@app.post("/v1/sensor-data")
async def receive_sensor_data(data: ESP32Data):
    try:
        new_reading = SensorReading(
            senderIndex=data.senderIndex,
            sensorValue=data.sensorValue,
            buttonState=data.buttonState,
            message=data.message,
            timestamp=datetime.now().replace(microsecond=0).isoformat(),
        )
        sensor_readings.append(new_reading)

        if len(sensor_readings) > 1000:
            sensor_readings.pop(0)

        return {
            "status": "success",
            "message": "Data received successfully",
            "received_at": new_reading.timestamp,
        }
    except Exception as e:
        print(f"Error processing data: {e}")
        raise HTTPException(status_code=500, detail="Internal server error")


@app.get("/health")
async def health_check():
    return {
        "status": "Healthy",
        "total_readings": len(sensor_readings),
        "timestamp": datetime.now().isoformat(),
    }
