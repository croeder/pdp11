# PDP11 emulator: C translation

A module-for-module C11 translation of the non-GUI parts of
[timberwoof/PDP11](https://github.com/timberwoof/PDP11) (commit `e513026`),
plus its unit tests.  Each `src/pdp11_*.c` corresponds to the Python module of
the same name, and functions keep their Python names, prefixed with the class
(`Ram.read_word` becomes `Ram_read_word`), so the two can be read side by side.

This branch fixes the emulator's instruction bugs: instructions follow the DEC
PDP-11 processor handbook, checked against SIMH (see below).  The tag
`c-translation-faithful` is the earlier translation, which reproduces the
Python exactly, bugs included.  Every change from the Python is marked with a
`FIX:` comment in the source.

## Build and run

Put this `c/` directory in the root of the Python repository.  The programs
open `config_default.json` and `source/M9301-YA.txt` by relative path, as the
Python does, so they run from the repository root.

```
cd c
make            # library, runpdp11, hello_world, test programs
make test       # run all tests
make run        # boot the M9301-YA ROM on this terminal (Ctrl-C to stop)
make hello      # run the Hello, World! program
make simhcheck SIMH=/path/to/simh/BIN/pdp11   # compare with SIMH (needs python3)
make difftest   # compare with the Python (needs python3; differs where bugs were fixed)
```

`build/runpdp11 [-l listing] [limit]` stops after `limit` instructions if
given.  `-l listing` also loads a program from a listing (octal address and
word on each line, the format of `source/M9301-YA.txt`) and starts at its
first address; `../game` uses it to run a game compiled with GCC.  Like the
Python, every run writes `./pdp11.log`.

Requires a C11 compiler and POSIX (pthreads, termios, `open_memstream`).
Tested with gcc and clang on Linux.  The Makefile adds `-D_DARWIN_C_SOURCE`
on macOS, which needs it for `clock_gettime`; the macOS build has not been
tested here.

## What is translated

| Python | C |
|---|---|
| `pdp11_hardware.py` (Registers, Ram, PSW, Stack, AddressModes) | `pdp11_hardware.c` |
| `pdp11_br_ops`, `cc_ops`, `noopr_ops`, `other_ops`, `rss_ops`, `ss_ops`, `ssdd_ops` | same names |
| `pdp11.py` (PDP11, dispatch, `instruction_cycle`, `pdp11Run.run`) | `pdp11.c` |
| `pdp11_boot`, `pdp11_dl11`, `pdp11_m9301`, `pdp11_rk11`, `pdp11_config`, `pdp11_logger`, `pdp11_util`, `stopwatches` | same names |
| `pdp11_terminal.py` | `pdp11_terminal.c`: a working stdin/stdout terminal (see below) |
| `runpdp11.py`, `hello_world.py` | `runpdp11.c`, `hello_world.c` |

Not translated: the PySimpleGUI/tkinter consoles (`pdp11_vt52_Console`,
`pdp11_tek4010_Console`, `Tek_4010`, `M9301_YA_UI`, `pdp11_console`,
`keyreader`) and the threaded `run_with_VT52_emulator` /
`run_with_TEK4010_emulator` loops.  `pdp11Run_run()` with the stdio terminal
takes their place.

## How Python semantics are kept

The translation keeps the Python's structure.  Python integers are unbounded
and signed; every Python int here is an `int64_t` (`pyint`), and `pyint.h`
supplies Python-exact `<<`, `>>`, `//` and `%` for the places that still use
them.  Python's comparisons bind more loosely than its bitwise operators
(`a & b == 0` means `(a & b) == 0`), the reverse of C, so those expressions are
parenthesized explicitly.  `set_psw(mode=-1, ..., psw=-1)` keyword arguments
are written `PSW_set_psw(psw, .n = 1, .z = 0)`; unnamed fields default to -1.
Where the Python would raise (a failed `assert`, for example), the C prints
the Python exception name and stops.

## Bugs fixed

Compared with the Python (and with the `c-translation-faithful` tag):

Registers, memory and addressing
- Registers, the PC, SP and computed addresses wrap at 16 bits.  The Python
  let values go negative or past 0o177777 (`NEG` stored a negative number,
  for example) and asserted on carries.
- Word accesses at an odd address drop bit 0, as the 11/03 (LSI-11) does,
  instead of asserting.
- `JMP`/`JSR` destinations in modes 4, 5 and 7 used the wrong target (an
  extra memory read, and in mode 7 the PC added to the result).
- `MOV`, `CLR` and `SXT` no longer read their destination, and `TST`, `CMP`
  and `BIT` no longer write theirs.  This matters for device registers: the
  Python's `MOV R0,@#RBUF`-style writes first triggered the device's read.

Condition codes and results
- `set_v` set the mode bits instead of V, so INC, DEC, NEG and CMP never set V.
- `ADD`/`SUB`: C now means carry out / borrow; the Python tied C to V and
  missed most carries and every SUB borrow.  `CMP` now sets C.
- `NEG`, `ADC`, `SBC` set N, Z, V and C (the Python set none for ADC and SBC).
- `ROR`/`ROL` rotate through C; the byte versions change only the low byte.
  `ASR`/`ASL` byte versions change only the low byte, and `ASL` sets C (the
  Python tested bit 45).  `ASLB` no longer raises TypeError.
- `SXT` stores -1 (0o177777), not 1, and clears V.
- `MOVB` to a register sign-extends into the high byte.
- `BIT` and `CMP` compute the right value and leave the destination alone.
- `BGE` tests N xor V (the Python tested N or V).
- The set-condition-code instructions set only the selected bits.

EIS, XOR, SOB
- `MUL` sets C when the product doesn't fit in 16 bits.
- `DIV` truncates toward zero with the remainder taking the dividend's sign;
  divide by zero and quotient overflow set V (and C for zero) and leave the
  registers unchanged.  The Python tested the numerator for zero, raised on a
  zero divisor, and its quotient correction was wrong when both operands
  were negative or the division was exact.
- `ASH` and `ASHC` implement the signed shift count, V and C; the Python's
  ASHC shifted right by the whole source value.  All four fetch a word
  operand (the Python fetched a byte).
- `XOR` writes its destination, not the register, reads the register before
  evaluating the destination, and clears V.
- `SOB` decrements and branches (the Python multiplied).

Traps and control
- `BPT` and `IOT` trap through vectors 14 and 20 (push PS and PC, load both
  from the vector).  The Python's IOT raised AttributeError, set PC to 20
  itself and put 22 in SP; BPT just stopped.
- `RTT` pops PC and PS like RTI; the Python popped the PS into SP.
- `WAIT` stops without skipping the next word (there are no interrupts to
  wait for).
- `JSR SP,dst` pushes the decremented SP, as SIMH does.
- `JMP`/`JSR` to a register (illegal) stop the CPU without jumping to 0.
- `run()` counted every instruction twice; the "Halting" log line on a jump
  to itself no longer claims to halt.

### Not implemented (unchanged from the Python)

Interrupts; the EMT and TRAP instructions; traps for illegal and reserved
instructions (the CPU stops instead); MARK, MTPS, MFPS, MFPI, MTPI, MFPD and
MTPD (they do nothing); and the PSW is not writable through address 0o177776.

## Where the Python cannot run, and what the C does instead

- `PDP11.__init__` uses `threading` without importing it, `instruction_cycle`
  reads `self.pdp11.CPU_cycles`, and `run()` calls a `terminal.window_cycle()`
  that does not exist.  The C uses the object's own `CPU_cycles` and calls
  `Terminal_cycle()`.
- `Terminal.cycle()` uses an undefined `newchar` and would print the
  character's number, and the Terminal had no keyboard input.  The C terminal
  writes transmitted characters to stdout and delivers keystrokes from stdin
  to the DL11 receiver (stdin is put in no-echo, character-at-a-time mode and
  restored on exit).
- `Config` logs an undefined `configFilePath` on a JSON error; the C logs the
  file name.

## Tests

`tests/` ports the 11 test files the repository's GitHub workflow runs, plus
`test_config`: 179 tests, run in the same order with the same shared state
between tests (pytest class attributes become static globals).

Expectations that encoded the bugs above were changed to DEC's behavior, each
with a comment giving the Python's old expected value: the MOVB
sign-extension cases, the ADD/SUB carry cases, the MUL carry cases, the DIV
case with two negative operands (the test's own reference formula was wrong),
the ROR/RORB/ROLB/ASRB cases, and JMP to a register.  Each changed value
was run in SIMH with the test's exact inputs, and all 24 match.

Three whole-machine tests cannot run in the Python repository as committed
(importing `pdp11` pulls in PySimpleGUI, and `PDP11()` fails on the missing
import).  Their C versions run: `test_DL11` (same assertions), `test_implemented`
(no assertions, as in Python), and `test_hello_world`, which in Python only
opens the VT52 window; here it captures the terminal output and asserts the
message was printed and the CPU halted.

Tests named `not_test_*` (skipped by pytest) are not ported.

## Checking against SIMH

`make simhcheck SIMH=...` uses [SIMH](https://github.com/open-simh/simh)'s
PDP-11 simulator, set up as an 11/03 with EIS, as an independent reference.
It generates random single-instruction cases (every implemented instruction
group, all addressing modes, registers pointing into a test region), runs
each in SIMH and in the C, and compares R0-R7, NZVC, and two regions of
memory.  Not compared: register-mode JMP/JSR (SIMH traps, the C stops), cases
that reach the I/O page (SIMH has real devices there), and operands using
`-(PC)` or `@-(PC)`, which DEC documents as unpredictable.

Two runs of 5,000 cases (seeds 1 and 2) compared 9,520 cases with no
mismatches.  While the fixes were being made, this check found three
problems the handbook reading alone had missed: odd word addresses,
`JSR SP,dst`, and the order in which `XOR` reads its register.

The M9301-YA boot ROM was also run in both, with the same keystrokes
(`L 2000`, `E`, `D 123456`, `E`) typed at its console.  The output was
identical up to the point where SIMH stopped with a trap-stack abort (with SP
at 4) partway through the `D` command; that looks like SIMH's stack-limit
check, which the 11/03 doesn't have, but I didn't confirm it.  In both, the
register display now prints six-digit octal (the Python printed `0 0 0 0`),
and `E` prints no value, so that is the ROM's behavior rather than an
emulator fault.

## Differential test against the Python

`make difftest` runs random single instructions through this C and the
Python and compares the results.  On the `c-translation-faithful` tag it
reports no mismatches (8,000 cases); on this branch it reports the cases
where the fixes above change the outcome.
