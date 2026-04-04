from sqlalchemy import Boolean, Column, Integer, String, Float, DateTime, ForeignKey, Index, JSON
from sqlalchemy.orm import relationship
from .database import Base
from datetime import datetime, timezone


class User(Base):
    __tablename__ = "users"

    id = Column(Integer, primary_key=True, autoincrement=True)
    first_name = Column(String(100), nullable=False)
    last_name = Column(String(100), nullable=False)
    email = Column(String(255), unique=True, nullable=False, index=True)
    password = Column(String(255), nullable=False)


class Pack(Base):
    __tablename__ = "packs"

    id = Column(Integer, primary_key=True, autoincrement=True)
    name = Column(String(255), unique=True, nullable=False, index=True)
    sender_index = Column(Integer, unique=True, nullable=False)

    readings = relationship(
        "PackReading",
        back_populates="pack",
        cascade="all, delete-orphan",
        lazy="dynamic",
    )


class PackReading(Base):
    """One row per MQTT message — matches the JSON the master publishes."""
    __tablename__ = "pack_readings"

    id = Column(Integer, primary_key=True, autoincrement=True)
    timestamp = Column(
        DateTime, default=lambda: datetime.now(timezone.utc), nullable=False, index=True
    )

    pack_id = Column(
        Integer,
        ForeignKey("packs.id", ondelete="CASCADE"),
        nullable=False,
        index=True,
    )

    sender_index = Column(Integer, nullable=False)

    # Cell voltages stored as JSON list of ints (mV), e.g. [3650, 3700, ...]
    cell_voltages = Column(JSON, nullable=False)

    v_stack = Column(Integer, nullable=False)   # mV
    v_pack = Column(Integer, nullable=False)    # mV
    current = Column(Integer, nullable=False)   # mA (signed)

    chip_temp = Column(Float, nullable=False)   # deg C
    temp1 = Column(Float, nullable=False)
    temp2 = Column(Float, nullable=False)
    temp3 = Column(Float, nullable=False)

    charge = Column(Float, nullable=False)       # Ah
    charge_time = Column(Integer, nullable=False) # seconds

    is_charging = Column(Boolean, nullable=False)
    is_discharging = Column(Boolean, nullable=False)
    message = Column(String(50), nullable=True)

    pack = relationship("Pack", back_populates="readings")

    __table_args__ = (
        Index("idx_pack_timestamp", "pack_id", "timestamp"),
    )
