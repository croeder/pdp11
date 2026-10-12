# PDP-11/03 emulator in Python, fixed

A copy of the Python emulator from the repository root with the same bugs
fixed as in the C version (`../c`), so that it runs programs the same way
the C, Go and Rust versions do and gives the same results as SIMH. The
modules keep their names, classes and structure; each change is marked with
a `FIX:` comment saying what the original did. The originals at the root
are unchanged.

Standard library only. The PySimpleGUI consoles are not in this folder: the
machine talks to the terminal you run it in, through stdin and stdout.

As with the other versions, run the programs from the repository root: the
emulator reads `config_default.json` and `source/M9301-YA.txt` relative to
the current directory, and writes `pdp11.log` there.

    python3 python/runpdp11.py [-l listing] [instruction-limit]
    python3 python/hello_world.py
    cd python && python3 -m pytest

`runpdp11.py` boots the M9301-YA ROM console. At the `$` prompt it takes
`L addr` (load address), `E ` (examine, E then space), `D value` (deposit)
and `S` (start), each ended with Return. With `-l` it also loads a program
listing (an octal address and word on each line) and starts at its first
address; `../game/run.sh python` uses this to run the Rogue-like game.
Ctrl-C stops it and restores the terminal.

## What changed

**The bugs fixed in the C.** The full list, with what each instruction did
before, is in `../c/README.md` under "Bugs fixed". In short:

- Registers, the PC, SP and computed addresses wrap at 16 bits, and word
  accesses at an odd address drop bit 0, as on the 11/03.
- Addressing: JMP and JSR in modes 4, 5 and 7 went to the wrong address.
  MOV, CLR and SXT no longer read their destination, and TST, CMP and BIT no
  longer write theirs, which matters for device registers.
- Condition codes: `set_v` set the mode bits instead of V. ADD and SUB carry
  and borrow, CMP's C, NEG, ADC and SBC were all wrong or missing. ROR, ROL,
  ASR and ASL rotate through C and the byte versions change only the low
  byte. SXT stores -1, MOVB to a register sign-extends, BGE tests N xor V,
  and the set-condition-code instructions set only the selected bits.
- EIS: MUL sets C; DIV truncates toward zero and handles zero divisors and
  overflow; ASH and ASHC use a signed shift count; all four take a word
  operand. XOR writes its destination; SOB decrements and branches.
- Traps and control: BPT and IOT trap through vectors 14 and 20, RTT
  behaves like RTI, WAIT doesn't skip a word, `JSR SP` pushes the
  decremented SP, and JMP or JSR to a register stops the CPU instead of
  jumping to 0.

The address-mode logic is now in one place, `AddressModes.resolve`, which
the get, set, address and jump helpers share, as in the C.

**What stopped the Python running at all.**

- `pdp11.py` used `threading` without importing it, and imported the
  PySimpleGUI, multiprocessing and RK11 modules whether or not they were used.
- `instruction_cycle` built a debug line from `self.pdp11.CPU_cycles`, which
  doesn't exist, on every instruction.
- `run()` called `terminal.window_cycle()`, which Terminal doesn't have,
  counted every instruction twice, and stopped after 100,000 by default.
- `Terminal.cycle()` used an undefined `newchar` and would have printed the
  character code as a number, and nothing read the keyboard.
- `Config` named an undefined variable when the JSON was bad.

**The terminal** (`pdp11_terminal.py`) is new. It writes each byte the DL11
transmits to stdout. A thread reads stdin into a queue, and after each
instruction, when the DL11 receiver is empty, the terminal moves one key
into it. When stdin is a terminal, it's put in character-at-a-time mode
without echo, with Return arriving as CR, and restored on exit.

**Logging.** If `pdp11.log` can't be opened, logging is turned off instead
of the program stopping. The game's run script relies on this, as it does
for the C and Go versions.

## Verification

- **pytest:** 184 tests pass. They are the original tests with the C
  version's expectations, where the original expected the buggy result
  (each such line has a FIX comment). `test_hello_world` now runs the
  program with its output captured and checks it, where the original opened
  the VT52 window and checked nothing. `conftest.py` runs the tests from the
  repository root. The root's GUI, M9301, echo and import tests aren't
  copied, as they aren't in the C either.
- **SIMH:** `simh_ops.py` reads and prints the same format as the C driver,
  so the C comparison script runs it unchanged:

      python3 c/tests/differential/simh_check.py <simh>/BIN/pdp11 python/simh_ops.py 5000 1

  Seeds 1 and 2, 5,000 cases each: 9,520 compared, 0 mismatches. The cases
  not compared (JMP or JSR to a register, I/O-page access, `-(PC)`) are the
  ones the C comparison leaves out, and their counts are the same as the C's.
- **Against the C:** on 3,000 random single-instruction cases, `simh_ops.py`
  and the C `simh_ops` print byte-identical output.
- **Whole machine:** the hello-world output, and the ROM console session
  `L 1000 / D 12345 / D 707 / L 1000 / E / E` over 200,000 instructions,
  are byte-identical to the C version's.
- **The game:** `../game/hosttest.py python` replays bot games on this
  version and compares the screen output with the game built for the host:
  7 games (3 of 40 keys, 4 of 150) were all byte-identical.

## Speed

Wall-clock time for 300,000 instructions of the ROM's idle loop
(`runpdp11.py 300000 < /dev/null`), one run each:

| | Time | Instructions per second |
|---|---:|---:|
| As shipped (writes `pdp11.log`, 449,540 lines) | 10.9 s | about 27,000 |
| Logging off (`pdp11.log` a directory) | 3.4 s | about 88,000 |

With logging off that is about 20 times slower than the C and 30 times
slower than the Rust (both measured over 3,000,000 instructions; see
`../rust/README.md`). The game takes about 74,000 instructions a key,
under a second on this version.

Much of the remaining time is f-string log messages, which Python builds
even when logging is off, and the lock calls around every I/O-page access.
I left both as they were, to keep the code close to the original.
