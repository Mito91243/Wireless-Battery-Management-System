from pydantic import BaseModel
from datetime import datetime
from typing import Optional, List


class UserCreate(BaseModel):
    first_name: str
    last_name: str
    email: str
    password: str


class UserResponse(BaseModel):
    id: int
    first_name: str
    last_name: str
    email: str

    class Config:
        from_attributes = True


class PackResponse(BaseModel):
    id: int
    name: str
    sender_index: int

    class Config:
        from_attributes = True


class PackReadingResponse(BaseModel):
    id: int
    timestamp: datetime
    pack_id: int
    sender_index: int
    cell_voltages: List[int]
    v_stack: int
    v_pack: int
    current: int
    chip_temp: float
    temp1: float
    temp2: float
    temp3: float
    charge: float
    charge_time: int
    is_charging: bool
    is_discharging: bool
    message: Optional[str] = None

    class Config:
        from_attributes = True
