# schemas.py (optional but recommended for Pydantic models)
from pydantic import BaseModel, EmailStr
from datetime import datetime
from typing import Optional

class UserCreate(BaseModel):
    first_name: str
    last_name: str
    email: EmailStr
    password: str

class UserResponse(BaseModel):
    id: int
    first_name: str
    last_name: str
    email: str
    
    class Config:
        from_attributes = True

class PackCreate(BaseModel):
    name: str

class PackResponse(BaseModel):
    id: int
    name: str
    
    class Config:
        from_attributes = True

class ReadingCreate(BaseModel):
    pack_id: int
    v_real: float
    current: float
    temperature: float
    cycles: int
    v_estimated: float
    soc: float
    soh: float
    ekf_soc: float
    power: Optional[float] = None
    charging_discharging: Optional[bool] = None

class BatteryReadingCreate(BaseModel):
    battery_position: int
    pack_id: int
    voltage: float