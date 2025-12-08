import serial
import os
import struct

ser = serial.serial_for_url('socket://127.0.0.1:8888', 115200)

file_size = os.stat('build/kernel8.img').st_size

# '<I' 代表 Little Endian (<), Unsigned Int (I, 4 bytes)
header = struct.pack('<I', file_size)
ser.write(header)
print(ser.readline().decode())