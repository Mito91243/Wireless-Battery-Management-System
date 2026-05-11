"""Firmware artifact management + master OTA dispatch.

V1 scope:
  * Admin uploads .bin files via multipart POST /v1/firmware
  * Backend serves them at GET /v1/firmware/{id}/blob?token=... with a
    short-lived HMAC token the master is given by the dispatch route
  * POST /v1/packs/{pack_id}/ota mints the token, builds the JSON payload,
    and publishes it on bms/cmd/<master_pairing_code>

Auth: gated by `get_current_user` (matches the rest of this codebase, which
has no admin role yet). Upload is open to any authenticated user — when a
proper role model lands, add the check here.
"""

from __future__ import annotations

import hashlib
import hmac
import time
from pathlib import Path

from fastapi import APIRouter, Depends, File, Form, HTTPException, Request, UploadFile, status
from fastapi.responses import FileResponse
from sqlalchemy.orm import Session

from app.auth import get_current_user
from app.models.database import get_db
from app.models.models import FirmwareImage, Pack, User
from app.models.schemas import FirmwareDispatchRequest, FirmwareImageResponse
from app.mqtt_subscriber import (
    FIRMWARE_DIR,
    OTA_BLOB_TTL_SECONDS,
    OTA_URL_SECRET,
    publish_command,
)

router = APIRouter(prefix="/v1/firmware", tags=["firmware"])


def _ensure_firmware_dir() -> Path:
    d = Path(FIRMWARE_DIR)
    d.mkdir(parents=True, exist_ok=True)
    return d


def _sign_blob_token(image_id: int, expires_at: int) -> str:
    message = f"{image_id}:{expires_at}".encode()
    return hmac.new(OTA_URL_SECRET.encode(), message, hashlib.sha256).hexdigest()


def _verify_blob_token(image_id: int, token: str) -> None:
    # Token format: "<exp>.<sig>"
    parts = (token or "").split(".", 1)
    if len(parts) != 2:
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Bad token format")
    exp_raw, sig = parts
    try:
        exp = int(exp_raw)
    except ValueError:
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Bad token format")
    if exp < int(time.time()):
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Token expired")
    expected = _sign_blob_token(image_id, exp)
    if not hmac.compare_digest(expected, sig):
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Token signature mismatch")


def _mint_blob_url(request: Request, image_id: int) -> str:
    exp = int(time.time()) + OTA_BLOB_TTL_SECONDS
    token = f"{exp}.{_sign_blob_token(image_id, exp)}"
    base = str(request.base_url).rstrip("/")
    return f"{base}/v1/firmware/{image_id}/blob?token={token}"


@router.post("", response_model=FirmwareImageResponse, status_code=status.HTTP_201_CREATED)
async def upload_firmware(
    version: str = Form(...),
    file: UploadFile = File(...),
    _user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    version = version.strip()
    if not version:
        raise HTTPException(status_code=400, detail="Version is required")

    existing = db.query(FirmwareImage).filter(FirmwareImage.version == version).first()
    if existing:
        raise HTTPException(status_code=409, detail=f"Firmware v{version} already exists")

    target_dir = _ensure_firmware_dir()
    safe_name = f"master-{version}.bin"
    artifact_path = target_dir / safe_name

    sha = hashlib.sha256()
    size = 0
    with artifact_path.open("wb") as out:
        while True:
            chunk = await file.read(64 * 1024)
            if not chunk:
                break
            sha.update(chunk)
            size += len(chunk)
            out.write(chunk)

    if size == 0:
        artifact_path.unlink(missing_ok=True)
        raise HTTPException(status_code=400, detail="Uploaded file is empty")

    image = FirmwareImage(
        version=version,
        sha256=sha.hexdigest(),
        size=size,
        artifact_path=str(artifact_path),
    )
    db.add(image)
    db.commit()
    db.refresh(image)
    return image


@router.get("", response_model=list[FirmwareImageResponse])
def list_firmware(
    _user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    return (
        db.query(FirmwareImage)
        .order_by(FirmwareImage.created_at.desc())
        .all()
    )


@router.get("/{image_id}/blob")
def get_firmware_blob(
    image_id: int,
    token: str = "",
    db: Session = Depends(get_db),
):
    # No auth dependency: the master holds a short-lived signed token instead
    # of a JWT. Verifying the token below is the gate.
    _verify_blob_token(image_id, token)

    image = db.query(FirmwareImage).filter(FirmwareImage.id == image_id).first()
    if not image:
        raise HTTPException(status_code=404, detail="Firmware not found")

    path = Path(image.artifact_path)
    if not path.is_file():
        raise HTTPException(status_code=410, detail="Firmware artifact missing on disk")

    return FileResponse(
        path,
        media_type="application/octet-stream",
        filename=f"master-{image.version}.bin",
    )


def dispatch_ota(
    pack_id: int,
    payload: FirmwareDispatchRequest,
    request: Request,
    user: User,
    db: Session,
) -> dict:
    """Mint a signed blob URL and publish an `ota` command to the pack's
    master via MQTT. Exposed for `POST /v1/packs/{pack_id}/ota` in packs.py.
    """
    pack = db.query(Pack).filter(Pack.id == pack_id).first()
    if not pack:
        raise HTTPException(status_code=404, detail="Pack not found")

    if not pack.master_pairing_code:
        raise HTTPException(
            status_code=409,
            detail="Pack has no master_pairing_code yet — wait for the master to publish telemetry",
        )

    image = (
        db.query(FirmwareImage)
        .filter(FirmwareImage.id == payload.firmware_image_id)
        .first()
    )
    if not image:
        raise HTTPException(status_code=404, detail="Firmware image not found")
    if not Path(image.artifact_path).is_file():
        raise HTTPException(status_code=410, detail="Firmware artifact missing on disk")

    blob_url = _mint_blob_url(request, image.id)
    command = {
        "op": "ota",
        "url": blob_url,
        "version": image.version,
        "sha256": image.sha256,
    }
    ok = publish_command(pack.master_pairing_code, command)
    if not ok:
        raise HTTPException(status_code=502, detail="MQTT publish failed")

    return {
        "topic": f"bms/cmd/{pack.master_pairing_code}",
        "payload": command,
        "dispatched_by": user.email,
    }
