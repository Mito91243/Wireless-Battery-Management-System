"""
monitor.py — Read one or two serial ports and print labeled output.

Usage:
    python monitor.py COM3
    python monitor.py COM3 COM4
"""
import sys
import time
import threading
import serial


def read_port(label, port, baud=115200):
    """Continuously read a serial port and print lines with a label."""
    while True:
        try:
            with serial.Serial(port, baud, timeout=1) as ser:
                print(f"[{label}] Connected to {port}", flush=True)
                while True:
                    raw = ser.readline()
                    if raw:
                        line = raw.decode("utf-8", errors="replace").strip()
                        if line:
                            print(f"[{label}] {line}", flush=True)
        except serial.SerialException as e:
            print(f"[{label}] Serial error on {port}: {e}", flush=True)
            print(f"[{label}] Retrying in 2s...", flush=True)
            time.sleep(2)
        except Exception as e:
            print(f"[{label}] Unexpected error: {e}", flush=True)
            time.sleep(2)


def main():
    if len(sys.argv) < 2:
        print("Usage: python monitor.py PORT1 [PORT2]")
        sys.exit(1)

    ports = sys.argv[1:]
    labels = ["SLAVE", "MASTER"]

    threads = []
    for i, port in enumerate(ports):
        label = labels[i] if i < len(labels) else f"PORT{i}"
        t = threading.Thread(target=read_port, args=(label, port), daemon=True)
        t.start()
        threads.append(t)

    print("")
    print("Monitoring... Press Ctrl+C to stop.")
    print("=" * 50)
    print("")

    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n[MONITOR] Stopped.")
        sys.exit(0)


if __name__ == "__main__":
    main()