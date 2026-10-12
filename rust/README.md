# PDP-11/03 emulator in Rust

This is a Rust version of the PDP-11 emulator in this repository. It emulates
an LSI-11 (PDP-11/03) with the EIS instructions, the console DL11 serial
interface and the M9301-YA boot ROM, and runs on the terminal you start it
in. There is no GUI.

The repository holds four versions: the original Python (with bugs), a
module-for-module C translation of it in `../c`, a Go translation of the
fixed C in `../go`, and this one. The C and Go versions keep the Python's
structure. This one is organised the way Rust code is usually organised.
This README records the decisions that led to that, and what came of
checking the result against SIMH and the C version.

## Build and run

    cd rust
    cargo test
    cargo build --release
    cd .. && rust/target/release/runpdp11      # optional argument: instruction limit

Run the programs from the repository root. They read `config_default.json`
and `source/M9301-YA.txt` from the current directory, and write `pdp11.log`
there. At the `$` prompt, the ROM console takes these commands:

- `L addr`: load address
- `E `: examine (E, then space)
- `D value`: deposit
- `S`: start

End each command with Return. Ctrl-C exits and restores the terminal.
Linux and macOS are supported.

```
rust/
  Cargo.toml             dependencies: libc, log, serde_json
  src/
    lib.rs               Size (word or byte); module list
    machine.rs           Machine: the bus, the stack, dispatch, the run loop
    registers.rs psw.rs ram.rs address_modes.rs
    br_ops.rs cc_ops.rs noopr_ops.rs other_ops.rs ss_ops.rs ssdd_ops.rs rss_ops.rs
    boot.rs dl11.rs m9301.rs terminal.rs
    config.rs logger.rs stopwatches.rs util.rs
    bin/runpdp11.rs      boot the ROM console on this terminal
    bin/hello_world.rs   run the "Hello, World!" program
    bin/simh_ops.rs      single-instruction driver for the SIMH comparison
```

## Architectural decisions

### 1. Translated from the Go version, not the C

The C and Go versions behave identically. On 3,000 random single-instruction
cases their drivers print byte-identical results, and so do the hello-world
program and a ROM console session. So the choice of source only affected how
directly the code would carry over, and the Go carried over more directly in
four places:

- **Errors.** Where the Python raised, the C prints a message and calls
  `exit`. That is why the C SIMH driver forks a child process for every case:
  otherwise one failing case would end the whole run. The Go panics, and its
  driver catches the panic with `recover()`. Rust's `panic!` and
  `catch_unwind` work the same way, so there's no fork.
- **Device registers.** In the C a device handler is a function pointer plus
  a `void *device` argument. In the Go it's a closure over the device. (In
  the end neither was kept; see decision 4.)
- **Keyboard.** The C polls stdin with `select` inside the CPU loop. The Go
  reads keys in a goroutine and passes them through a buffered channel. That
  maps directly onto a thread and an `mpsc` channel.
- **Python scaffolding.** The C carries over things that exist only to imitate
  Python: an `int64` type to mimic Python integers, a macro imitating keyword
  arguments (`PSW_set_psw(&psw, .psw = 0)`), and fixed `char` buffers for
  disassembly text. The Go had already replaced all of these with plain `int`
  and `string`.

Both versions still serve as references for verification: the C directly,
and SIMH through the C's comparison script.

### 2. One `Machine` owns all the state

In the C and Go, the instruction groups are objects that each hold pointers to
the same registers, memory and PSW. Those groups are the branch,
single-operand, double-operand, EIS and "other" ops, plus the address modes
and the stack. The DL11's handlers are stored inside the memory object, and
memory calls them. Rust won't let several objects hold mutable references to
the same data, which left two options.

- **Keep the shape with `Rc<RefCell<…>>`.** This preserves the
  object-for-object correspondence, but borrow conflicts are found at run time,
  as panics. This emulator has a real case of it. The PSW is mapped into
  memory at 0o177776, and in the Go it installs a reader on memory for that
  address. Take an instruction whose destination is the PSW's address. While
  setting the condition codes, the code would hold the PSW borrowed mutably.
  If the memory write then reached the PSW's handler, which borrows the PSW
  again, the program would panic.
- **Restructure around ownership.** One `Machine` owns the registers, memory,
  PSW, devices and terminal, and each instruction group becomes methods on
  it. The PSW is a field, so there's nothing to borrow twice, and the
  equivalent mistake is a compile error.

This version takes the second option. It fit the code well: apart from those
shared pointers, the Go instruction-group structs held only a reference to
the stopwatches. They existed to give their functions access to shared state.

What that cost in practice:

- **No struct-for-struct match.** The code no longer lines up struct for
  struct with the C and Go. Decision 3 keeps it file for file.
- **Disjoint borrows have to be visible.** A method can't hold a reference
  into one field of `self` while calling another `&mut self` method, so
  helpers take the specific fields they need. `Terminal::cycle` takes
  `&mut Dl11`, not `&mut Machine`, so `run` can pass `&mut self.dl11` while
  using `self.terminal`. The single-operand and double-operand `compute`
  functions take `&mut Psw`, not the whole machine.
- **One redesign during writing.** The first draft of the single-operand
  `compute` used a closure that held the PSW mutably while other branches of
  the same `match` read the carry bit. The borrow checker rejected it. Each
  operation now returns its result, V, and C (or "unaffected"), and the flags
  are applied once at the end. That is also easier to read against the
  handbook's tables.
- **Address-mode resolution goes step by step.** It reads a register, then
  memory, then writes the register back. Each step copies its value out
  before the next one starts.

### 3. Instruction groups are `impl Machine` blocks, one file per group

Rust allows one type to have `impl` blocks in several modules. So
`br_ops.rs`, `cc_ops.rs`, `noopr_ops.rs`, `other_ops.rs`, `ss_ops.rs`,
`ssdd_ops.rs` and `rss_ops.rs` each hold the methods for their group, and
each is named after the Python module it corresponds to.
`Machine::execute` dispatches in the Python's order:

- condition codes
- branches
- no-operand
- single-operand
- EIS
- double-operand
- other

Each group decodes with `match` on the opcode where the C and Go used lookup
maps. That covers mnemonics, branch conditions, and single-operand
operations as an `enum`.

### 4. The bus is a `match` on the address

Devices no longer register callbacks with memory. `Machine::read_word`,
`write_word`, `read_byte` and `write_byte` check whether the address is in the
I/O page, and if it is, `match` it:

| Address               | Device                          | Reads                     | Writes            |
|-----------------------|---------------------------------|---------------------------|-------------------|
| 0o177776              | PSW                             | the PSW                   | memory (see below)|
| 0o173024              | M9301 switches                  | the switch settings       | memory            |
| 0o177560-0o177566, even | console DL11 (RCSR, RBUF, XCSR, XBUF) | the register, with its side effects | the register |
| anything else         | memory                          | memory                    | memory            |

Writes to the PSW's address and to the switches go to memory, because they
did in the Python, C and Go. Keeping that means the versions stay comparable.

The C and Go SIMH drivers detected I/O-page access by installing a handler
on every I/O address. Here a single field, `Machine::io_touched`, is set by
any I/O-page access other than the PSW. It flags exactly the same cases as
the C: 17 and 11 on the two SIMH seeds, and 59 in the 15,000-case comparison
below.

### 5. No lock: the keyboard thread feeds the CPU through a channel

The Python, C and Go guard the DL11 with a lock, because the Python's GUI
thread touched it. Here only the CPU thread touches the machine. A separate
thread reads stdin and sends bytes through `mpsc::sync_channel(256)`. After
each instruction, the run loop passes at most one waiting key to the DL11,
and only when its receiver is empty.

Removing the lock also removed its logging: the C and Go write three log
lines for every DL11 register access. That turned out to be most of their
run time (see Outcomes).

### 6. Words are `u16`, bytes `u8`, with wrapping arithmetic

The Python used unbounded integers and masked them. The C used an `int64`
type and the Go used `int`, both masking in the same way. Here registers,
the PSW and memory words are `u16`, bytes are `u8`, and arithmetic uses
`wrapping_add`, `wrapping_sub`, `overflowing_add` and so on. A `Size` enum
(`Word` or `Byte`) supplies the mask, the sign bit, and `merge`, which keeps
the high byte when a byte operation writes a register.

The Python's conversion helpers become casts, so they and their tests are
gone; `util.rs` instead tests the casts on the same values. For example,
`pythonifyPDP11Word(x)` is `x as i16` and `twosComplementNegative(x)` is
`x.wrapping_neg()`.

DIV computes in `i64`, so -2³¹ ÷ -1 reports overflow. In `i32` it would
panic. The `div_errors` test covers this.

### 7. Errors

- **Loading.** A missing or unreadable configuration file or ROM listing is
  an `io::Error` returned from `Machine::boot` or `load_listing`. The
  binaries print it and exit.
- **Running.** Instruction execution has no error path. An instruction that
  no group implements returns `Executed { run: false, .. }` and stops the
  CPU, as in the C and Go. So does an illegal JMP or JSR to a register: the
  hardware would trap there, but traps through vector 4 aren't emulated.
- **SIMH driver.** `simh_ops` still runs each case inside `catch_unwind`, so
  a panic would be reported as `CRASH`, the same output as the C driver. In
  the comparisons none occurred.

### 8. Dependencies: `libc`, `log`, `serde_json`

The standard library has no terminal control and no JSON parser.

- **`libc`** provides `tcgetattr`, `tcsetattr` and `signal`. `RawMode`
  switches stdin to character-at-a-time input without echo and restores it
  on `Drop`. A SIGINT or SIGTERM handler restores the saved settings (kept
  in a `OnceLock`) and calls `_exit(130)`. Both calls are safe to make in a
  signal handler.
- **`log`** is the logging facade. `logger.rs` implements it with a file
  logger. The level is checked before a message is formatted, so the
  per-instruction debug line costs nothing at INFO level.
- **`serde_json`** reads `config_default.json`. `Config::lookup` returns
  `Option`, where the Python returned the string `"ERROR"`.

### 9. Paths are parameters, not the current directory

`Machine::boot(root)` takes the directory that holds `config_default.json`
and `source/`. The binaries pass `.`, and the tests pass the repository root,
derived from `CARGO_MANIFEST_DIR`.

The Go tests change directory once in `TestMain`. That can't be done safely
here, because `cargo test` runs tests on several threads and the current
directory is shared by the whole process.

### 10. Tests are independent

`cargo test` runs tests in parallel, in no fixed order. The pytest classes,
and the C and Go tests that followed them, carried state from one test to
the next. For example, the branch-offset test relied on the PC left by the
test before it. Here every test builds its own `Machine`. The tests never
initialise the logger, so nothing writes `pdp11.log` and parallel tests
don't compete for it.

The tests sit in each module's `tests` submodule. They cover the cases of
the C and Go tests, including all 29 MUL cases, the four DIV cases and every
SUB case. A few were changed:

- The address-mode test for mode 3 uses mode 3. The Python's used mode 2.
- The DL11 tests send every character from 32 to 128, where the Python's
  sent one random character.
- `test_implemented` logged a survey of opcodes without asserting anything.
  The Rust version asserts that every opcode a group claims has a name, and
  counts each group.

Further tests cover things the earlier suites didn't:

- BPT and RTI round trips
- DIV by zero and DIV overflow
- ASH and ASHC flags
- SOB
- XOR reading its register before the destination
- JSR SP pushing the decremented SP
- byte access to DL11 registers
- the PSW on the bus
- booting the ROM to its prompt

## Outcomes

Recorded 12 October 2026, on a 2-core Intel Xeon at 2.1 GHz under Linux, with:

- Rust 1.97.0, `--release`
- GCC 13.3 at `-O2`
- Go 1.24.7

### Tests and lint

`cargo test` passes all 88 tests, and `cargo clippy --all-targets` reports no
warnings.

### Against SIMH

`simh_ops` reads and writes the same format as the C driver, so the C
comparison script runs it unchanged:

    python3 ../c/tests/differential/simh_check.py /path/to/simh/BIN/pdp11 target/release/simh_ops 5000 1

SIMH (open-simh) was configured as an 11/03 with EIS. The script generates
random cases, runs each in SIMH and in the emulator, and compares R0-R7, the
NZVC bits, and two regions of memory.

| Seed | Cases | Compared | Mismatches | Not compared: JMP/JSR to a register | touched the I/O page | used -(PC) or @-(PC) |
|------|------:|---------:|-----------:|------:|------:|------:|
| 1    | 5,000 | 4,759    | 0          | 28    | 17    | 196   |
| 2    | 5,000 | 4,761    | 0          | 44    | 11    | 184   |

The cases not compared are the ones the C version doesn't compare either:

- **JMP or JSR to a register:** SIMH traps, but this emulator stops.
- **I/O-page access:** SIMH has real device registers there.
- **-(PC) and @-(PC):** DEC documents these as unpredictable.

The counts in each column are the same as the C version's.

### Against the C version

The same random cases (seeds 1, 2 and 3, 5,000 each) were fed to the C and
Rust drivers and their output compared line by line. All 15,000 lines are
byte-identical, including the 59 cases that touched the I/O page and so are
left out of the SIMH comparison.

### Whole machine

- The hello-world program prints the same output as the C version's, byte for
  byte, and halts at the same PC (0o2030).
- The ROM console session `L 1000`, `D 12345`, `D 707`, `L 1000`, `E`, `E`,
  over 200,000 instructions, is byte-identical to the C version's.
- With no input, the ROM prints its registers and the `$` prompt, and then
  waits. The C does the same.

### Speed

These are wall-clock times for 3,000,000 instructions of the ROM's idle loop
(`runpdp11 3000000 < /dev/null`), from single runs, so they show sizes of
difference rather than precise figures. For the "logging off" column, each
program ran in a directory where `pdp11.log` was a directory, so the log file
couldn't be opened and nothing was written.

| Version | As shipped | Logging off | Log lines written |
|---------|-----------:|------------:|------------------:|
| C       | 6.4 s      | 1.6-1.7 s   | 4,499,573         |
| Go      | 15.7-17.8 s | 3.1-3.2 s  | the same messages as the C |
| Rust    | 1.1 s      | 1.0-1.1 s   | 145               |

Most of the C and Go time is logging. Of the C's 4.5 million lines, about 1.5
million each are three lock messages:

- the DL11's `get_lock called by read_RCSR`
- the DL11's `release_lock called by read_RCSR`
- memory's `got lock`

They come from the ROM reading the DL11 receiver status on every pass of its
idle loop. The Go writes the same three messages: over 300,000 instructions
it wrote 449,568 lines, where the C wrote 449,573.

With logging off, the Rust version is about 1.5 times as fast as the C and about 3 times as fast as the
Go. All three still time every instruction group with stopwatches and keep
the table of executed instructions.

## Behaviour that differs from the C and Go

- The DL11 has no lock and logs no lock traffic.
- Log lines name the source file and line, not the function.
- Negative index words disassemble as signed octal (`-0o2(R1)`). The Python's
  formula gave wrong values for most negative offsets. This only shows in the
  log.
- The RK11 disk interface, a stub that nothing attached, isn't translated.

## Behaviour kept from the Python, so the versions stay comparable

- Writes to the PSW's address (0o177776) and to the M9301 switches go to
  memory.
- Opcodes 0o000210-0o000237 decode as RTS. On an 11/03 they are reserved.
- Only a 16-bit memory size (the configured one) works with the devices. The
  CPU's addresses are 16 bits, and the I/O page is the top 8 KB of memory.
  With more memory configured, the Rust version starts, but no address
  reaches a device. The Go refuses to start, because it can't register the
  devices below its I/O page.
