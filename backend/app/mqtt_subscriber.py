"""
MQTT subscriber that consumes the BMS telemetry stream published by the
master ESP32 and persists it to the database.

The master publishes JSON payloads to `MQTT_TOPIC` (default `bms/data`) with a
shape roughly like::

    {
      "senderIndex": 1,
      "v1": 3650, "v2": 3700, ... "v16": 3690,   # mV (cells with value 0 omitted)
      "vStack": 47450,                            # mV
      "vPack":  47300,                            # mV
      "current": -250,                            # mA (negative = discharge)
      "charge":  1234.5,                          # Ah
      "chargeTime": 3600,                         # s
      "chipTemp": 32.5,                           # degC
      "temp1": 28.3, "temp2": 29.1, "temp3": 27.8,
      "isCharging": false,
      "isDischarging": true,
      "message": "BMS_ACTIVE",
      "packId": "wbms-pack-1"                     # optional override
    }

Each message creates one `Reading` row (pack-level summary) and N
`BatteryReading` rows (one per present cell voltage).

Design notes:
  * paho-mqtt's network loop runs in its own thread; callbacks fire on that
    thread, so we open a dedicated SQLAlchemy session per message and close
    it immediately (no dependency on FastAPI's request-scoped `get_db`).
  * Failures (broker down, malformed payload, missing pack) are logged but
    never propagate — the API must keep working even if MQTT is unavailable.
  * Packs are matched by `pack_identifier`. The identifier is read from the
    payload `packId` field if present, otherwise built from
    `MQTT_PACK_ID_TEMPLATE` (default ``wbms-pack-{senderIndex}``). Unknown
    packs are skipped with a throttled warning.
"""

from __future__ import annotations

import json
import logging
import os
import threading
from datetime import datetime
from typing import Any, Optional

import paho.mqtt.client as mqtt
from sqlalchemy.orm import Session

from app.models.database import SessionLocal
from app.models.models import BatteryReading, Pack, Reading

log = logging.getLogger("wbms.mqtt")

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "bms/data")
MQTT_USERNAME = os.getenv("MQTT_USERNAME") or None
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD") or None
MQTT_CLIENT_ID = os.getenv("MQTT_CLIENT_ID", "wbms-backend")
MQTT_PACK_ID_TEMPLATE = os.getenv("MQTT_PACK_ID_TEMPLATE", "wbms-pack-{senderIndex}")

# Nominal LiFePO4/Li-ion SOC curve endpoints (volts per cell)
_CELL_V_EMPTY = 3.0
_CELL_V_FULL = 4.2

# Module-level client so start/stop can manage a single instance.
_client: Optional[mqtt.Client] = None
_client_lock = threading.Lock()
_unknown_packs_warned: set[str] = set()


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _resolve_pack_identifier(payload: dict[str, Any]) -> Optional[str]:
    explicit = payload.get("packId") or payload.get("pack_identifier")
    if explicit:
        return str(explicit)
    sender_index = payload.get("senderIndex")
    if sender_index is None:
        return None
    try:
        return MQTT_PACK_ID_TEMPLATE.format(senderIndex=int(sender_index))
    except (ValueError, KeyError):
        return None


def _estimate_soc(cell_voltages_v: list[float]) -> float:
    """Simple linear SoC estimate from the mean cell voltage."""
    if not cell_voltages_v:
        return 0.0
    mean_v = sum(cell_voltages_v) / len(cell_voltages_v)
    pct = (mean_v - _CELL_V_EMPTY) / (_CELL_V_FULL - _CELL_V_EMPTY) * 100.0
    return max(0.0, min(100.0, pct))


def _extract_cell_voltages_mv(payload: dict[str, Any]) -> dict[int, float]:
    """Return {position: voltage_volts} for keys v1..v16 present in payload."""
    cells: dict[int, float] = {}
    for i in range(1, 17):
        val = payload.get(f"v{i}")
        if val is None:
            continue
        try:
            mv = float(val)
        except (TypeError, ValueError):
            continue
        if mv <= 0:
            continue
        cells[i] = mv / 1000.0  # mV → V
    return cells


def _charging_discharging_flag(payload: dict[str, Any]) -> Optional[bool]:
    """Map firmware's split isCharging/isDischarging into a single tri-state.

    True  = charging, False = discharging, None = idle / unknown.
    """
    if payload.get("isCharging"):
        return True
    if payload.get("isDischarging"):
        return False
    return None


def _persist_reading(db: Session, pack: Pack, payload: dict[str, Any]) -> None:
    cell_voltages = _extract_cell_voltages_mv(payload)

    # Pack-level electricals
    v_pack_mv = payload.get("vPack") or payload.get("vStack") or 0
    try:
        v_real = float(v_pack_mv) / 1000.0  # mV → V
    except (TypeError, ValueError):
        v_real = 0.0

    try:
        current_a = float(payload.get("current", 0)) / 1000.0  # mA → A
    except (TypeError, ValueError):
        current_a = 0.0

    # Average of available thermistors, fall back to chip temp
    temps = [
        payload.get(k)
        for k in ("temp1", "temp2", "temp3")
        if payload.get(k) is not None
    ]
    try:
        temperature = (
            sum(float(t) for t in temps) / len(temps)
            if temps
            else float(payload.get("chipTemp", 0.0))
        )
    except (TypeError, ValueError):
        temperature = 0.0

    soc = _estimate_soc(list(cell_voltages.values()))
    soh = 100.0  # no long-term health estimate yet
    power = v_real * current_a
    now = datetime.utcnow()

    reading = Reading(
        timestamp=now,
        pack_id=pack.id,
        v_real=v_real,
        current=current_a,
        temperature=temperature,
        cycles=0,
        v_estimated=v_real,
        soc=soc,
        soh=soh,
        ekf_soc=soc,
        power=power,
        charging_discharging=_charging_discharging_flag(payload),
    )
    db.add(reading)

    for position, voltage_v in cell_voltages.items():
        db.add(
            BatteryReading(
                battery_position=position,
                timestamp=now,
                pack_id=pack.id,
                voltage=voltage_v,
            )
        )

    db.commit()


def _handle_payload(raw: bytes) -> None:
    try:
        payload = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        log.warning("Dropping malformed MQTT payload: %s", exc)
        return

    if not isinstance(payload, dict):
        log.warning("Dropping non-object MQTT payload: %r", type(payload).__name__)
        return

    pack_identifier = _resolve_pack_identifier(payload)
    if not pack_identifier:
        log.warning("MQTT payload has no packId or senderIndex: %r", payload)
        return

    db = SessionLocal()
    try:
        pack = (
            db.query(Pack).filter(Pack.pack_identifier == pack_identifier).first()
        )
        if pack is None:
            if pack_identifier not in _unknown_packs_warned:
                _unknown_packs_warned.add(pack_identifier)
                log.warning(
                    "MQTT payload targets unknown pack_identifier=%r; create a pack "
                    "with this identifier in the dashboard to start persisting data",
                    pack_identifier,
                )
            return
        _persist_reading(db, pack, payload)
    except Exception:
        db.rollback()
        log.exception("Failed to persist MQTT reading for pack=%r", pack_identifier)
    finally:
        db.close()


# ---------------------------------------------------------------------------
# paho-mqtt callbacks
# ---------------------------------------------------------------------------

def _on_connect(client, _userdata, _flags, reason_code, _properties=None):
    # paho-mqtt v2 passes a ReasonCode; v1 passed an int. Both stringify.
    log.info("MQTT connected (reason=%s), subscribing to %r", reason_code, MQTT_TOPIC)
    client.subscribe(MQTT_TOPIC, qos=0)


def _on_disconnect(_client, _userdata, _flags, reason_code, _properties=None):
    log.warning("MQTT disconnected (reason=%s); paho will auto-reconnect", reason_code)


def _on_message(_client, _userdata, msg):
    _handle_payload(msg.payload)


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def start_mqtt() -> None:
    """Start the MQTT subscriber in a background thread.

    Safe to call multiple times — subsequent calls are no-ops. Broker
    unavailability at startup does NOT raise; paho will keep retrying.
    """
    global _client
    with _client_lock:
        if _client is not None:
            return

        client = mqtt.Client(
            client_id=MQTT_CLIENT_ID,
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        )
        if MQTT_USERNAME:
            client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

        client.on_connect = _on_connect
        client.on_disconnect = _on_disconnect
        client.on_message = _on_message
        # paho handles reconnect automatically; tune the backoff a bit.
        client.reconnect_delay_set(min_delay=1, max_delay=30)

        try:
            client.connect_async(MQTT_BROKER, MQTT_PORT, keepalive=60)
        except Exception as exc:
            log.warning(
                "MQTT initial connect_async to %s:%s failed: %s (will keep retrying)",
                MQTT_BROKER,
                MQTT_PORT,
                exc,
            )

        client.loop_start()
        _client = client
        log.info("MQTT subscriber started (broker=%s:%s)", MQTT_BROKER, MQTT_PORT)


def stop_mqtt() -> None:
    """Stop the MQTT subscriber. Safe to call if not started."""
    global _client
    with _client_lock:
        if _client is None:
            return
        try:
            _client.loop_stop()
            _client.disconnect()
        except Exception:
            log.exception("Error while stopping MQTT client")
        _client = None
        log.info("MQTT subscriber stopped")
