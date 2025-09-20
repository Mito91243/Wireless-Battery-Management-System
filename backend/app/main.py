from typing import List, Optional
import datetime
from fastapi import FastAPI, HTTPException, Query
from pydantic import BaseModel
from datetime import datetime 

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

@app.get("/v1/sensor-data/latest")
async def get_latest_data_per_sensor():
    if not sensor_readings:
        return {
            "status": "success",
            "message": "No sensor data available",
            "data": []
        }
    
    latest_readings = {}
    
    for reading in sensor_readings:
        sensor_index = reading.senderIndex
        if sensor_index not in latest_readings:
            latest_readings[sensor_index] = reading
        else:
            if reading.timestamp > latest_readings[sensor_index].timestamp:
                latest_readings[sensor_index] = reading
    
    return {
        "status": "success",
        "message": f"Latest data for {len(latest_readings)} sensors",
        "total_sensors": len(latest_readings),
        "data": list(latest_readings.values())
    }

@app.get("/v1/sensor-data/sensor/{sensor_index}")
async def get_data_by_sensor_index(sensor_index: int):
    """Get all readings for a specific sensor index"""
    sensor_data = [reading for reading in sensor_readings if reading.senderIndex == sensor_index]
    
    if not sensor_data:
        raise HTTPException(
            status_code=404, 
            detail=f"No data found for sensor index {sensor_index}"
        )
    
    return {
        "status": "success",
        "message": f"Retrieved {len(sensor_data)} readings for sensor {sensor_index}",
        "sensor_index": sensor_index,
        "total_readings": len(sensor_data),
        "data": sensor_data
    }

@app.get("/v1/sensor-data/all")
async def get_all_sensor_data(
    limit: Optional[int] = Query(None, description="Limit number of results"),
    offset: Optional[int] = Query(0, description="Offset for pagination")
):
    """Get all sensor readings with optional pagination"""
    if not sensor_readings:
        return {
            "status": "success",
            "message": "No sensor data available",
            "total_readings": 0,
            "data": []
        }
    
    start_index = offset
    if limit:
        end_index = start_index + limit
        paginated_data = sensor_readings[start_index:end_index]
    else:
        paginated_data = sensor_readings[start_index:]
    
    return {
        "status": "success",
        "message": f"Retrieved {len(paginated_data)} readings",
        "total_readings": len(sensor_readings),
        "returned_readings": len(paginated_data),
        "offset": offset,
        "limit": limit,
        "data": paginated_data
    }


@app.get("/health")
async def health_check():
    return {
        "status": "Healthy",
        "total_readings": len(sensor_readings),
        "timestamp": datetime.now().isoformat()
    }
