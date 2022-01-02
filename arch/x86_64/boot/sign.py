#!/usr/bin/env python3
import sys

BLOCK_SIZE = 512

def main():
    binary = bytearray(open(sys.argv[1], mode='r+b').read())
    if len(binary) > BLOCK_SIZE - 2:
        print("boot block too large:", len(binary), "(max 510)")
        sys.exit(1)

    binary.extend(b'\x00' * (510 - len(binary)))
    binary.extend(b'\x55\xAA')

    with open(sys.argv[1], mode='w+b') as fd:
        fd.write(binary)

if __name__ == "__main__":
    main()
