import serial
import os
import struct
import sys      # [補上] 缺少這個會無法使用 sys.exit
import select   # [補上] 缺少這個會無法使用 select

try:
    ser = serial.serial_for_url('socket://127.0.0.1:8888', 115200)
except Exception as e:
    print(f"Failed to connect to QEMU: {e}")
    print("Check: Is 'make qemu-gdb' running?")
    sys.exit(1)

retry_count = 0 
max_retries = 50

# ---------------------------------------------------------
# 主要邏輯區塊 (這是一整個大的 try，用來接住 KeyboardInterrupt)
# ---------------------------------------------------------
try:
    # 1. 等待 Bootloader 回應
    while True:
        try:
            line = ser.readline().decode('utf-8', errors='ignore')
        except OSError:
            pass
            
        if line:
            print(f"[Raw Output]: {line.strip()}") 
            
        if "OSDI: Ready" in line:
            print("Device is ready! Starting transmission.")
            break
            
        if not line:
            retry_count += 1
            if retry_count > max_retries:
                print("Error: Timed out waiting for device ready signal.")
                sys.exit(1)

    # 2. 傳送檔案
    file_size = os.stat('build/kernel8.img').st_size
    header = struct.pack('<I', file_size)
    ser.write(header)
    
    # 讀取大小確認
    response = ser.readline().decode().strip()
    print(f"Size Sent. Device replied: {response}")

    # 傳送內容
    with open('build/kernel8.img', 'rb') as f:
        print("Sending kernel image...")
        ser.write(f.read())
        print("Kernel image sent successfully.")

    print("------------------------------------------------")
    print("Entering interactive mode (Ctrl+C to exit)...")
    print("------------------------------------------------")

    # 3. 互動模式 (不需要額外的 try，直接接著寫)
    while True:
        # 監聽 Serial 和 鍵盤
        rlist, _, _ = select.select([ser.fileno(), sys.stdin.fileno()], [], [])

        for fd in rlist:
            if fd == ser.fileno():
                data = ser.read(ser.in_waiting or 1)
                if data:
                    print(data.decode('utf-8', errors='ignore'), end='', flush=True)
                else:
                    print("\n[Connection closed by device]")
                    sys.exit(0)

            elif fd == sys.stdin.fileno():
                user_input = sys.stdin.readline()
                ser.write(user_input.encode())

except KeyboardInterrupt:
    print("\nExiting...")

except Exception as e:
    print(f"\n[Error]: An exception occurred: {e}")

finally:
    ser.close()