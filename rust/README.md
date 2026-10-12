# PDP-11/03 emulator in Rust

A Rust version of the emulator in `../c` and `../go` (the bug-fixed versions,
which follow the DEC processor handbook and match SIMH). It emulates an
LSI-11 (PDP-11/03) with the EIS instructions, the console DL11, and the
M9301-YA boot ROM, and talks to the terminal you run it in. No GUI.

## Design

The C and Go versions keep the Python's object structure: each instruction
group (branches, single-operand, double-operand, ...) is an object holding
its own pointers to the shared registers, memory and PSW, and devices
register callbacks with memory. Rust does not allow several objects to hold
mutable references to the same data, so this version is organised around
ownership instead:

- One `Machine` owns the registers, memory, PSW, devices and terminal.
- Each instruction group is an `impl Machine` block in its own module,
  named after the Python one (`br_ops.rs`, `ss_ops.rs`, `ssdd_ops.rs`,
  `rss_ops.rs`, `cc_ops.rs`, `noopr_ops.rs`, `other_ops.rs`), so the files
  still line up with the other versions.
- The bus is a `match` on the address in `Machine::read_word` and friends:
  the PSW at 0o177776, the M9301 switches at 0o173024, the DL11 at
  0o177560-0o177566, memory otherwise. Nothing registers callbacks.
- Words are `u16` and bytes `u8`, with wrapping arithmetic, so the Python's
  integer-conversion helpers become casts.
- There is no lock: the keyboard is read on its own thread and passed to
  the CPU loop through a channel, so only the CPU thread touches the machine.

```
rust/
  Cargo.toml             dependencies: libc (termios), log, serde_json (config)
  src/
    lib.rs               Size (word/byte), module list
    machine.rs           Machine: the bus, the stack, dispatch, the run loop
    registers.rs psw.rs ram.rs address_modes.rs
    br_ops.rs cc_ops.rs noopr_ops.rs other_ops.rs ss_ops.rs ssdd_ops.rs rss_ops.rs
    boot.rs dl11.rs m9301.rs terminal.rs
    config.rs logger.rs stopwatches.rs util.rs
    bin/runpdp11.rs      boot the ROM console on this terminal
    bin/hello_world.rs   run the "Hello, World!" program
    bin/simh_ops.rs      single-instruction driver for the SIMH comparison
```

## Build and run

    cd rust
    cargo test
    cargo build --release
    cd .. && rust/target/release/runpdp11      # optional argument: instruction limit

Run the programs from the repository root: they read `config_default.json`
and `source/M9301-YA.txt` from the current directory, and write `pdp11.log`
there. At the `$` prompt the ROM console takes `L addr` (load address),
`E ` (examine: E, then space), `D value` (deposit) and `S` (start), ended
with Return. Ctrl-C exits and restores the terminal. Linux and macOS.

## Verification

- `cargo test`: 88 tests, covering the cases of the C/Go tests plus
  some for behaviour they did not test (traps, DIV errors, shifts, SOB, XOR,
  byte access to the DL11). `cargo clippy` is clean.
- SIMH: `simh_ops` reads and writes the same format as the C driver, so the
  C comparison script runs it unchanged:

      cargo build --release
      python3 ../c/tests/differential/simh_check.py /path/to/simh/BIN/pdp11 target/release/simh_ops 5000 1

  Seeds 1 and 2, 5,000 cases each: 9,520 compared, 0 mismatches. Against the
  C driver directly, 15,000 cases (seeds 1-3) give byte-identical output.
- Whole machine: the hello world output, and the ROM console session
  `L 1000 / D 12345 / D 707 / L 1000 / E / E` over 200,000 instructions,
  are byte-identical to the C version's.

Speed: 3,000,000 instructions of the ROM's idle loop take about 1.1 s here,
against about 6.4 s for the C and 18 s for the Go. Most of that difference
is logging, not language: the C and Go log three lines for every access to
a DL11 register (the Python's lock messages), 4.5 million lines in that
run, where this version writes 145.

## Differences from the C and Go

- The DL11 has no lock and does not log lock traffic.
- Log lines name the source file and line rather than the function.
- Negative index words disassemble as signed octal (`-0o2(R1)`); the Python's
  formula gave wrong values for most negative offsets. This only affects the
  log.
- The Python's integer-conversion helpers and their tests are gone; the
  `util` tests check the casts that replace them on the same values.
- The RK11 disk stub, which nothing attached, is not translated.
- The tests are independent of each other, since `cargo test` runs them in
  parallel and in no fixed order; the pytest/C/Go tests carried state from
  one test to the next. The address-mode test for mode 3 uses mode 3 (the
  Python's used mode 2), and the DL11 tests use every character instead of
  one random one.

Behaviour that is unchanged, so the versions stay comparable: writes to the
PSW's address and to the M9301 switches go to memory, and opcodes
0o000210-0o000237 decode as RTS, as in the Python.
