// runpdp11 - translation of runpdp11.py: power on the PDP11 (M9301-YA boot
// ROM, console emulator) and run it on this terminal.
//
// Usage: runpdp11 [-l listing] [instruction-limit]
//
//	-l listing  also load a program ("address word" lines in octal, the
//	            format of source/M9301-YA.txt) and start at its first address
//	instruction-limit  default: no limit; Ctrl-C stops
//
// Run it from the directory that holds config_default.json and source/.
package main

import (
	"flag"
	"fmt"
	"os"
	"strconv"

	"pdp11emu/pdp11"
)

func main() {
	listing := flag.String("l", "", "program listing to load and start")
	flag.Usage = func() {
		fmt.Fprintln(os.Stderr, "usage: runpdp11 [-l listing] [instruction-limit]")
	}
	flag.Parse()
	limit := -1
	switch flag.NArg() {
	case 0:
	case 1:
		n, err := strconv.Atoi(flag.Arg(0))
		if err != nil {
			flag.Usage()
			os.Exit(2)
		}
		limit = n
	default:
		flag.Usage()
		os.Exit(2)
	}
	p := pdp11.New("None")
	if *listing != "" {
		p.Reg.SetPC(p.Boot.ReadPDP11AssemblyFile(*listing))
	}
	restore := p.Terminal.RawInput()
	defer restore()
	p.Terminal.KeysFrom(os.Stdin)
	p.Run(limit)
}
