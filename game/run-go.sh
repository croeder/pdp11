#!/bin/sh
# Runs the game on the Go emulator; see run.sh.
exec "$(dirname "$0")/run.sh" go "$@"
