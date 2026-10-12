#!/bin/sh
# Runs the game on one of the emulators:
#
#     game/run.sh c|go|rust [instruction-limit]
#
# Builds the emulator if needed, and the game if the cross compiler is on
# PATH or in ~/pdp11-tools/bin (otherwise it uses the rogue.lst in the
# repository).  Ctrl-C stops
# the emulator.
#
# The emulators read config_default.json and source/ from the current
# directory and write pdp11.log there, so the game runs in a temporary
# directory holding links to those, where pdp11.log is a directory: the log
# can't be opened, so the emulators don't log.  (The C and Go versions log
# three lines every time the program polls the console, and the game polls
# continuously while it waits for a key: millions of lines a minute, and
# most of their running time.)
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
which=${1:-}
[ $# -gt 0 ] && shift
case $which in
c)
    make -s -C "$root/c" build/runpdp11 >&2
    emulator=$root/c/build/runpdp11 ;;
go)
    (cd "$root/go" && go build -o runpdp11 ./cmd/runpdp11)
    emulator=$root/go/runpdp11 ;;
rust)
    (cd "$root/rust" && cargo build -q --release --bin runpdp11)
    emulator=$root/rust/target/release/runpdp11 ;;
*)
    echo "usage: $0 c|go|rust [instruction-limit]" >&2
    exit 2 ;;
esac
if command -v pdp11-aout-gcc >/dev/null 2>&1 || [ -x "$HOME/pdp11-tools/bin/pdp11-aout-gcc" ]; then
    make -s -C "$here" rogue.lst >&2
fi

run=$(mktemp -d)
trap 'rm -rf "$run"' EXIT INT TERM
ln -s "$root/config_default.json" "$root/source" "$run/"
mkdir "$run/pdp11.log"
cd "$run"
"$emulator" -l "$here/rogue.lst" "$@"
