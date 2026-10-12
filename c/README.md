# PDP11 emulator: C translation

A module-for-module C11 translation of the non-GUI parts of
[timberwoof/PDP11](https://github.com/timberwoof/PDP11) (commit `e513026`),
plus its unit tests.  Each `src/pdp11_*.c` corresponds to the Python module of
the same name, and functions keep their Python names, prefixed with the class
(`Ram.read_word` becomes `Ram_read_word`), so the two can be read side by side.

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
make difftest   # compare against the Python (needs python3)
```

`build/runpdp11 [limit]` stops after `limit` instructions if given.  Like the
Python, every run writes `./pdp11.log`.

Requires a C11 compiler and POSIX (pthreads, termios, `open_memstream`).
Tested with gcc on Linux.

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

Python integers are unbounded and signed, and the emulator depends on that:
results are often left unmasked, `NEG` can put a negative number in a
register, and memory can be indexed with a negative address (a `bytearray`
counts back from the end).  Every Python int here is an `int64_t` (`pyint`),
and `pyint.h` supplies Python-exact `<<`, `>>`, `//` and `%`.  Two other
details that change results:

- Python's comparisons bind more loosely than its bitwise operators
  (`a & b == 0` means `(a & b) == 0`), the reverse of C, so those expressions
  are parenthesized explicitly.
- `set_psw(mode=-1, ..., psw=-1)` keyword arguments are written
  `PSW_set_psw(psw, .n = 1, .z = 0)`; unnamed fields default to -1.

When the Python would raise (a failed `assert`, `ZeroDivisionError`,
`IndexError`, ...), the C prints the Python exception name and stops.

## Python behaviour reproduced as written

These look like bugs in the emulator, but changing them would make this a
rewrite rather than a translation, so the C does what the Python does:

- `PSW.set_v` calls `self.set_psw(v)`, so `v` lands in the first parameter,
  `mode`.  INC, DEC, NEG and CMP therefore rewrite the mode bits and never set V.
- `BGE` branches on `(N | V) == 0` rather than `N ^ V`.
- The set-condition-code instructions replace NZVC instead of OR-ing bits in.
- `ROL` byte uses `0o0000000010000000` and `ASL` uses `0o1000000000000000`
  as single-bit masks; both are octal, so they test bits 21 and 45 (ASL never sets C).
- `SXT` stores 1, not -1; word `ROR` takes C from the result; `BIT` writes its
  result to the destination; `CMP` writes the destination back.
- `DIV` checks the numerator for zero; a zero denominator raises.  `ASH` takes
  the "register sign" from the register number.  `ASHC` shifts by the whole
  source; `SOB` multiplies.
- `IOT` loads 0o22 into SP rather than PS.  RSS instructions fetch the source
  in byte mode.
- `run()` counts each instruction twice in `CPU_cycles`;
  `instruction_cycle` logs "Halting" when the PC did not change but continues.

## Where the Python cannot run, and what the C does instead

- `PDP11.__init__` uses `threading` without importing it, `instruction_cycle`
  reads `self.pdp11.CPU_cycles`, and `run()` calls a `terminal.window_cycle()`
  that does not exist.  The C uses the object's own `CPU_cycles` and calls
  `Terminal_cycle()`.
- `IOT` pushes `self.psw.PSW` (no such attribute): the C pushes the PSW value.
  `RTT` calls `psw.set_condition_codes` (no such method): the C omits the call.
- `ASLB` is missing an operator (`... & MASK_HIGH_BYTE (lowbyte << 1) ...`,
  a TypeError): the C joins the halves with `|`.
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
between tests (pytest class attributes become static globals).  All pass, as
they do in Python.

Three whole-machine tests cannot run in the Python repository as committed
(importing `pdp11` pulls in PySimpleGUI, and `PDP11()` fails on the missing
import).  Their C versions run: `test_DL11` (same assertions), `test_implemented`
(no assertions, as in Python), and `test_hello_world`, which in Python only
opens the VT52 window; here it captures the terminal output and asserts the
message was printed and the CPU halted.

Tests named `not_test_*` (skipped by pytest) are not ported.

## Differential test against the Python

`make difftest` runs random single instructions from every instruction group
through both implementations, starting from the same registers, PSW and
pseudo-random memory, and compares the registers, PSW, run flag, disassembly
string and a checksum of memory.  Cases where the Python raises are skipped.
Runs of 3,000 and 5,000 cases (seeds 1 and 7) gave no mismatches; a
deliberately planted one-character change to `BGE` produced 9.

The whole machine was also compared by running a minimally patched copy of
the Python (the missing import and attribute fixed, GUI imports removed)
headless for 200,000 instructions of the M9301-YA boot ROM: final registers,
PSW, cycle count, terminal output, and the executed-instruction report were
identical, including after typing `L 2000` and `E` at the ROM console.
That run took 0.5 s in C and 6.3 s in Python, mostly spent writing the log.
