# Rogue-11: a small Rogue-like for a bare PDP-11

A Rogue-like game written in C, compiled with GCC for the PDP-11, and run on
the C, Go and Rust emulators in this repository. It runs on the bare machine,
with no operating system. It talks to the console DL11 itself, and uses ANSI
escape codes, which the terminal window running the emulator interprets, to
put characters anywhere on the screen.

Each of the five levels has nine rooms joined by corridors, with monsters,
gold and potions. Find the stairs to go down. On level 5, the Amulet of
Yendor is where the stairs would be, and taking it wins.

## Playing

From anywhere:

    game/run-rust.sh      # or run-c.sh, run-go.sh

Each script builds its emulator if needed and starts the game. The compiled
game, `rogue.lst`, is in the repository, so playing doesn't need the cross
compiler.

| Key | Action |
|-----|--------|
| `h` `j` `k` `l` or arrows | move left, down, up, right |
| `y` `u` `b` `n` | move diagonally |
| `>` | go down the stairs |
| `.` or `s` | rest a turn |
| `q` | quit |

Walk into a monster to fight it, and onto gold (`*`) or a potion (`!`) to pick
it up. At the title screen, pressing a digit from 1 to 9 always gives the same
dungeon for that digit; any other key gives a random one. Ctrl-C stops the
emulator at any point.

Each key takes the PDP-11 about 74,000 instructions, measured over a
624-key game, mostly to redraw the map. That works out to roughly 30 ms per
key on the Rust emulator, 50 ms on the C and 80 ms on the Go.

## How it works

**No operating system.** The program is loaded at 0o1000, above the trap
vectors. `crt0.s` sets the stack pointer to 0o160000, so the stack grows
down from the top of memory below the I/O page. It then zeroes the
uninitialised data, calls `main`, and halts when `main` returns, which stops
the emulator. The program takes about 20 KB of the 56 KB below the I/O page.

**Console.** `console.c` polls the DL11 registers at 0o177560-0o177566:

- **Output:** wait for XCSR bit 7 (transmitter ready), then write XBUF.
- **Input:** wait for RCSR bit 7 (character received), then read RBUF.

There are no interrupts, and none are needed.

**Screen.** `gotoxy` sends `ESC [ row ; col H`. The game keeps a copy of
what the terminal shows, and each turn sends only the characters that
changed, the way curses does. It never relies on a bare line feed, because
the emulators' terminal setting turns LF into CR LF.

**No clock.** The game is turn-based: it waits for a key, then the monsters
move. The random number generator is a 16-bit xorshift. Its seed is the
number of times the title screen polled the keyboard before a key arrived,
which depends on how long the player took to press it.

**Loading.** `tolisting.py` turns the compiled image into the listing format
the emulators already read for the M9301 ROM: one octal address and word per
line. All three emulators' `runpdp11` now accept `-l listing`, which loads
the listing after the ROM and starts at its first address:

    runpdp11 [-l listing] [instruction-limit]

**Logging.** The run scripts start the emulator in a temporary directory,
linked to `config_default.json` and `source/`, where `pdp11.log` is a
directory. The log can't be opened, so nothing is logged. Otherwise the C and
Go emulators would log three lines every time the game polled the keyboard,
which is millions of lines a minute while it waits.

## Building

The cross compiler is GNU binutils 2.42 and GCC 14.2, configured for
`pdp11-aout`, C only, with libgcc and no C library. `build-toolchain.sh`
downloads and builds them into `~/pdp11-tools`. I built them with those
configure options on Linux; the script's download step is the part I
couldn't run from here.

    export PATH=~/pdp11-tools/bin:$PATH
    make -C game            # rogue.lst
    make -C game check      # start address and instruction check
    make -C game disassembly

The compiler flags:

- **`-m40`** targets an 11/40-class CPU, which has the EIS instructions (MUL,
  DIV, ASH, ASHC) and no floating-point unit, like the emulated 11/03.
- **`-msoft-float`**: the game uses no floating point.
- **`-fno-bit-tests`** works around a GCC bug (next section).

On a.out targets GCC makes `main` call `__main` first, to run constructors,
and libgcc's version needs `atexit`. `console.c` supplies an empty `__main`.

`make check` fails the build in two cases:

- the program doesn't start at 0o1000;
- the disassembly contains an instruction the emulators don't implement, so
  the program would stop there (floating point, MTPS/MFPS, MARK, EMT, TRAP
  and others).

### A GCC bug: comparisons done as bit tests

The first build couldn't move the player. SIMH ran the same program and
showed the same screen, which ruled out the emulators. The same source
compiled for the host moved normally, which ruled out the game logic. The
cause was in the code GCC generated for

```c
return c == '.' || c == '#' || c == '+' || c == '>';
```

GCC turns a test against a set of characters into a bit-mask test: one bit
for each character's offset from `#`, the lowest of them. In GCC 14.2's
PDP-11 code, the mask is shifted right one place in advance, and the
character's offset is reduced by one to compensate. For every character but
the first, that comes out the same. For `#` itself, the offset becomes -1,
the shift becomes one place left, and its bit, already shifted off the end
of the mask, is gone. So `#` tests false. After the test was inlined into
the main loop, GCC dropped `.` and `+` as well. Only `>` remained walkable,
so every move was refused.

A test program run on the emulator printed, for each character, whether it
tested true:

```
-O2                  .Y #n +Y >Y -n |n  n
-O2 -fno-bit-tests   .Y #Y +Y >Y -n |n  n
host gcc             .Y #Y +Y >Y -n |n  n
```

The Makefile builds with `-fno-bit-tests`. I haven't checked whether the bug
is reported against GCC, or fixed in a later version.

## Testing

`hosttest.py` checks the PDP-11 build against a build for the computer it
runs on:

1. It compiles the same `rogue.c` with the host's C compiler, with
   `host/console.c`, which reads keys from the command line and writes to
   stdout, in place of the DL11 code.
2. A bot plays the host build. It reads the screen after every key, fights
   whatever is next to it, heads for the stairs, items and unexplored doors
   and corridors, rests when hurt, and now and then does something random.
3. It replays the bot's keys on the emulators.
4. It compares the output of every run byte for byte.

A difference would mean a miscompilation like the one above, an emulator
bug, or code that depends on the size of `int`.

    python3 game/hosttest.py [c] [go] [rust] [--games N] [--keys N] [--seed N]

Results from 12 October 2026:

- **Rust:** 20 bot games of up to 300 keys, all identical to the host build.
- **C and Go:** 6 bot games each, all 12 runs identical to the host build.
- **One full game of 624 keys,** in which the bot reaches level 5 and is
  killed by a dragon, gave identical output on all three emulators and the
  host: 19,690 bytes.

Over 30 bot games of up to 3,000 keys on the host build, the bot won 4. The
others died: 11 on level 4, 9 on level 5, 3 on level 3 and 3 on level 2.

## Files

| File | Contents |
|------|----------|
| `rogue.c` | the game |
| `console.c`, `console.h` | DL11 input and output, ANSI screen control, `memset`/`memcpy`/`__main` |
| `crt0.s` | startup: stack, data, `main`, `HALT` |
| `Makefile` | build, `check`, `disassembly` |
| `tolisting.py` | memory image to the emulators' listing format |
| `rogue.lst` | the compiled game, ready to load |
| `run.sh`, `run-c.sh`, `run-go.sh`, `run-rust.sh` | run the game on an emulator |
| `build-toolchain.sh` | build the cross compiler |
| `hosttest.py`, `host/` | the host-build comparison and its console |
