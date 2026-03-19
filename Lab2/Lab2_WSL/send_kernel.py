#!/usr/bin/env python3
import struct
import sys
import time

BOOT_MAGIC = 0x544F4F42  # "BOOT"

def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <serial_dev> <kernel_bin>")
        sys.exit(1)

    serial_dev = sys.argv[1]
    kernel_path = sys.argv[2]

    with open(kernel_path, "rb") as f:
        kernel_data = f.read()

    header = struct.pack("<II", BOOT_MAGIC, len(kernel_data))

    with open(serial_dev, "wb", buffering=0) as tty:
        tty.write(header)
        tty.write(kernel_data)

    print(f"sent {len(kernel_data)} bytes to {serial_dev}")

if __name__ == "__main__":
    main()