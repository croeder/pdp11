# PDP-11/03 emulator in Go

A translation of the C version in `../c` (the bug-fixed one, not the faithful
translation of the Python), module for module, with the tests ported as Go
tests. Standard library only; no GUI. The machine talks to the terminal you
run it in.

    go/
      go.mod                 module pdp11emu
      pdp11/                 the emulator (package pdp11)
        util.go config.go stopwatches.go hardware.go
        br_ops.go cc_ops.go noopr_ops.go other_ops.go ss_ops.go ssdd_ops.go rss_ops.go
        boot.go dl11.go m9301.go terminal.go termios_linux.go termios_bsd.go pdp11.go
        *_test.go            the ported tests
      cmd/runpdp11/          boot the M9301-YA ROM console on stdin/stdout
      cmd/hello_world/       run the "Hello, World!" program
      cmd/simh_ops/          single-instruction driver for the SIMH comparison

As with the C, run the programs from the repository root: the emulator reads
`config_default.json` and `source/M9301-YA.txt` relative to the current
directory, and writes `pdp11.log` there.

## Build and run

    cd go
    go test ./...
    go build ./cmd/runpdp11
    cd .. && go/runpdp11           # optional argument: instruction limit

At the `$` prompt the ROM console takes `L addr` (load address), `E `
(examine, E then space), `D value` (deposit), and `S` (start), each ended
with Return. Ctrl-C exits and restores the terminal. Linux, macOS and the
BSDs are supported (termios through `syscall`).

## Verification

- `go test ./...`: all the ported tests pass (165 test functions and
  subtests), with the C version's expectations.
- SIMH: `cmd/simh_ops` reads and writes the same format as the C
  `simh_ops`, so the C driver runs it unchanged:

      go build -o /tmp/simh_ops_go ./cmd/simh_ops
      python3 ../c/tests/differential/simh_check.py /path/to/simh/BIN/pdp11 /tmp/simh_ops_go 5000 1

  Seeds 1 and 2, 5,000 cases each: 9,520 compared, 0 mismatches.
  On 3,000 cases the Go and C drivers print byte-identical results.
- Whole machine: hello world output, and the ROM console session
  `L 1000 / D 12345 / D 707 / L 1000 / E / E` over 200,000 instructions,
  are byte-identical to the C version's.

Speed: about 2.5x slower than the C on the ROM's idle loop (3,000,000
instructions in about 16 s vs 6 s here), still around 12x faster than the
Python. Both spend much of that time in logging.

## Differences from the C

- Errors the Python raised (and the C reported before exiting) are panics.
- The keyboard is read by a goroutine into a buffered channel; the CPU loop
  takes at most one key per instruction, when the DL11 receiver is empty.
- Tests share fixtures through package variables and depend on declaration
  order within a file, as the pytest classes did; `go test -run` on a single
  test can therefore behave differently.
