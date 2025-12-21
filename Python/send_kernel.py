import serial
import os
import struct

ser = serial.serial_for_url('socket://127.0.0.1:8888', 115200)

retry_count = 0 
max_retries = 50

while True:
    try:
        # 讀取一行 (因為有 timeout，這裡最多卡 1 秒就會回傳)
        line = ser.readline().decode('utf-8', errors='ignore')
        
        # [DEBUG] 只要收到任何東西，立刻印出來！
        if line:
            print(f"[Raw Output]: {line.strip()}") 
            
        if "OSDI: Ready" in line:
            print("Device is ready! Starting transmission.")
            break
        
        # 沒收到東西，繼續等
        if not line:
            # 如果是 socket 剛連上可能還沒吐資料，允許空轉一下
            retry_count += 1
            if retry_count > max_retries:
                print("Error: Timed out waiting for device ready signal.")
                print("Check: 1. Is the kernel running? 2. Is baudrate correct?")
                exit(1)
                
    except OSError:
        pass

file_size = os.stat('build/kernel8.img').st_size

# '<I' 代表 Little Endian (<), Unsigned Int (I, 4 bytes)
header = struct.pack('<I', file_size)
ser.write(header)
print(ser.readline().decode())

# [新增] 打開 kernel8.img 並傳送內容
with open('build/kernel8.img', 'rb') as f:
    print("Sending kernel image...")
    ser.write(f.read())  # 這行才是真正把 Kernel 送過去！
    print("Kernel image sent successfully.")

# [選用] 繼續讀取 Bootloader 傳回來的 "Jump" 訊息，確認有跳轉
while True:
    try:
        line = ser.readline().decode('utf-8', errors='ignore')
        if line:
            print(f"[Device]: {line.strip()}")
    except:
        print(except)
        break