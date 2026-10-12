#!/bin/sh
# Runs the game on the Rust emulator; see run.sh.
exec "$(dirname "$0")/run.sh" rust "$@"
