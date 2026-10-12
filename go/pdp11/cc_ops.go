// cc_ops.go - translation of pdp11_cc_ops (condition code operators, 0o240-0o277).
package pdp11

const (
	setOpcode   = 0000260
	clearOpcode = 0000240
	pswBits     = 0000017
)

type CcOps struct {
	psw *PSW
	sw  *StopWatches
}

func NewCcOps(psw *PSW, sw *StopWatches) *CcOps {
	logInfof("initializing ConditionCodeOps")
	return &CcOps{psw, sw}
}

func (o *CcOps) IsCcOp(instruction int) bool { return 0000240 <= instruction && instruction <= 0000277 }

func (o *CcOps) DoCcOp(instruction int) OpResult {
	o.sw.Start("cc")
	defer o.sw.Stop("cc")
	var assembly string
	switch {
	case instruction&setOpcode == setOpcode:
		// FIX: set the selected bits; the Python replaced all four NZVC bits.
		set := instruction & pswBits
		assembly = "SETCC " + PyOct(set)
		o.psw.SetNZVCBits((o.psw.PSW & pswBits) | set)
	case instruction&clearOpcode == clearOpcode:
		clr := instruction & pswBits
		assembly = "CLRCC " + PyOct(clr)
		o.psw.SetNZVCBits(o.psw.PSW &^ clr)
	default:
		raise("UnboundLocalError", "assembly referenced before assignment")
	}
	return OpResult{Run: true, Assembly: assembly}
}
