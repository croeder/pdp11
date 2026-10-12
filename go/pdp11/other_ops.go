// other_ops.go - translation of pdp11_other_ops (RTS, JSR, and leftovers).
package pdp11

import "fmt"

type OtherOps struct {
	reg   *Registers
	ram   *Ram
	psw   *PSW
	am    *AddressModes
	stack *Stack // other_ops makes its own Stack(reg, ram, psw)
	sw    *StopWatches
}

func NewOtherOps(reg *Registers, ram *Ram, psw *PSW, am *AddressModes, sw *StopWatches) *OtherOps {
	logInfof("initializing OtherOps")
	return &OtherOps{reg, ram, psw, am, NewStack(reg, ram, psw), sw}
}

// RTS: 00 20 0R  PC <- reg; reg <- (SP)^
func (o *OtherOps) rts(instruction int) (bool, string, string) {
	r := instruction & 07
	o.reg.SetPC(o.reg.Get(r))
	o.reg.Set(r, o.stack.Pop())
	return true, "RTS", fmt.Sprintf("    %s %s RTS R%d", PyOct(o.reg.GetPC()), PyOct(instruction), r)
}

// JSR: 00 4R DD  push(reg); reg <- PC; PC <- (dst)
func (o *OtherOps) jsr(instruction int) (bool, string, string) {
	r := (instruction & 0700) >> 6
	dd := instruction & 077
	run, target, _, _ := o.am.Jump(dd)
	if !run {
		// FIX: JSR R,Rn is illegal; stop instead of jumping to 0.  *** should trap
		return false, "JSR", "JSR with register destination is illegal"
	}
	// SP is decremented before R is read, so JSR SP,dst pushes the
	// decremented SP (as SIMH and the hardware do; the Python pushed the old SP).
	o.reg.SetSP(o.reg.GetSP() - 2)
	o.ram.WriteWord(o.reg.GetSP(), o.reg.Get(r))
	o.reg.Set(r, o.reg.GetPC())
	o.reg.SetPC(target)
	return true, "JSR", fmt.Sprintf("    %s %s JSR R%d DD:%s address:%s",
		PyOct(o.reg.GetPC()), PyOct(instruction), r, PyOct(dd), PyOct(target))
}

func unimplementedOther(name string) func(*OtherOps, int) (bool, string, string) {
	return func(o *OtherOps, instruction int) (bool, string, string) {
		return true, name, PyOct(instruction) + " unimplemented"
	}
}

var otherInstructions = map[int]func(*OtherOps, int) (bool, string, string){
	0000200: (*OtherOps).rts,
	0004000: (*OtherOps).jsr,
	0006400: unimplementedOther("MARK"),
	0006500: unimplementedOther("MFPI"),
	0006600: unimplementedOther("MTPI"),
}

func (o *OtherOps) IsOtherOp(instruction int) bool {
	m1 := instruction & 0777700
	switch m1 {
	case 0000200, 0002000, 0004000, 0006400, 0006500, 0006600:
		return true
	}
	return instruction&0777000 == 0004000
}

func (o *OtherOps) DoOtherOp(instruction int) OpResult {
	o.sw.Start("other")
	defer o.sw.Stop("other")
	opcode := instruction & 0777700
	if instruction&0777000 == 0004000 {
		opcode = 0004000
	}
	fn, ok := otherInstructions[opcode]
	if !ok {
		return OpResult{Assembly: PyOct(instruction), Report: "Error: other opcode not found"}
	}
	run, assembly, report := fn(o, instruction)
	return OpResult{Run: run, Assembly: assembly, Report: report}
}
