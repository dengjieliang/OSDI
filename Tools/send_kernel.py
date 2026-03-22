import serial
import os
import struct
import sys
import select
import time

try:
    # 連接 QEMU
    ser = serial.serial_for_url('socket://127.0.0.1:8888', 115200)
except Exception as e:
    print(f"Failed to connect to QEMU: {e}")
    sys.exit(1)

retry_count = 0 
max_retries = 50

try:
    print("Listening for Bootloader...")
    
    # 1. 等待 Bootloader 回應
    while True:
        line = None  # [修正] 每次迴圈開始前，先將 line 重置為 None
        try:
            # 嘗試讀取一行
            raw = ser.readline()
            if raw:
                line = raw.decode('utf-8', errors='ignore')
        except OSError:
            pass
            
        if line:
            print(f"[Device]: {line.strip()}") 
            if "OSDI: Ready" in line:
                print("-> Device is ready! Starting transmission.")
                break
        
        # 簡單的 Timeout 機制 (這裡僅作示範，可依需求調整)
        # time.sleep(0.1) 

    # 2. 傳送檔案
    kernel_path = 'build/kernel8.img'
    file_size = os.stat(kernel_path).st_size
    header = struct.pack('<I', file_size)
    
    print(f"Sending kernel size: {file_size} bytes")
    ser.write(header)
    
    # 讀取大小確認 (這裡會等待你的 uart_send_hex 回傳)
    # [注意] 如果你的 C 語言 uart_send_hex 沒有換行，這裡可能會黏在一起，但通常不至於報錯
    response = ser.readline().decode('utf-8', errors='ignore').strip()
    print(f"Size Sent. Device replied: {response}")

    # 傳送內容
    with open(kernel_path, 'rb') as f:
        print("Sending kernel image...")
        ser.write(f.read())
        print("Kernel image sent successfully.")

    print("------------------------------------------------")
    print("Entering interactive mode (Ctrl+C to exit)...")
    print("------------------------------------------------")

    # 3. 互動模式
    while True:
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
    print(f"\n[Error]: {e}")
finally:
    ser.close()