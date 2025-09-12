import time
import sys
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
BAUD = 115200

if __name__ == "__main__":
    ser = serial.Serial(PORT, BAUD, timeout=1)
    i = 0
    try:
        while True:
            lines = [
                f"Mock status #{i}",
                "CPU temp: 42C",
                "GPU load: 12%",
                time.strftime("%H:%M:%S"),
            ]
            # Truncate to 20 chars and join with newline, end with blank line
            norm = [(s[:20] if len(s) > 20 else s) for s in lines]
            payload = ("\n".join(norm) + "\n\n").encode()
            ser.write(payload)
            ser.flush()
            i += 1
            time.sleep(5)
    finally:
        ser.close()