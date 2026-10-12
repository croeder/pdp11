// simh_ops - Go side of the comparison with SIMH; the counterpart of
// c/tests/differential/simh_ops.c, with the same input and output, so
// c/tests/differential/simh_check.py can drive it.
//
// Reads one case per line: R0..R7, PSW, then (address value) pairs in octal
// to deposit.  An emulator stop (a panic) is reported as CRASH.  Prints one
// result line per case: R0..R7, PSW, an I/O-page-touched flag, then the
// words of the compared memory regions.
package main

import (
	"bufio"
	"fmt"
	"os"
	"strconv"
	"strings"

	"pdp11emu/pdp11"
)

var regions = [][2]int{{0400, 0500}, {01000, 01200}}

func runCase(line string) (result string) {
	defer func() {
		if r := recover(); r != nil {
			result = fmt.Sprintf("CRASH %v", r)
		}
	}()
	lock := &pdp11.Lock{}
	sw := pdp11.NewStopWatches()
	reg := pdp11.NewRegisters()
	ram := pdp11.NewRam(lock, reg, 16)
	psw := pdp11.NewPSW(ram)
	stack := pdp11.NewStack(reg, ram, psw)
	am := pdp11.NewAddressModes(reg, ram, psw)
	br := pdp11.NewBrOps(reg, psw, sw)
	cc := pdp11.NewCcOps(psw, sw)
	noopr := pdp11.NewNooprOps(reg, psw, stack, sw)
	other := pdp11.NewOtherOps(reg, ram, psw, am, sw)
	rss := pdp11.NewRssOps(reg, psw, am, sw)
	ss := pdp11.NewSsOps(reg, psw, am, sw)
	ssdd := pdp11.NewSsddOps(reg, psw, am, sw)

	// SIMH's script clears memory; NewRam fills the vector area with 0o277.
	clear(ram.Memory[:ram.IOSpace])

	// Any access to the I/O page (other than the PSW) is flagged, because
	// SIMH has real device registers there and this machine has none.
	touchedIO := 0
	for a := ram.IOSpace; a < psw.PSWAddress; a++ {
		ram.RegisterIOReader(a, func() int { touchedIO = 1; return 0 }, "io_read")
		ram.RegisterIOWriter(a, func(int) { touchedIO = 1 }, "io_write")
	}

	fields := strings.Fields(line)
	num := func(i int, base int) int {
		v, err := strconv.ParseInt(fields[i], base, 64)
		if err != nil {
			panic(err)
		}
		return int(v)
	}
	for i := 9; i+1 < len(fields); i += 2 {
		ram.WriteWord(num(i, 8), num(i+1, 8))
	}
	for i := 0; i < 8; i++ {
		reg.Set(i, num(i, 10))
	}
	psw.SetPSW(num(8, 10))

	// one fetch-decode-execute cycle, dispatched as in PDP11.Dispatch
	instruction := ram.ReadWordFromPC()
	switch {
	case cc.IsCcOp(instruction):
		cc.DoCcOp(instruction)
	case br.IsBrOp(instruction):
		br.DoBrOp(instruction)
	case noopr.IsNooprOp(instruction):
		noopr.DoNooprOp(instruction)
	case ss.IsSsOp(instruction):
		ss.DoSsOp(instruction)
	case rss.IsRssOp(instruction):
		rss.DoRssOp(instruction)
	case ssdd.IsSsddOp(instruction):
		ssdd.DoSsddOp(instruction)
	default:
		other.DoOtherOp(instruction)
	}

	var b strings.Builder
	for i := 0; i < 8; i++ {
		fmt.Fprintf(&b, "%06o ", reg.Get(i))
	}
	fmt.Fprintf(&b, "%06o %d", psw.PSW&pdp11.MaskWord, touchedIO)
	for _, r := range regions {
		for a := r[0]; a < r[1]; a += 2 {
			fmt.Fprintf(&b, " %06o", ram.ReadWord(a))
		}
	}
	return b.String()
}

func main() {
	in := bufio.NewScanner(os.Stdin)
	in.Buffer(make([]byte, 1<<16), 1<<20)
	out := bufio.NewWriter(os.Stdout)
	defer out.Flush()
	for in.Scan() {
		fmt.Fprintln(out, runCase(in.Text()))
	}
}
