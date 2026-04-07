# models.py
from sqlalchemy import Boolean, Column, Integer, String, Float, DateTime, ForeignKey, Index
from sqlalchemy.orm import relationship
from app.models.database import Base
from datetime import datetime

class User(Base):
    __tablename__ = "users"

    id = Column(Integer, primary_key=True, autoincrement=True)
    first_name = Column(String(100), nullable=False)
    last_name = Column(String(100), nullable=False)
    email = Column(String(255), unique=True, nullable=False, index=True)
    password = Column(String(255), nullable=False)

    packs = relationship("Pack", back_populates="owner", cascade="all, delete-orphan")


class Pack(Base):
    __tablename__ = "packs"

    id = Column(Integer, primary_key=True, autoincrement=True)
    name = Column(String(255), nullable=False, index=True)
    pack_identifier = Column(String(50), unique=True, nullable=False, index=True)
    series_count = Column(Integer, nullable=False, default=13)
    parallel_count = Column(Integer, nullable=False, default=4)
    user_id = Column(
        Integer,
        ForeignKey("users.id", ondelete="CASCADE"),
        nullable=False,
        index=True,
    )

    owner = relationship("User", back_populates="packs")
    
    # Relationships - One Pack has many Readings and BatteryReadings
    readings = relationship(
        "Reading", 
        back_populates="pack", 
        cascade="all, delete-orphan",
        lazy="dynamic"  # Query on-demand instead of loading all at once
    )
    battery_readings = relationship(
        "BatteryReading", 
        back_populates="pack", 
        cascade="all, delete-orphan",
        lazy="dynamic"
    )

class Reading(Base):
    __tablename__ = "readings"

    id = Column(Integer, primary_key=True, autoincrement=True)
    timestamp = Column(DateTime, default=datetime.utcnow, nullable=False, index=True)
    
    # Foreign Key to Pack
    pack_id = Column(
        Integer, 
        ForeignKey("packs.id", ondelete="CASCADE", onupdate="CASCADE"), 
        nullable=False,
        index=True
    )
    
    v_real = Column(Float, nullable=False)
    current = Column(Float, nullable=False)
    temperature = Column(Float, nullable=False)
    cycles = Column(Integer, nullable=False)
    v_estimated = Column(Float, nullable=False)
    soc = Column(Float, nullable=False)
    soh = Column(Float, nullable=False)
    ekf_soc = Column(Float, nullable=False)
    power = Column(Float, nullable=True)
    charging_discharging = Column(Boolean, nullable=True)
    
    # Relationship - Many Readings belong to one Pack
    pack = relationship("Pack", back_populates="readings")
    
    # Composite index for efficient queries
    __table_args__ = (
        Index('idx_pack_timestamp', 'pack_id', 'timestamp'),
    )

class BatteryReading(Base):
    __tablename__ = "battery_readings"

    id = Column(Integer, primary_key=True, autoincrement=True)
    battery_position = Column(Integer, nullable=False)
    timestamp = Column(DateTime, default=datetime.utcnow, nullable=False, index=True)
    
    # Foreign Key to Pack
    pack_id = Column(
        Integer, 
        ForeignKey("packs.id", ondelete="CASCADE", onupdate="CASCADE"), 
        nullable=False,
        index=True
    )
    
    voltage = Column(Float, nullable=False)
    
    # Relationship - Many BatteryReadings belong to one Pack
    pack = relationship("Pack", back_populates="battery_readings")
    
    # Composite index for efficient queries
    __table_args__ = (
        Index('idx_pack_battery_timestamp', 'pack_id', 'battery_position', 'timestamp'),
    )