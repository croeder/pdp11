"""Compare single-instruction behaviour of the C emulator with SIMH.

SIMH's PDP-11 simulator (https://github.com/open-simh/simh), configured as
an 11/03 with the EIS option, serves as an independent reference.  Random
cases are generated here (independently of both emulators), run in SIMH and
in the C emulator (simh_ops), and compared: R0-R7, the NZVC bits, and the
memory words at 0o400-0o476 and 0o1000-0o1176.  Generated values stay
below the I/O page (0o160000), where SIMH has device registers and the test
fixture has none.

    make simhcheck SIMH=/path/to/simh/BIN/pdp11 [SIMH_CASES=3000] [SIMH_SEED=1]

Not compared: register-mode JMP/JSR (SIMH traps through vector 4; the C
emulator stops instead),
cases that touch the I/O page (SIMH has device registers there), and
operands using -(PC) or @-(PC), whose behaviour DEC calls unpredictable.  WAIT, RESET and the instructions the C leaves
unimplemented (MARK, MTPS, MFPS, MFPI, MTPI, MFPD, MTPD, EMT, TRAP) are not
generated.
"""
import random
import re
import subprocess
import sys
import tempfile

REGION = range(0o1000, 0o1200, 2)        # data region: registers point here
CODE = 0o400                             # instruction and its two extra words
COMPARED = list(range(0o400, 0o500, 2)) + list(REGION)
VECTORS = {0o4: 0o7000, 0o6: 0, 0o10: 0o7010, 0o12: 0,
           0o14: 0o5014, 0o16: 0o5, 0o20: 0o5020, 0o22: 0o12}   # BPT and IOT load these


def random_instruction(rng):
    kind = rng.randrange(9)
    if kind == 0:   # branches
        return rng.choice([0o000400, 0o001000, 0o001400, 0o002000, 0o002400, 0o003000, 0o003400,
                           0o100000, 0o100400, 0o101000, 0o101400, 0o102000, 0o102400,
                           0o103000, 0o103400]) | rng.randrange(0o400)
    if kind == 1:   # condition codes
        return 0o240 + rng.randrange(0o40)
    if kind == 2:   # no-operand, except WAIT and RESET
        return rng.choice([0, 2, 3, 4, 6])
    if kind in (3, 4):  # single operand
        word = [0o000100, 0o000300] + list(range(0o005000, 0o006400, 0o100)) + [0o006700]
        byte = list(range(0o105000, 0o106400, 0o100))
        return rng.choice(word + byte) | rng.randrange(0o100)
    if kind == 5:   # EIS, XOR, SOB
        return rng.choice([0o070000, 0o071000, 0o072000, 0o073000, 0o074000, 0o077000]) | rng.randrange(0o1000)
    if kind in (6, 7):  # double operand
        return rng.choice([0o010000, 0o020000, 0o030000, 0o040000, 0o050000, 0o060000]) \
            | rng.choice([0, 0o100000]) | rng.randrange(0o10000)
    return rng.choice([0o000200 | rng.randrange(8), 0o004000 | rng.randrange(0o1000)])


def random_word(rng):
    return rng.choice(REGION) if rng.random() < 0.5 else rng.randrange(0o160000)


def make_case(rng):
    regs = [rng.choice(REGION) if rng.random() < 0.7 else rng.randrange(0o160000) for _ in range(6)]
    regs.append(rng.randrange(0o1100, 0o1200, 2))     # SP, near the top of the region
    regs.append(CODE)                                  # PC
    psw = rng.randrange(0o20)                          # NZVC
    memory = dict(VECTORS)
    memory[CODE] = random_instruction(rng)
    for a in (CODE + 2, CODE + 4):                     # index words / immediates
        memory[a] = rng.randrange(0, 0o100, 2) if rng.random() < 0.6 else rng.randrange(0o160000)
    for a in REGION:
        memory[a] = random_word(rng)
    return regs, psw, memory


def run_simh(simh, cases):
    lines = ['set cpu 11/03', 'set cpu eis']
    for i, (regs, psw, memory) in enumerate(cases):
        lines.append(f'echo @@CASE {i}')
        lines.append('reset')          # clear any trap or trace request left by the last case
        lines.append('d 0-157776 0')   # clear memory below the I/O page
        for a, v in memory.items():
            lines.append(f'd {a:o} {v:o}')
        for r, v in enumerate(regs[:6]):
            lines.append(f'd r{r} {v:o}')
        lines.append(f'd sp {regs[6]:o}')
        lines.append(f'd psw {psw:o}')
        lines.append(f'd pc {regs[7]:o}')
        lines.append('s')
        if memory[CODE] in (0o3, 0o4):
            # SIMH takes a BPT or IOT trap at the start of the next step; that
            # step also runs the HALT at the handler, leaving PC = handler + 2.
            lines.append('s')
        lines.append('e r0,r1,r2,r3,r4,r5,sp,pc,psw')
        lines.append('e 400-476')
        lines.append('e 1000-1176')
    lines.append('exit')
    with tempfile.NamedTemporaryFile('w', suffix='.do', delete=False) as f:
        f.write('\n'.join(lines) + '\n')
        script = f.name
    out = subprocess.run([simh, script], capture_output=True, text=True).stdout
    results = {}
    for chunk in out.split('@@CASE ')[1:]:
        number, _, body = chunk.partition('\n')
        values = dict(re.findall(r'^(\w+):\s+([0-7]+)', body, re.M))
        regs = [int(values[k], 8) for k in ('R0', 'R1', 'R2', 'R3', 'R4', 'R5', 'SP', 'PC')]
        psw = int(values['PSW'], 8)
        mem = [int(values[f'{a:o}'], 8) for a in COMPARED]
        results[int(number)] = (regs, psw, mem)
    return results


def run_c(c_binary, cases):
    lines = []
    for regs, psw, memory in cases:
        deposits = ' '.join(f'{a:o} {v:o}' for a, v in memory.items())
        lines.append(' '.join(str(r) for r in regs) + f' {psw} ' + deposits)
    out = subprocess.run([c_binary], input='\n'.join(lines) + '\n', capture_output=True, text=True).stdout
    results = []
    for line in out.splitlines():
        if line.startswith('CRASH'):
            results.append(line)
            continue
        fields = [int(x, 8) for x in line.split()]
        results.append((fields[:8], fields[8], fields[10:], fields[9]))
    return results


def describe(instruction):
    return f'{instruction:06o}'


def main():
    simh, c_binary = sys.argv[1], sys.argv[2]
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 3000
    seed = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    rng = random.Random(seed)
    cases = [make_case(rng) for _ in range(count)]
    reference = run_simh(simh, cases)
    actual = run_c(c_binary, cases)

    compared = skipped = mismatches = pc_decrement = io_page = 0
    by_opcode = {}
    for i, (regs, psw, memory) in enumerate(cases):
        s_regs, s_psw, s_mem = reference[i]
        if memory[CODE] in (0o3, 0o4):
            s_regs = s_regs[:7] + [(s_regs[7] - 2) & 0o177777]   # undo the handler's HALT
        instruction = memory[CODE]
        if (instruction & 0o177770) == 0o000100 or (instruction & 0o177070) == 0o004000:
            skipped += 1          # JMP/JSR to a register: SIMH traps (on the next step); the C stops
            continue
        c = actual[i]
        instruction = memory[CODE]
        specifiers = [instruction & 0o77]
        if (instruction & 0o070000) not in (0, 0o070000):
            specifiers.append((instruction >> 6) & 0o77)
        if any(spec in (0o47, 0o57) for spec in specifiers):
            pc_decrement += 1     # -(PC) or @-(PC): unpredictable, model-dependent
            continue
        if not isinstance(c, str) and c[3]:
            io_page += 1          # touched the I/O page
            continue
        compared += 1
        if isinstance(c, str):
            problem = c
        else:
            c_regs, c_psw, c_mem, _ = c
            problem = None
            if c_regs != s_regs:
                problem = f'regs simh={[oct(x) for x in s_regs]} c={[oct(x) for x in c_regs]}'
            elif (c_psw & 0o17) != (s_psw & 0o17):
                problem = f'nzvc simh={s_psw & 0o17:04b} c={c_psw & 0o17:04b}'
            elif c_mem != s_mem:
                diffs = [(oct(a), oct(sv), oct(cv)) for a, sv, cv in zip(COMPARED, s_mem, c_mem) if sv != cv]
                problem = f'memory (address, simh, c) {diffs[:4]}'
        if problem:
            mismatches += 1
            key = memory[CODE] & 0o177700
            by_opcode.setdefault(key, []).append((describe(memory[CODE]), regs, psw, problem))

    for key in sorted(by_opcode):
        examples = by_opcode[key]
        print(f'opcode group {key:06o}: {len(examples)} mismatches, e.g. instruction {examples[0][0]}')
        print(f'    start regs {[oct(x) for x in examples[0][1]]} nzvc {examples[0][2]:04b}')
        print(f'    {examples[0][3]}')
    print(f'{compared} cases compared, {mismatches} mismatches.  Not compared: {skipped} register-mode '
          f'JMP/JSR (SIMH traps, the C stops), {io_page} that touched the I/O page, '
          f'{pc_decrement} using -(PC) or @-(PC)')
    sys.exit(1 if mismatches else 0)


if __name__ == '__main__':
    main()
