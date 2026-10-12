// runpdp11 - translation of runpdp11.py: power on the PDP11 (M9301-YA boot
// ROM, console emulator) and run it on this terminal.
//
// Usage: runpdp11 [instruction-limit]   (default: no limit; Ctrl-C stops)
// Run it from the directory that holds config_default.json and source/.
package main

import (
	"os"
	"strconv"

	"pdp11emu/pdp11"
)

func main() {
	limit := -1
	if len(os.Args) > 1 {
		n, err := strconv.Atoi(os.Args[1])
		if err != nil {
			os.Stderr.WriteString("usage: runpdp11 [instruction-limit]\n")
			os.Exit(2)
		}
		limit = n
	}
	p := pdp11.New("None")
	restore := p.Terminal.RawInput()
	defer restore()
	p.Terminal.KeysFrom(os.Stdin)
	p.Run(limit)
}
