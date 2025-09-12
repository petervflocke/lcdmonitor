import time

if __name__ == "__main__":
    counter = 0
    while True:
        # Mock serial output, 3 lines of 20 chars
        lines = [f"Line {i} {counter:04d}".ljust(20) for i in range(3)]
        payload = "\n".join(lines)
        print(payload)  # Replace with pyserial write
        counter += 1
        time.sleep(5)
