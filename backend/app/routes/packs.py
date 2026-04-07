import random
from datetime import datetime

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session

from app.auth import get_current_user
from app.models.database import get_db
from app.models.models import User, Pack, Reading, BatteryReading
from app.models.schemas import PackCreate, PackResponse

router = APIRouter(prefix="/v1/packs", tags=["packs"])


@router.post("", response_model=PackResponse, status_code=status.HTTP_201_CREATED)
def create_pack(
    pack_data: PackCreate,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    existing = db.query(Pack).filter(Pack.pack_identifier == pack_data.pack_identifier).first()
    if existing:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="A pack with this identifier already exists",
        )

    pack = Pack(
        name=pack_data.name,
        pack_identifier=pack_data.pack_identifier,
        series_count=pack_data.series_count,
        parallel_count=pack_data.parallel_count,
        user_id=current_user.id,
    )
    db.add(pack)
    db.commit()
    db.refresh(pack)
    return pack


@router.get("", response_model=list[PackResponse])
def list_packs(
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    return db.query(Pack).filter(Pack.user_id == current_user.id).all()


@router.delete("/{pack_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_pack(
    pack_id: int,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    pack = db.query(Pack).filter(Pack.id == pack_id, Pack.user_id == current_user.id).first()
    if not pack:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Pack not found")
    db.delete(pack)
    db.commit()


@router.get("/data/latest")
def get_latest_pack_data(
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    packs = db.query(Pack).filter(Pack.user_id == current_user.id).all()

    battery_packs = []
    for pack in packs:
        series = pack.series_count
        parallel = pack.parallel_count
        total_cells = series * parallel

        # Try to get latest real reading from DB
        latest_reading = (
            db.query(Reading)
            .filter(Reading.pack_id == pack.id)
            .order_by(Reading.timestamp.desc())
            .first()
        )

        if latest_reading:
            soc = int(latest_reading.soc)
            soh = int(latest_reading.soh)
            voltage = f"{latest_reading.v_real:.2f}"
            current_val = f"{latest_reading.current:.2f}"
            temp = f"{latest_reading.temperature:.2f}"
            power = latest_reading.power or (latest_reading.v_real * latest_reading.current)

            # Get cell voltages from DB
            cell_readings = (
                db.query(BatteryReading)
                .filter(BatteryReading.pack_id == pack.id)
                .order_by(BatteryReading.timestamp.desc())
                .limit(total_cells)
                .all()
            )

            if cell_readings:
                cells = []
                for cr in sorted(cell_readings, key=lambda x: x.battery_position):
                    v = cr.voltage
                    cell_status = "caution" if v < 3.3 or v > 4.1 else "safe"
                    cells.append({"value": f"{v:.2f}", "status": cell_status})
                # Pad if not enough cell readings
                while len(cells) < total_cells:
                    cells.append({"value": "0.00", "status": "safe"})
            else:
                cells = _mock_cells(total_cells)

            pack_status = "safe"
            if soc < 20 or soh < 80:
                pack_status = "alert"
            elif soc < 40 or soh < 90:
                pack_status = "caution"
        else:
            # Generate mock data when no real readings exist
            soc = random.randint(50, 100)
            soh = random.randint(90, 100)
            v = round(random.uniform(3.2 * series, 4.2 * series) / series * series, 2)
            voltage = str(v)
            current_val = str(round(random.uniform(5.0, 15.0), 2))
            temp = str(round(random.uniform(25.0, 40.0), 2))
            cells = _mock_cells(total_cells)
            pack_status = "safe" if random.random() > 0.2 else "caution"

        battery_packs.append({
            "name": pack.name,
            "id": pack.pack_identifier,
            "status": pack_status,
            "soc": soc,
            "soh": soh,
            "voltage": voltage,
            "current": current_val,
            "temp": temp,
            "config": f"{series}S{parallel}P ({total_cells} cells)",
            "series": str(series),
            "parallel": str(parallel),
            "cells": cells,
        })

    return {
        "status": "success",
        "timestamp": datetime.now().replace(microsecond=0).isoformat(),
        "packs": battery_packs,
    }


def _mock_cells(count: int) -> list:
    cells = []
    for _ in range(count):
        cell_voltage = round(random.uniform(3.2, 4.2), 2)
        cell_status = "caution" if cell_voltage < 3.3 or cell_voltage > 4.1 else "safe"
        cells.append({"value": f"{cell_voltage:.2f}", "status": cell_status})
    return cells
