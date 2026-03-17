#!/bin/bash
# ==============================================================
# flash.sh — Upload and monitor sender/receiver ESP32 firmware
# ==============================================================
# Usage:
#   ./flash.sh              # Upload both, then monitor both
#   ./flash.sh sender       # Upload + monitor sender only
#   ./flash.sh receiver     # Upload + monitor receiver only
#   ./flash.sh monitor      # Skip upload, monitor both
# ==============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONF="$SCRIPT_DIR/ports.conf"
MONITOR_PY="$SCRIPT_DIR/monitor.py"

# ==================== FIND TOOLS ====================

# Find pio.exe
if [ -x "$HOME/.platformio/penv/Scripts/pio.exe" ]; then
  PIO="$HOME/.platformio/penv/Scripts/pio.exe"
elif command -v pio &>/dev/null; then
  PIO="pio"
else
  echo "[ERROR] pio not found. Is PlatformIO installed?"
  exit 1
fi

# Find python — check PlatformIO's bundled python first, then system
if [ -x "$HOME/.platformio/penv/Scripts/python.exe" ]; then
  PYTHON="$HOME/.platformio/penv/Scripts/python.exe"
elif command -v python &>/dev/null; then
  PYTHON="python"
elif command -v python3 &>/dev/null; then
  PYTHON="python3"
else
  echo "[ERROR] Python not found."
  exit 1
fi

# ==================== LOAD CONFIG ====================
if [ ! -f "$CONF" ]; then
  echo "[ERROR] ports.conf not found at: $CONF"
  echo ""
  echo "Create it next to flash.sh with:"
  echo "  SENDER_PORT=COM3"
  echo "  RECEIVER_PORT=COM4"
  exit 1
fi
source "$CONF"

if [ -z "$SENDER_PORT" ] || [ -z "$RECEIVER_PORT" ]; then
  echo "[ERROR] ports.conf must define SENDER_PORT and RECEIVER_PORT"
  exit 1
fi

# Check monitor.py exists
if [ ! -f "$MONITOR_PY" ]; then
  echo "[ERROR] monitor.py not found at: $MONITOR_PY"
  echo "Place monitor.py in the same directory as flash.sh."
  exit 1
fi

echo "========================================"
echo "  Sender port:   $SENDER_PORT"
echo "  Receiver port: $RECEIVER_PORT"
echo "  PIO:           $PIO"
echo "  Python:        $PYTHON"
echo "========================================"

# ==================== FUNCTIONS ====================

upload_sender() {
  echo ""
  echo "──────────────────────────────────────"
  echo "[SLAVE] Uploading sender firmware to $SENDER_PORT..."
  echo "──────────────────────────────────────"
  set +e
  "$PIO" run -e slave -t upload --upload-port "$SENDER_PORT"
  local rc=$?
  set -e
  if [ $rc -ne 0 ]; then
    echo "[SLAVE] ✗ Upload FAILED (exit code $rc)"
    return 1
  fi
  echo "[SLAVE] ✓ Upload OK"
}

upload_receiver() {
  echo ""
  echo "──────────────────────────────────────"
  echo "[MASTER] Uploading receiver firmware to $RECEIVER_PORT..."
  echo "──────────────────────────────────────"
  set +e
  "$PIO" run -e master -t upload --upload-port "$RECEIVER_PORT"
  local rc=$?
  set -e
  if [ $rc -ne 0 ]; then
    echo "[MASTER] ✗ Upload FAILED (exit code $rc)"
    return 1
  fi
  echo "[MASTER] ✓ Upload OK"
}

monitor_both() {
  echo ""
  echo "──────────────────────────────────────"
  echo "[MONITOR] Both ports — $SENDER_PORT + $RECEIVER_PORT"
  echo "──────────────────────────────────────"
  "$PYTHON" "$MONITOR_PY" "$SENDER_PORT" "$RECEIVER_PORT"
}

monitor_one() {
  local env=$1
  local port=$2
  echo ""
  echo "──────────────────────────────────────"
  echo "[MONITOR] $env on $port"
  echo "──────────────────────────────────────"
  "$PYTHON" "$MONITOR_PY" "$port"
}

# ==================== MAIN ====================
case "${1:-all}" in
  sender)
    upload_sender && monitor_one SLAVE "$SENDER_PORT"
    ;;
  receiver)
    upload_receiver && monitor_one MASTER "$RECEIVER_PORT"
    ;;
  monitor)
    monitor_both
    ;;
  all)
    upload_sender
    upload_receiver
    monitor_both
    ;;
  *)
    echo "Usage: ./flash.sh [sender|receiver|monitor|all]"
    exit 1
    ;;
esac