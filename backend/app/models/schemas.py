from pydantic import BaseModel, EmailStr
from datetime import datetime
from typing import Optional


class Token(BaseModel):
    access_token: str
    token_type: str


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
    pack_identifier: str
    pairing_code: str = ""
    series_count: int = 3
    parallel_count: int = 1


class PackClaim(BaseModel):
    pairing_code: str


class PackResponse(BaseModel):
    id: int
    name: str
    pack_identifier: str
    pairing_code: str
    series_count: int
    parallel_count: int
    user_id: Optional[int] = None
    auto_created: bool = False

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