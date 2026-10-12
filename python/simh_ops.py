#!/usr/bin/env python3
"""Python side of the comparison with SIMH (see ../c/tests/differential/simh_check.py).

Reads and prints the same format as the C simh_ops, so simh_check.py runs it
unchanged:

    python3 ../c/tests/differential/simh_check.py <simh pdp11> python/simh_ops.py 5000 1

Each input line is one case: R0..R7 and the PSW in decimal, then octal
(address value) pairs to deposit.  Each case runs on fresh hardware for one
instruction.  The output line is R0..R7, the PSW, an I/O-page-touched flag,
then the words of the compared memory regions, all octal; an exception is
printed as CRASH.
"""
import logging
import os
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# pylint: disable=wrong-import-position
from pdp11_hardware import Registers, Ram, PSW, Stack, AddressModes
from pdp11_br_ops import br_ops
from pdp11_cc_ops import cc_ops
from pdp11_noopr_ops import noopr_ops
from pdp11_other_ops import other_ops
from pdp11_rss_ops import rss_ops
from pdp11_ss_ops import ss_ops
from pdp11_ssdd_ops import ssdd_ops
from stopwatches import StopWatches

REGIONS = [(0o400, 0o500), (0o1000, 0o1200)]


def run_case(line):
    reg = Registers()
    ram = Ram(threading.Lock(), reg, 16)
    psw = PSW(ram)
    stack = Stack(reg, ram, psw)
    am = AddressModes(reg, ram, psw)
    sw = StopWatches()
    ops = [
        (cc := cc_ops(psw, sw)).is_cc_op, cc.do_cc_op,
        (br := br_ops(reg, ram, psw, sw)).is_br_op, br.do_br_op,
        (no := noopr_ops(reg, ram, psw, stack, sw)).is_noopr_op, no.do_noopr_op,
        (ss := ss_ops(reg, ram, psw, am, sw)).is_ss_op, ss.do_ss_op,
        (rss := rss_ops(reg, ram, psw, am, sw)).is_rss_op, rss.do_rss_op,
        (sd := ssdd_ops(reg, ram, psw, am, sw)).is_ssdd_op, sd.do_ssdd_op,
    ]
    other = other_ops(reg, ram, psw, am, sw)

    # Any access to the I/O page (other than the PSW) is flagged, because
    # SIMH has real device registers there and this hardware has none.
    touched = [0]

    def io_read():
        touched[0] = 1
        return 0

    def io_write(_data):
        touched[0] = 1

    ram.memory[0:ram.io_space] = bytes(ram.io_space)
    for address in range(ram.io_space, psw.psw_address):
        ram.register_io_reader(address, io_read)
        ram.register_io_writer(address, io_write)

    fields = line.split()
    regs = [int(f) for f in fields[:8]]
    start_psw = int(fields[8])
    pairs = fields[9:]
    for i in range(0, len(pairs) - 1, 2):
        ram.write_word(int(pairs[i], 8), int(pairs[i + 1], 8))
    for i, value in enumerate(regs):
        reg.set(i, value)
    psw.set_psw(psw=start_psw)

    # one fetch-decode-execute cycle, dispatched as in PDP11.dispatch_opcode
    instruction = ram.read_word_from_pc()
    for i in range(0, len(ops), 2):
        if ops[i](instruction):
            ops[i + 1](instruction)
            break
    else:
        other.do_other_op(instruction)

    out = [f'{reg.get(i):06o}' for i in range(8)]
    out.append(f'{psw.get_psw() & 0o177777:06o}')
    out.append(str(touched[0]))
    for low, high in REGIONS:
        out += [f'{ram.read_word(a):06o}' for a in range(low, high, 2)]
    return ' '.join(out)


def main():
    logging.disable(logging.CRITICAL)
    for line in sys.stdin:
        try:
            result = run_case(line)
        except Exception as e:  # pylint: disable=broad-except
            result = f'CRASH {type(e).__name__}: {e}'.replace('\n', ' ')
        print(result, flush=True)


if __name__ == '__main__':
    main()
