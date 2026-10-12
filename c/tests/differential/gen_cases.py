"""Differential test, Python side.

Runs random single instructions through the Python emulator's hardware and
instruction classes (dispatched in the same order as pdp11.py) and writes the
starting state and the Python's resulting state to a case file.  The C program
diff_ops replays each case and compares.

Run from the repository root:
    python3 c/tests/differential/gen_cases.py [count] [seed] > cases.txt

Cases where the Python raises (an assert, TypeError, AttributeError,
ZeroDivisionError, ...) are skipped, since those are crashes, not behaviour.
"""
import logging
import random
import sys
import threading

sys.path.insert(0, '.')
from pdp11_hardware import Registers, Ram, PSW, Stack, AddressModes
from pdp11_br_ops import br_ops
from pdp11_cc_ops import cc_ops
from pdp11_noopr_ops import noopr_ops
from pdp11_other_ops import other_ops
from pdp11_rss_ops import rss_ops
from pdp11_ss_ops import ss_ops
from pdp11_ssdd_ops import ssdd_ops
from stopwatches import StopWatches

logging.disable(logging.CRITICAL)

MEMORY_TOP = 0o160000      # fill and compare everything below the I/O page
INSTRUCTION_ADDRESS = 0o400


def lcg_fill(ram, seed):
    """fill memory below the I/O page from a simple LCG (same as diff_ops.c)"""
    x = seed & 0x7fffffff
    for address in range(0, MEMORY_TOP):
        x = (x * 1103515245 + 12345) & 0x7fffffff
        ram.memory[address] = (x >> 16) & 0xff


def checksum(ram):
    total = 0
    for address in range(0, MEMORY_TOP):
        total = (total * 31 + ram.memory[address] + address) & 0xffffffffffff
    return total


def random_instruction(rng):
    kind = rng.randrange(8)
    if kind == 0:    # branches
        return rng.choice([0o000400, 0o001000, 0o001400, 0o002000, 0o002400, 0o003000, 0o003400,
                           0o100000, 0o100400, 0o101000, 0o101400, 0o102000, 0o102400,
                           0o103000, 0o103400]) | rng.randrange(0o400)
    if kind == 1:    # condition codes
        return 0o240 + rng.randrange(0o40)
    if kind == 2:    # no-operand
        return rng.randrange(7)
    if kind == 3:    # single operand
        return rng.choice([0o000100, 0o000300] + list(range(0o005000, 0o007000, 0o100)) +
                          list(range(0o105000, 0o107000, 0o100))) | rng.randrange(0o100)
    if kind == 4:    # register-source
        return rng.choice([0o070000, 0o071000, 0o072000, 0o073000, 0o074000, 0o077000]) | rng.randrange(0o1000)
    if kind in (5, 6):  # double operand
        return rng.choice([0o010000, 0o020000, 0o030000, 0o040000, 0o050000, 0o060000]) \
            | rng.choice([0, 0o100000]) | rng.randrange(0o10000)
    return rng.choice([0o000200 | rng.randrange(8), 0o004000 | rng.randrange(0o1000),
                       0o006400 | rng.randrange(0o100)])


def random_register(rng, index):
    if index == 7:
        return INSTRUCTION_ADDRESS + 2
    if rng.random() < 0.7:
        return rng.randrange(0o1000, 0o150000, 2)   # a plausible even address
    return rng.randrange(0o200000)


def run_case(rng, seed):
    reg = Registers()
    ram = Ram(threading.Lock(), reg, 16)
    psw = PSW(ram)
    stack = Stack(reg, ram, psw)
    am = AddressModes(reg, ram, psw)
    sw = StopWatches()
    br = br_ops(reg, ram, psw, sw)
    cc = cc_ops(psw, sw)
    noopr = noopr_ops(reg, ram, psw, stack, sw)
    other = other_ops(reg, ram, psw, am, sw)
    rss = rss_ops(reg, ram, psw, am, sw)
    ss = ss_ops(reg, ram, psw, am, sw)
    ssdd = ssdd_ops(reg, ram, psw, am, sw)

    memseed = rng.randrange(1, 0x7fffffff)
    lcg_fill(ram, memseed)
    instruction = random_instruction(rng)
    ram.write_word(INSTRUCTION_ADDRESS, instruction)
    registers = [random_register(rng, i) for i in range(8)]
    for i, value in enumerate(registers):
        reg.set(i, value)
    start_psw = rng.randrange(0o400) if rng.random() < 0.8 else rng.randrange(0o1000000)
    psw.set_psw(psw=start_psw)

    # same order as PDP11.dispatch_opcode
    if cc.is_cc_op(instruction):
        result = cc.do_cc_op(instruction)
    elif br.is_br_op(instruction):
        result = br.do_br_op(instruction)
    elif noopr.is_noopr_op(instruction):
        result = noopr.do_noopr_op(instruction)
    elif ss.is_ss_op(instruction):
        result = ss.do_ss_op(instruction)
    elif rss.is_rss_op(instruction):
        result = rss.do_rss_op(instruction)
    elif ssdd.is_ssdd_op(instruction):
        result = ssdd.do_ssdd_op(instruction)
    else:
        result = other.do_other_op(instruction)
    run, operand1, operand2, assembly, report = result

    after = [reg.get(i) for i in range(8)]
    return (memseed, instruction, start_psw, registers,
            int(bool(run)), assembly, after, psw.psw, checksum(ram))


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    rng = random.Random(seed)
    written = skipped = 0
    while written < count:
        state = rng.getstate()
        try:
            case = run_case(rng, seed)
        except Exception:   # a Python crash; not comparable behaviour
            skipped += 1
            continue
        memseed, instruction, start_psw, registers, run, assembly, after, end_psw, mem = case
        print(memseed, instruction, start_psw, *registers, run, *after, end_psw, mem,
              assembly.replace(' ', '_') or '-')
        written += 1
    print(f'# {written} cases written, {skipped} skipped because the Python raised', file=sys.stderr)


if __name__ == '__main__':
    main()
