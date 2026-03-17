"""
Read MAC address directly from each ESP32 chip using esptool.
No sketch needed — reads from hardware eFuse.

Usage:  python scripts/get_mac.py
"""

import subprocess
import re
import sys

PORTS = {
    "SENDER": "COM3",
    "RECEIVER": "COM6",
}

MAC_RE = re.compile(r'MAC:\s*([0-9a-f]{2}(?::[0-9a-f]{2}){5})', re.IGNORECASE)


def get_mac(port):
    try:
        result = subprocess.run(
            ["python", "-m", "esptool", "--port", port, "read_mac"],
            capture_output=True, text=True, timeout=10
        )
        output = result.stdout + result.stderr
        m = MAC_RE.search(output)
        if m:
            return m.group(1).upper(), None
        return None, output.strip()
    except FileNotFoundError:
        return None, "esptool not found. Run: pip install esptool"
    except subprocess.TimeoutExpired:
        return None, "Timed out"
    except Exception as e:
        return None, str(e)


def main():
    print("Reading MAC addresses from ESP32 chips...\n")

    for label, port in PORTS.items():
        mac, err = get_mac(port)
        if mac:
            print(f"  {label} ({port}):  {mac}")
        else:
            print(f"  {label} ({port}):  ERROR - {err}")

    print()


if __name__ == "__main__":
    main()
