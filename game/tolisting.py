#!/usr/bin/env python3
"""Turn a raw PDP-11 memory image into the listing format the emulators
load: one "address word" pair per line, both in octal, the format of
source/M9301-YA.txt.  The emulators start at the first address.

    python3 tolisting.py image.bin 01000 > program.lst

image.bin is what `pdp11-aout-objcopy -O binary` writes: the program's bytes
from its load address up, low byte of each word first.
"""
import sys


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    image = open(sys.argv[1], 'rb').read()
    base = int(sys.argv[2], 8)
    if base % 2:
        sys.exit('the load address must be even')
    if len(image) % 2:
        image += b'\0'
    if base + len(image) > 0o160000:
        sys.exit(f'{len(image)} bytes from 0o{base:o} run into the I/O page at 0o160000')
    print(f'; {sys.argv[1]}: {len(image)} bytes at 0o{base:o}')
    for i in range(0, len(image), 2):
        word = image[i] | image[i + 1] << 8
        print(f'{base + i:06o} {word:06o}')


if __name__ == '__main__':
    main()
