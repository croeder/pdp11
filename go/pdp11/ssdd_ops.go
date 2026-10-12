// ssdd_ops.go - translation of pdp11_ssdd_ops (double-operand instructions
// 01 SS DD - 06 SS DD and 11 SS DD - 16 SS DD).
//
// Instruction semantics follow the DEC processor handbook, as in the fixed C
// version; FIX comments say what the Python did.
package pdp11

type ssddInstruction func(o *SsddOps, b byte, source, dest int) int

type SsddOps struct {
	reg *Registers
	psw *PSW
	am  *AddressModes
	sw  *StopWatches
}

func NewSsddOps(reg *Registers, psw *PSW, am *AddressModes, sw *StopWatches) *SsddOps {
	logDebugf("initializing DoubleOperandOps")
	return &SsddOps{reg, psw, am, sw}
}

// ByteMask is ssdd_ops.byte_mask.
func (o *SsddOps) ByteMask(b byte, value, target int) int { return byteMask(b, value, target) }

// MOV: N, Z; V cleared; C unaffected.  MOVB to a register sign-extends.
// FIX: the Python kept the register's old high byte.
func ssddMOV(o *SsddOps, b byte, source, dest int) int {
	s := source & sizeMask(b)
	setNZ(o.psw, b, s)
	o.psw.SetV(0)
	if b == 'B' && s&MaskByteMSB != 0 {
		return s | MaskHighByte // memory destinations get only the low byte
	}
	return s
}

// CMP: src - dst; neither changes.  V: opposite signs and the result has the
// sign of dst.  C: borrow (src < dst unsigned).
// FIX: the Python's V test wrote the mode bits and C was never set.
func ssddCMP(o *SsddOps, b byte, source, dest int) int {
	s, d := source&sizeMask(b), dest&sizeMask(b)
	r := (s - d) & sizeMask(b)
	setNZ(o.psw, b, r)
	o.psw.SetV(bit((s^d)&(^d^r)&signBit(b) != 0))
	o.psw.SetC(bit(s < d))
	return dest
}

// BIT: src & dst; neither changes.
// FIX: the Python tested byte_mask(source, source & dest) and wrote it to dst.
func ssddBIT(o *SsddOps, b byte, source, dest int) int {
	r := source & dest & sizeMask(b)
	setNZ(o.psw, b, r)
	o.psw.SetV(0)
	return dest
}

func ssddBIC(o *SsddOps, b byte, source, dest int) int {
	r := ^source & dest & sizeMask(b)
	setNZ(o.psw, b, r)
	o.psw.SetV(0)
	return byteMask(b, r, dest)
}

func ssddBIS(o *SsddOps, b byte, source, dest int) int {
	r := (source | dest) & sizeMask(b)
	setNZ(o.psw, b, r)
	o.psw.SetV(0)
	return byteMask(b, r, dest)
}

// ADDSUB: ADD (b == 'W') or SUB (b == 'B'); both are word instructions.
// ADD V: same-sign operands, other-sign result; C: carry out of bit 15.
// SUB V: opposite-sign operands, result has the sign of src; C: borrow.
// FIX: the Python tied C to V, missed carries, and never set C on SUB borrows.
func ssddADDSUB(o *SsddOps, b byte, source, dest int) int {
	s, d := source&MaskWord, dest&MaskWord
	var r, v, c int
	if b == 'W' {
		r = (s + d) & MaskWord
		v = bit(^(s^d)&(s^r)&MaskWordMSB != 0)
		c = bit(s+d > MaskWord)
	} else {
		r = (d - s) & MaskWord
		v = bit((s^d)&(^s^r)&MaskWordMSB != 0)
		c = bit(s > d)
	}
	setNZ(o.psw, 'W', r)
	o.psw.SetV(v)
	o.psw.SetC(c)
	return r
}

var doubleOperandInstructions = map[int]ssddInstruction{
	0010000: ssddMOV, 0020000: ssddCMP, 0030000: ssddBIT,
	0040000: ssddBIC, 0050000: ssddBIS, 0060000: ssddADDSUB,
}

var doubleOperandNames = map[int]string{
	0010000: "MOV", 0020000: "CMP", 0030000: "BIT", 0040000: "BIC", 0050000: "BIS", 0060000: "ADD",
	0110000: "MOVB", 0120000: "CMPB", 0130000: "BITB", 0140000: "BICB", 0150000: "BISB", 0160000: "SUB",
}

// DoubleOperandName returns the mnemonic for instruction & 0o170000.
func DoubleOperandName(opcode int) (string, bool) {
	n, ok := doubleOperandNames[opcode]
	return n, ok
}

func (o *SsddOps) IsSsddOp(instruction int) bool {
	switch instruction & 0070000 {
	case 0010000, 0020000, 0030000, 0040000, 0050000, 0060000:
		return true
	}
	return false
}

func (o *SsddOps) DoSsddOp(instruction int) OpResult {
	o.sw.Start("ssdd")
	defer o.sw.Stop("ssdd")
	var bw byte = 'W'
	if instruction&0100000 != 0 {
		bw = 'B'
	}
	opcode := instruction & 0070000
	fn, ok1 := doubleOperandInstructions[opcode]
	name, ok2 := doubleOperandNames[instruction&0170000]
	if !ok1 || !ok2 {
		raise("UnboundLocalError", "report referenced before assignment")
	}
	// ADD and SUB are word instructions even though SUB has bit 15 set.
	// FIX: the Python fetched SUB's operands as bytes.
	size := bw
	if opcode == 0060000 {
		size = 'W'
	}
	s := o.am.Get(size, (instruction&0007700)>>6)
	// FIX: MOV does not read its destination (the Python did, which matters
	// for device registers such as the DL11 receive buffer).
	var d Operand
	if opcode == 0010000 {
		d = o.am.Address(size, instruction&077)
	} else {
		d = o.am.Get(size, instruction&077)
	}
	result := fn(o, bw, s.Value, d.Value)
	// FIX: CMP and BIT do not write their destination.
	if opcode != 0020000 && opcode != 0030000 {
		o.am.Set(size, d, result)
	}
	return OpResult{Run: true, Operand1: s.OperandWord, Operand2: d.OperandWord,
		Assembly: name + " " + s.Assembly + "," + d.Assembly}
}
