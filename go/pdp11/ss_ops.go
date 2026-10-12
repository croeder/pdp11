// ss_ops.go - translation of pdp11_ss_ops (single-operand instructions).
//
// Instruction semantics follow the DEC processor handbook, as in the fixed C
// version; FIX comments say what the Python did.
package pdp11

import "fmt"

// sizes: M is the value mask, S the sign bit, for 'B' (byte) or word
func sizeMask(b byte) int {
	if b == 'B' {
		return MaskLowByte
	}
	return MaskWord
}

func signBit(b byte) int {
	if b == 'B' {
		return MaskByteMSB
	}
	return MaskWordMSB
}

// setNZ sets N and Z from a result already masked to the operand size.
func setNZ(p *PSW, b byte, r int) {
	p.SetN(bit(r&signBit(b) != 0))
	p.SetZ(bit(r == 0))
}

// byteMask: for byte operations only the low byte changes; the target's
// high byte is kept.
func byteMask(b byte, value, target int) int {
	if b == 'B' {
		return (value & MaskLowByte) | (target & MaskHighByte)
	}
	return value
}

type ssInstruction func(o *SsOps, operand int, b byte) (result int, report string)

type SsOps struct {
	reg *Registers
	psw *PSW
	am  *AddressModes
	sw  *StopWatches
}

func NewSsOps(reg *Registers, psw *PSW, am *AddressModes, sw *StopWatches) *SsOps {
	logInfof("initializing SingleOperandOps")
	return &SsOps{reg, psw, am, sw}
}

// ByteMask is ss_ops.byte_mask.
func (o *SsOps) ByteMask(b byte, value, target int) int { return byteMask(b, value, target) }

func ssJMP(o *SsOps, operand int, b byte) (int, string) {
	o.reg.SetPC(operand)
	return operand, ""
}

func ssSWAB(o *SsOps, operand int, b byte) (int, string) {
	d := operand & MaskWord
	r := (d&0xFF00)>>8 | (d&0x00FF)<<8
	o.psw.NFrom('B', r&0xFF)
	o.psw.ZFrom('B', r&0xFF)
	o.psw.SetV(0)
	o.psw.SetC(0)
	return r, ""
}

func ssCLR(o *SsOps, operand int, b byte) (int, string) {
	o.psw.SetNZVC(0, 1, 0, 0)
	return byteMask(b, 0, operand), ""
}

// COM: N, Z; V cleared; C set
func ssCOM(o *SsOps, operand int, b byte) (int, string) {
	r := ^operand & sizeMask(b)
	setNZ(o.psw, b, r)
	o.psw.SetV(0)
	o.psw.SetC(1)
	return byteMask(b, r, operand), ""
}

// INC: V set if dst was the largest positive value; C unaffected.
// FIX: the Python's V test wrote the mode bits.
func ssINC(o *SsOps, operand int, b byte) (int, string) {
	r := (operand + 1) & sizeMask(b)
	setNZ(o.psw, b, r)
	o.psw.SetV(bit(r == signBit(b)))
	return byteMask(b, r, operand), ""
}

// DEC: V set if dst was the most negative value; C unaffected.
func ssDEC(o *SsOps, operand int, b byte) (int, string) {
	r := (operand - 1) & sizeMask(b)
	setNZ(o.psw, b, r)
	o.psw.SetV(bit(r == signBit(b)-1))
	return byteMask(b, r, operand), ""
}

// NEG: V set if result is the most negative value; C set unless result is 0.
// FIX: the Python stored a negative Python int and never set V or C.
func ssNEG(o *SsOps, operand int, b byte) (int, string) {
	r := (-operand) & sizeMask(b)
	setNZ(o.psw, b, r)
	o.psw.SetV(bit(r == signBit(b)))
	o.psw.SetC(bit(r != 0))
	return byteMask(b, r, operand), ""
}

// ADC.  FIX: the Python set no condition codes.
func ssADC(o *SsOps, operand int, b byte) (int, string) {
	c := o.psw.C()
	r := (operand + c) & sizeMask(b)
	setNZ(o.psw, b, r)
	o.psw.SetV(bit(c == 1 && r == signBit(b)))
	o.psw.SetC(bit(c == 1 && r == 0))
	return byteMask(b, r, operand), ""
}

// SBC.  FIX: the Python set no condition codes.
func ssSBC(o *SsOps, operand int, b byte) (int, string) {
	c := o.psw.C()
	r := (operand - c) & sizeMask(b)
	setNZ(o.psw, b, r)
	o.psw.SetV(bit(c == 1 && r == signBit(b)-1))
	o.psw.SetC(bit(c == 1 && r == sizeMask(b)))
	return byteMask(b, r, operand), ""
}

func ssTST(o *SsOps, operand int, b byte) (int, string) {
	setNZ(o.psw, b, operand&sizeMask(b))
	o.psw.SetV(0)
	o.psw.SetC(0)
	return operand, ""
}

// ROR: C into the sign bit, bit 0 into C; V = N xor C.
// FIX: the Python's byte version rotated the high byte too, and the word
// version took C from the result.
func ssROR(o *SsOps, operand int, b byte) (int, string) {
	d := operand & sizeMask(b)
	r := d >> 1
	if o.psw.C() == 1 {
		r |= signBit(b)
	}
	c := d & 1
	setNZ(o.psw, b, r)
	o.psw.SetV(o.psw.N() ^ c)
	o.psw.SetC(c)
	return byteMask(b, r, operand), ""
}

// ROL: the sign bit into C, C into bit 0; V = N xor C.
// FIX: the Python ignored the carry and its byte version tested bit 21.
func ssROL(o *SsOps, operand int, b byte) (int, string) {
	d := operand & sizeMask(b)
	r := (d<<1 | o.psw.C()) & sizeMask(b)
	c := bit(d&signBit(b) != 0)
	setNZ(o.psw, b, r)
	o.psw.SetV(o.psw.N() ^ c)
	o.psw.SetC(c)
	return byteMask(b, r, operand), fmt.Sprintf("ROL(%s)=%s", PyBin(operand), PyBin(r))
}

// ASR: the sign bit is replicated; C is the old bit 0; V = N xor C.
func ssASR(o *SsOps, operand int, b byte) (int, string) {
	d := operand & sizeMask(b)
	r := d>>1 | d&signBit(b)
	c := d & 1
	setNZ(o.psw, b, r)
	o.psw.SetV(o.psw.N() ^ c)
	o.psw.SetC(c)
	return byteMask(b, r, operand), ""
}

// ASL: 0 into bit 0; C is the old sign bit; V = N xor C.
// FIX: the Python tested bit 45 for C, and its byte version raised TypeError.
func ssASL(o *SsOps, operand int, b byte) (int, string) {
	d := operand & sizeMask(b)
	r := (d << 1) & sizeMask(b)
	c := bit(d&signBit(b) != 0)
	setNZ(o.psw, b, r)
	o.psw.SetV(o.psw.N() ^ c)
	o.psw.SetC(c)
	return byteMask(b, r, operand), ""
}

// SXT: dst <- 0 if N clear, -1 if N set; Z = !N; V cleared.
// FIX: the Python stored 1 for -1 and did not clear V.
func ssSXT(o *SsOps, operand int, b byte) (int, string) {
	n := o.psw.N()
	o.psw.SetZ(n ^ 1)
	o.psw.SetV(0)
	if n == 1 {
		return MaskWord, ""
	}
	return 0, ""
}

func ssUnimplemented(report string) ssInstruction {
	return func(o *SsOps, operand int, b byte) (int, string) { return operand, report }
}

type ssEntry struct {
	fn   ssInstruction
	name string
	text string
	// how the destination is used: read and written (default), written
	// only (no read), read only (no write), or neither (unimplemented)
	writeOnly, readOnly, none bool
}

var singleOperandInstructions = map[int]ssEntry{
	0000100: {fn: ssJMP, name: "JMP", text: "jump"},
	0000300: {fn: ssSWAB, name: "SWAB", text: "swap bytes"},

	0005000: {fn: ssCLR, name: "CLR", text: "clear destination", writeOnly: true},
	0005100: {fn: ssCOM, name: "COM", text: "complement"},
	0005200: {fn: ssINC, name: "INC", text: "increment"},
	0005300: {fn: ssDEC, name: "DEC", text: "decrement"},
	0005400: {fn: ssNEG, name: "NEG", text: "negative"},
	0005500: {fn: ssADC, name: "ADC", text: "add carry"},
	0005600: {fn: ssSBC, name: "SBC", text: "subtract carry"},
	0005700: {fn: ssTST, name: "TST", text: "test", readOnly: true},
	0006000: {fn: ssROR, name: "ROR", text: "rotate right"},
	0006100: {fn: ssROL, name: "ROL", text: "rotate left"},
	0006200: {fn: ssASR, name: "ASR", text: "arithmetic shift right"},
	0006300: {fn: ssASL, name: "ASL", text: "arithmetic shift left"},
	0006400: {fn: ssUnimplemented("MARK **** not implemented"), name: "MARK", text: "subroutine cleanup", none: true},
	0006500: {fn: ssUnimplemented("MFPI NOT IMPLEMENTED"), name: "MFPI", text: "move from previous instruction space", none: true},
	0006600: {fn: ssUnimplemented("MTPI NOT IMPLEMENTED"), name: "MTPI", text: "move to previous instruction space", none: true},
	0006700: {fn: ssSXT, name: "SXT", text: "sign extend", writeOnly: true},

	0105000: {fn: ssCLR, name: "CLRB", text: "clear destination byte", writeOnly: true},
	0105100: {fn: ssCOM, name: "COMB", text: "complement byte"},
	0105200: {fn: ssINC, name: "INCB", text: "increment byte"},
	0105300: {fn: ssDEC, name: "DECB", text: "decrement byte"},
	0105400: {fn: ssNEG, name: "NEGB", text: "negative byte"},
	0105500: {fn: ssADC, name: "ADCB", text: "add carry byte"},
	0105600: {fn: ssSBC, name: "SBCB", text: "subtract carry byte"},
	0105700: {fn: ssTST, name: "TSTB", text: "test byte", readOnly: true},
	0106000: {fn: ssROR, name: "RORB", text: "rotate right byte"},
	0106100: {fn: ssROL, name: "ROLB", text: "rotate left byte"},
	0106200: {fn: ssASR, name: "ASRB", text: "arithmetic shift right byte"},
	0106300: {fn: ssASL, name: "ASLB", text: "arithmetic shift left byte"},
	0106400: {fn: ssUnimplemented("MTPS **** not implemented"), name: "MTPS", text: "move to PSW", none: true},
	0106500: {fn: ssUnimplemented("MFPD NOT IMPLEMENTED"), name: "MFPD", text: "move from previous data space", none: true},
	0106600: {fn: ssUnimplemented("MTPD NOT IMPLEMENTED"), name: "MTPD", text: "move to previous data space", none: true},
	0106700: {fn: ssUnimplemented("MFPS NOT IMPLEMENTED"), name: "MFPS", text: "move from psw", none: true},
}

// SingleOperandName returns the mnemonic for instruction & 0o107700.
func SingleOperandName(opcode int) (string, bool) {
	e, ok := singleOperandInstructions[opcode]
	return e.name, ok
}

func (o *SsOps) IsSsOp(instruction int) bool {
	// * 000 101 *** *** ***  or  * 000 110 *** *** ***, plus JMP and SWAB
	bits1412 := instruction&0070000 == 0
	b119 := instruction & 0007000
	isJMP := instruction&0177700 == 0000100
	isSWAB := instruction&0177700 == 0000300
	return (bits1412 && (b119 == 0006000 || b119 == 0005000)) || isSWAB || isJMP
}

func (o *SsOps) DoSsOp(instruction int) OpResult {
	o.sw.Start("ss")
	defer o.sw.Stop("ss")
	var b byte
	if instruction&0100000 != 0 {
		b = 'B'
	}
	source := instruction & 077
	opcode := instruction & 0107700
	e, ok := singleOperandInstructions[opcode]
	if !ok {
		raise("KeyError", "single_operand_instruction_names")
	}
	r := OpResult{Run: true}
	if opcode == 0000100 {
		run, target, word, asm := o.am.Jump(source)
		r.Run, r.Operand1, r.Assembly = run, word, e.name+" "+asm
		if run { // FIX: an illegal JMP Rn halts without jumping to 0
			_, r.Report = e.fn(o, target, b)
		}
		return r
	}
	// FIX: CLR and SXT write without reading the destination; TST reads
	// without writing; the unimplemented stubs neither read nor write.
	var d Operand
	if e.writeOnly || e.none {
		d = o.am.Address(b, source)
	} else {
		d = o.am.Get(b, source)
	}
	r.Operand1, r.Assembly = d.OperandWord, e.name+" "+d.Assembly
	result, report := e.fn(o, d.Value, b)
	r.Report = report
	if !e.readOnly && !e.none {
		o.am.Set(b, d, result)
	}
	return r
}
