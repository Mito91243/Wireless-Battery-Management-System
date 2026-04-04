"""
wBMS Backend — FastAPI + MQTT consumer.

Subscribes to the MQTT topic published by the master ESP32,
persists every reading to the database, and serves real data
to the frontend dashboard.
"""

import os
import json
import threading
import logging
from contextlib import asynccontextmanager
from datetime import datetime, timezone

from fastapi import FastAPI, Depends
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy.orm import Session
import paho.mqtt.client as mqtt

from .models.database import engine, Base, get_db, SessionLocal
from .models.models import Pack, PackReading

# ---------------------------------------------------------------------------
# Configuration (all overridable via env vars)
# ---------------------------------------------------------------------------
MQTT_BROKER = os.getenv("MQTT_BROKER", "3.110.221.59")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "bms/data")
MQTT_USERNAME = os.getenv("MQTT_USERNAME", "")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")

FRONTEND_ORIGIN = os.getenv("FRONTEND_ORIGIN", "http://localhost:5173")

# How many cells are connected per slave (from config.h CONNECTED_CELLS)
CONNECTED_CELLS = int(os.getenv("CONNECTED_CELLS", "13"))
# Pack topology: parallel groups per series cell
PARALLEL_COUNT = int(os.getenv("PARALLEL_COUNT", "4"))

log = logging.getLogger("wbms")
logging.basicConfig(level=logging.INFO)

# ---------------------------------------------------------------------------
# SoC estimation (voltage-based, Li-ion approximate curve)
# ---------------------------------------------------------------------------
_SOC_TABLE = [
    (2800, 0), (3000, 2), (3300, 5), (3500, 10), (3600, 20),
    (3700, 40), (3800, 60), (3900, 75), (4000, 85), (4100, 95), (4200, 100),
]


def _estimate_soc(avg_cell_mv: float) -> int:
    """Estimate SoC % from average cell voltage (mV) using linear interpolation."""
    if avg_cell_mv <= _SOC_TABLE[0][0]:
        return 0
    if avg_cell_mv >= _SOC_TABLE[-1][0]:
        return 100
    for i in range(len(_SOC_TABLE) - 1):
        v1, s1 = _SOC_TABLE[i]
        v2, s2 = _SOC_TABLE[i + 1]
        if v1 <= avg_cell_mv <= v2:
            return int(s1 + (s2 - s1) * (avg_cell_mv - v1) / (v2 - v1))
    return 50


def _compute_status(cell_voltages: list[int], chip_temp: float, temp1: float) -> str:
    """Derive pack health status from raw BMS readings."""
    active = [v for v in cell_voltages if v > 0]
    if not active:
        return "alert"
    if any(v < 2800 or v > 4250 for v in active):
        return "alert"
    if chip_temp > 60 or temp1 > 55:
        return "alert"
    if any(v < 3100 or v > 4150 for v in active):
        return "caution"
    if chip_temp > 45 or temp1 > 40:
        return "caution"
    return "safe"


# ---------------------------------------------------------------------------
# MQTT consumer (runs in a background daemon thread)
# ---------------------------------------------------------------------------
def _on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        log.info("MQTT connected — subscribing to %s", MQTT_TOPIC)
        client.subscribe(MQTT_TOPIC)
    else:
        log.error("MQTT connection failed, rc=%d", rc)


def _on_message(client, userdata, msg):
    """Called for every message on bms/data. Parses JSON and writes to DB."""
    try:
        payload = json.loads(msg.payload.decode())
    except (json.JSONDecodeError, UnicodeDecodeError):
        log.warning("Ignoring malformed MQTT payload")
        return

    sender_index = payload.get("senderIndex")
    if sender_index is None:
        log.warning("MQTT message missing senderIndex, skipping")
        return

    # Extract cell voltages (v1..v16), missing keys default to 0
    cell_voltages = [payload.get(f"v{i}", 0) for i in range(1, 17)]

    db: Session = SessionLocal()
    try:
        # Auto-create pack on first message from a new sender
        pack = db.query(Pack).filter(Pack.sender_index == sender_index).first()
        if pack is None:
            pack = Pack(name=f"Pack {sender_index}", sender_index=sender_index)
            db.add(pack)
            db.flush()
            log.info("Auto-created Pack '%s' for senderIndex=%d", pack.name, sender_index)

        reading = PackReading(
            pack_id=pack.id,
            sender_index=sender_index,
            cell_voltages=cell_voltages,
            v_stack=payload.get("vStack", 0),
            v_pack=payload.get("vPack", 0),
            current=payload.get("current", 0),
            chip_temp=payload.get("chipTemp", 0.0),
            temp1=payload.get("temp1", 0.0),
            temp2=payload.get("temp2", 0.0),
            temp3=payload.get("temp3", 0.0),
            charge=payload.get("charge", 0.0),
            charge_time=payload.get("chargeTime", 0),
            is_charging=payload.get("isCharging", False),
            is_discharging=payload.get("isDischarging", False),
            message=payload.get("message", ""),
        )
        db.add(reading)
        db.commit()
        log.info(
            "Stored reading for Pack %d (sender %d): vStack=%d current=%d",
            pack.id, sender_index, reading.v_stack, reading.current,
        )
    except Exception:
        db.rollback()
        log.exception("Failed to store MQTT reading")
    finally:
        db.close()


def _start_mqtt():
    """Launch MQTT client in a daemon thread."""
    client = mqtt.Client(
        client_id="wbms-backend",
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
    )
    if MQTT_USERNAME:
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.on_connect = _on_connect
    client.on_message = _on_message

    def _loop():
        try:
            client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
            client.loop_forever()
        except Exception:
            log.exception("MQTT thread crashed")

    t = threading.Thread(target=_loop, daemon=True)
    t.start()
    log.info("MQTT subscriber thread started (broker=%s:%d)", MQTT_BROKER, MQTT_PORT)


# ---------------------------------------------------------------------------
# FastAPI application
# ---------------------------------------------------------------------------
@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup: create tables + start MQTT
    Base.metadata.create_all(bind=engine)
    log.info("Database tables ensured")
    _start_mqtt()
    yield
    # Shutdown (nothing to clean up — daemon thread dies with process)


app = FastAPI(title="wBMS Backend", version="2.0.0", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=[FRONTEND_ORIGIN, "http://localhost:5173", "http://127.0.0.1:5173"],
    allow_credentials=False,
    allow_methods=["GET", "POST"],
    allow_headers=["*"],
)


# ---------------------------------------------------------------------------
# Endpoints
# ---------------------------------------------------------------------------
@app.get("/")
async def root():
    return {"message": "wBMS Backend", "docs": "/docs"}


@app.get("/health")
async def health(db: Session = Depends(get_db)):
    reading_count = db.query(PackReading).count()
    pack_count = db.query(Pack).count()
    return {
        "status": "healthy",
        "packs": pack_count,
        "total_readings": reading_count,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }


@app.get("/v1/pack-data/latest")
async def get_latest_pack_data(db: Session = Depends(get_db)):
    """
    Return the latest reading for every known pack, formatted for the
    frontend dashboard (matches the shape the React Dashboard expects).
    """
    packs = db.query(Pack).all()

    result_packs = []
    for pack in packs:
        latest: PackReading | None = (
            db.query(PackReading)
            .filter(PackReading.pack_id == pack.id)
            .order_by(PackReading.timestamp.desc())
            .first()
        )
        if latest is None:
            continue

        # Active cell voltages (non-zero)
        active_cells = [v for v in latest.cell_voltages if v > 0]
        avg_cell_mv = sum(active_cells) / len(active_cells) if active_cells else 0
        num_series = len(active_cells) if active_cells else CONNECTED_CELLS

        soc = _estimate_soc(avg_cell_mv)
        status = _compute_status(latest.cell_voltages, latest.chip_temp, latest.temp1)

        # Build 13S × 4P cell grid for the frontend.
        # Each series group has PARALLEL_COUNT cells at the same voltage.
        cells = []
        for i in range(num_series):
            v_mv = active_cells[i] if i < len(active_cells) else 0
            v_display = f"{v_mv / 1000:.2f}"
            if v_mv < 3100 or v_mv > 4150:
                cell_status = "caution"
            elif v_mv < 2800 or v_mv > 4250:
                cell_status = "alert"
            else:
                cell_status = "safe"
            for _ in range(PARALLEL_COUNT):
                cells.append({"value": v_display, "status": cell_status})

        # Pack-level voltage/current: convert from mV/mA to V/A
        pack_voltage = latest.v_stack / 1000
        pack_current = latest.current / 1000

        result_packs.append({
            "name": pack.name,
            "id": f"BP{pack.sender_index:03d}",
            "status": status,
            "soc": soc,
            "soh": 100,  # requires cycle data to compute properly
            "voltage": f"{pack_voltage:.2f}",
            "current": f"{pack_current:.2f}",
            "temp": f"{latest.chip_temp:.2f}",
            "config": f"{num_series}S{PARALLEL_COUNT}P ({num_series * PARALLEL_COUNT} cells)",
            "series": str(num_series),
            "parallel": str(PARALLEL_COUNT),
            "cells": cells,
            "raw": {
                "charge": latest.charge,
                "chargeTime": latest.charge_time,
                "temp1": latest.temp1,
                "temp2": latest.temp2,
                "temp3": latest.temp3,
                "isCharging": latest.is_charging,
                "isDischarging": latest.is_discharging,
                "message": latest.message,
            },
        })

    return {
        "status": "success",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "packs": result_packs,
    }


@app.get("/v1/pack-data/history")
async def get_pack_history(
    pack_id: int = 1,
    limit: int = 100,
    db: Session = Depends(get_db),
):
    """Return recent readings for a specific pack (for charting)."""
    readings = (
        db.query(PackReading)
        .filter(PackReading.pack_id == pack_id)
        .order_by(PackReading.timestamp.desc())
        .limit(limit)
        .all()
    )
    readings.reverse()  # oldest first for chart display

    return {
        "status": "success",
        "pack_id": pack_id,
        "readings": [
            {
                "timestamp": r.timestamp.isoformat(),
                "v_stack": r.v_stack,
                "v_pack": r.v_pack,
                "current": r.current,
                "chip_temp": r.chip_temp,
                "temp1": r.temp1,
                "charge": r.charge,
                "is_charging": r.is_charging,
            }
            for r in readings
        ],
    }
