"""Run the PDP11 on this terminal.

Powers on the PDP11 (which loads the M9301-YA boot ROM and starts at its
console/diagnostics entry point) and runs it on stdin/stdout.
Usage: python3 runpdp11.py [-l listing] [instruction-limit]
  -l listing         also load a program ("address word" lines in octal, the
                     format of source/M9301-YA.txt) and start at its first
                     address
  instruction-limit  default: no limit; Ctrl-C stops
Run it from the directory that holds config_default.json and source/.

FIX: the Python ran the PySimpleGUI VT52 window, whose keyboard input
never reached the DL11.
"""
import argparse
import signal
import sys

from pdp11 import PDP11
from pdp11 import pdp11Run
from pdp11_terminal import raw_input


def main():
    parser = argparse.ArgumentParser(prog='runpdp11')
    parser.add_argument('-l', dest='listing', help='load and start a program listing')
    parser.add_argument('limit', nargs='?', type=int, help='instruction limit')
    args = parser.parse_args()
    if args.limit is not None and args.limit < 0:
        parser.error('instruction-limit must not be negative')

    # instantiate the PDP11
    pdp11 = PDP11()
    if args.listing:
        base = pdp11.boot.read_pdp11_assembly_file(args.listing)
        pdp11.reg.set_pc(base, 'runpdp11 -l')

    restore = raw_input(sys.stdin)
    signal.signal(signal.SIGTERM, lambda signum, frame: sys.exit(128 + signum))
    try:
        pdp11.terminal.keys_from(sys.stdin)
        # power on
        pdp11Run(pdp11).run(args.limit)
    except KeyboardInterrupt:
        return 130
    finally:
        restore()
    return 0


if __name__ == '__main__':
    sys.exit(main())
