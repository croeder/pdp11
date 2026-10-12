// noopr_ops.go - translation of pdp11_noopr_ops (no-operand instructions 0-6).
package pdp11

type NooprOps struct {
	reg   *Registers
	psw   *PSW
	stack *Stack
	sw    *StopWatches
}

func NewNooprOps(reg *Registers, psw *PSW, stack *Stack, sw *StopWatches) *NooprOps {
	logInfof("initializing NoOperandOps")
	return &NooprOps{reg, psw, stack, sw}
}

type nooprInstruction struct {
	name string
	fn   func(o *NooprOps) (run bool, report string)
}

var noOperandInstructions = map[int]nooprInstruction{
	// 00 00 00 Halt
	0: {"HALT", func(o *NooprOps) (bool, string) { return false, "" }},
	// 00 00 01 Wait 4-75.  There are no interrupts to wait for, so the CPU stops.
	// FIX: the Python also advanced the PC past the next word.
	1: {"WAIT", func(o *NooprOps) (bool, string) { return false, "WAIT: no interrupts implemented; stopping" }},
	// 00 00 02 RTI return from interrupt 4-69: PC < (SP)^; PS < (SP)^
	2: {"RTI", func(o *NooprOps) (bool, string) {
		o.reg.SetPC(o.stack.Pop())
		o.psw.SetPSW(o.stack.Pop())
		return true, ""
	}},
	// 00 00 03 BPT breakpoint trap 4-67: trap through vector 14.
	// FIX: the Python advanced the PC and stopped.
	3: {"BPT", func(o *NooprOps) (bool, string) { o.stack.Trap(014); return true, "" }},
	// 00 00 04 IOT input/output trap 4-68: trap through vector 20.
	// FIX: the Python pushed a nonexistent attribute, set PC to 20 itself,
	// and loaded 22 into SP.
	4: {"IOT", func(o *NooprOps) (bool, string) { o.stack.Trap(020); return true, "" }},
	// 00 00 05 RESET reset external bus 4-76
	5: {"RESET", func(o *NooprOps) (bool, string) { return true, "; RESET unimplemented (mostly harmless)" }},
	// 00 00 06 RTT return from interrupt 4-70: like RTI (no trace trap is emulated).
	// FIX: the Python popped the PS into SP and then raised AttributeError.
	6: {"RTT", func(o *NooprOps) (bool, string) {
		o.reg.SetPC(o.stack.Pop())
		o.psw.SetPSW(o.stack.Pop())
		return true, ""
	}},
}

// NooprName returns a no-operand instruction's mnemonic.
func NooprName(instruction int) (string, bool) {
	i, ok := noOperandInstructions[instruction]
	return i.name, ok
}

func (o *NooprOps) IsNooprOp(instruction int) bool {
	_, ok := noOperandInstructions[instruction]
	return ok
}

func (o *NooprOps) DoNooprOp(instruction int) OpResult {
	o.sw.Start("noopr")
	defer o.sw.Stop("noopr")
	i, ok := noOperandInstructions[instruction]
	if !ok {
		return OpResult{Report: "Error: no-operand opcode not found"}
	}
	run, report := i.fn(o)
	return OpResult{Run: run, Assembly: i.name, Report: report}
}
