// br_ops.go - translation of pdp11_br_ops (branch instructions).
//
// MASK_HIGH_BYTE is the opcode, MASK_LOW_BYTE the signed word offset.
package pdp11

import "fmt"

// OpResult is the (run, operand1, operand2, assembly, report) tuple every
// do_*_op returns.
type OpResult struct {
	Run      bool
	Operand1 string
	Operand2 string
	Assembly string
	Report   string
}

type BrOps struct {
	reg *Registers
	psw *PSW
	sw  *StopWatches
}

func NewBrOps(reg *Registers, psw *PSW, sw *StopWatches) *BrOps {
	logInfof("initializing branchOps")
	return &BrOps{reg, psw, sw}
}

type branch struct {
	name string
	take func(p *PSW) bool
}

// branchInstructions: the conditions follow the DEC definitions.
// FIX: the Python's BGE tested N or V instead of N xor V.
var branchInstructions = map[int]branch{
	0000400: {"BR", func(p *PSW) bool { return true }},
	0001000: {"BNE", func(p *PSW) bool { return p.Z() == 0 }},
	0001400: {"BEQ", func(p *PSW) bool { return p.Z() == 1 }},
	0002000: {"BGE", func(p *PSW) bool { return p.N()^p.V() == 0 }},
	0002400: {"BLT", func(p *PSW) bool { return p.N()^p.V() == 1 }},
	0003000: {"BGT", func(p *PSW) bool { return p.Z()|(p.N()^p.V()) == 0 }},
	0003400: {"BLE", func(p *PSW) bool { return p.Z()|(p.N()^p.V()) == 1 }},
	0100000: {"BPL", func(p *PSW) bool { return p.N() == 0 }},
	0100400: {"BMI", func(p *PSW) bool { return p.N() == 1 }},
	0101000: {"BHI", func(p *PSW) bool { return p.C() == 0 && p.Z() == 0 }},
	0101400: {"BLOS", func(p *PSW) bool { return p.C()|p.Z() == 1 }},
	0102000: {"BVC", func(p *PSW) bool { return p.V() == 0 }},
	0102400: {"BVS", func(p *PSW) bool { return p.V() == 1 }},
	0103000: {"BCC", func(p *PSW) bool { return p.C() == 0 }}, // BCC = BHIS
	0103400: {"BCS", func(p *PSW) bool { return p.C() == 1 }}, // BCS = BLO
}

// BranchName returns the mnemonic for a branch opcode (instruction & 0o177400).
func BranchName(opcode int) (string, bool) {
	b, ok := branchInstructions[opcode]
	return b.name, ok
}

func (o *BrOps) IsBrOp(instruction int) bool {
	// *0 ** xxx: bit 15 either, bits 14-12 zero, bits 11-8 a branch code
	if instruction&0070000 != 0 {
		return false
	}
	_, ok := branchInstructions[instruction&0107400]
	return ok && instruction&0107400 != 0
}

func (o *BrOps) DoBrOp(instruction int) OpResult {
	o.sw.Start("br")
	defer o.sw.Stop("br")
	opcode := instruction & MaskHighByte
	offset := instruction & MaskLowByte
	b, ok := branchInstructions[opcode]
	if !ok {
		return OpResult{Assembly: "Error: branch opcode not found"}
	}
	if b.take(o.psw) {
		o.reg.SetPC2xOffset(offset)
	}
	return OpResult{Run: true, Assembly: fmt.Sprintf("%s %s", b.name, PyOct(offset))}
}
