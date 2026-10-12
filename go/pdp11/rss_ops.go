// rss_ops.go - translation of pdp11_rss_ops (EIS, XOR, SOB: 07 0R SS - 07 7R SS).
//
// Semantics follow the DEC processor handbook, and SIMH for the DIV error
// flags, as in the fixed C version; FIX comments say what the Python did.
package pdp11

import "fmt"

type RssOps struct {
	reg *Registers
	psw *PSW
	am  *AddressModes
	sw  *StopWatches
}

func NewRssOps(reg *Registers, psw *PSW, am *AddressModes, sw *StopWatches) *RssOps {
	logInfof("initializing doubleOperandOps")
	return &RssOps{reg, psw, am, sw}
}

var rssNames = map[int]string{
	0070000: "MUL", 0071000: "DIV", 0072000: "ASH", 0073000: "ASHC", 0074000: "XOR", 0077000: "SOB",
}

// RssName returns the mnemonic for instruction & 0o077000.
func RssName(opcode int) (string, bool) {
	n, ok := rssNames[opcode]
	return n, ok
}

func sx16(v int) int {
	v &= MaskWord
	if v&MaskWordMSB != 0 {
		return v - 0200000
	}
	return v
}

func sx32(v int) int {
	v &= MaskLong
	if v&MaskLongMSB != 0 {
		return v - 040000000000
	}
	return v
}

// MUL: (R, R|1) <- R * src.  High word to R, low to R|1 (an odd R gets the
// low word).  C set if the product does not fit in 16 bits.
// FIX: the Python never set C.
func (o *RssOps) MUL(r, source int) {
	p := sx16(o.reg.Get(r)) * sx16(source)
	o.reg.Set(r, p>>16)
	o.reg.Set(r|1, p)
	o.psw.SetNZVC(bit(p < 0), bit(p == 0), 0, bit(p > 077777 || p < -0100000))
}

// DIV: (R, R|1) / src.  Truncates toward zero; the remainder takes the
// dividend's sign.  Divide by zero sets V and C, quotient overflow sets V;
// both leave the registers unchanged.
// FIX: the Python tested the numerator for zero, raised on a zero divisor,
// and its quotient correction was wrong for exact and doubly negative cases.
func (o *RssOps) DIV(r, denominator int) string {
	num := o.reg.Get(r)<<16 | o.reg.Get(r|1)
	den := denominator & MaskWord
	if den == 0 {
		o.psw.SetNZVC(0, 1, 1, 1)
		return "DIV divide by zero"
	}
	n, d := sx32(num), sx16(den)
	q := n / d // Go integer division truncates toward zero, as the PDP-11 does
	if q > 077777 || q < -0100000 {
		o.psw.SetNZVC(bit(q < 0), 0, 1, 0)
		return "DIV overflow"
	}
	o.reg.Set(r, q)
	o.reg.Set(r|1, n-d*q)
	o.psw.SetNZVC(bit(q < 0), bit(q == 0), 0, 0)
	return ""
}

// arithShift shifts a value of the given width by count (low 6 bits, -32..31).
// v reports a sign change during a left shift, c the last bit shifted out.
func arithShift(value, bits, count int) (result, v, c int) {
	mask, sign := (1<<bits)-1, 1<<(bits-1)
	n := count & 077
	value &= mask
	if n&040 != 0 { // right shift by 64 - n
		for i := 0; i < 0100-n; i++ {
			c = value & 1
			value = value>>1 | value&sign
		}
	} else {
		for i := 0; i < n; i++ {
			c = bit(value&sign != 0)
			shifted := (value << 1) & mask
			if shifted&sign != value&sign {
				v = 1
			}
			value = shifted
		}
	}
	return value, v, c
}

// ASH.  FIX: the Python took the sign from the register number and never set C.
func (o *RssOps) ASH(r, source int) {
	res, v, c := arithShift(o.reg.Get(r), 16, source)
	o.reg.Set(r, res)
	o.psw.SetNZVC(bit(res&MaskWordMSB != 0), bit(res == 0), v, c)
}

// ASHC: (R, R|1) as one 32-bit value; for odd R a right shift rotates R.
// FIX: the Python shifted R right by the whole source value.
func (o *RssOps) ASHC(r, source int) {
	value := o.reg.Get(r)<<16 | o.reg.Get(r|1)
	res, v, c := arithShift(value, 32, source)
	o.reg.Set(r, res>>16)
	o.reg.Set(r|1, res)
	o.psw.SetNZVC(bit(res&MaskLongMSB != 0), bit(res == 0), v, c)
}

// SOB: R <- R - 1; if R != 0, PC <- PC - 2 * NN.
// FIX: the Python multiplied R by the operand.
func (o *RssOps) SOB(r, offset int) {
	v := (o.reg.Get(r) - 1) & MaskWord
	o.reg.Set(r, v)
	if v != 0 {
		o.reg.SetPC(o.reg.GetPC() - 2*(offset&077))
	}
}

func (o *RssOps) IsRssOp(instruction int) bool {
	if instruction&0100000 != 0 || instruction&0070000 != 0070000 {
		return false
	}
	_, ok := rssNames[instruction&0077000]
	return ok
}

// DoRssOp dispatches; each instruction stores its own results.
// FIX: the Python fetched the source as a byte and stored every result in R.
func (o *RssOps) DoRssOp(instruction int) OpResult {
	o.sw.Start("rss")
	defer o.sw.Stop("rss")
	opcode := instruction & 0077000
	r := (instruction & 0700) >> 6
	source := instruction & 077
	name, ok := rssNames[opcode]
	if !ok {
		raise("KeyError", "double_operand_RSS_instruction_names")
	}
	res := OpResult{Run: true}
	switch opcode {
	case 0077000: // SOB R,NN
		res.Assembly = fmt.Sprintf("%s R%d,%s", name, r, PyOct(source))
		o.SOB(r, source)
	case 0074000: // XOR R,DD
		// R is read before the destination is evaluated, so XOR R1,(R1)+
		// uses R1's value from before the increment (as on the 11/03).
		// FIX: the Python wrote the result into R and did not clear V.
		rvalue := o.reg.Get(r)
		d := o.am.Get('W', source)
		res.Assembly = fmt.Sprintf("%s R%d,%s", name, r, d.Assembly)
		res.Operand1 = d.OperandWord
		result := (rvalue ^ d.Value) & MaskWord
		o.psw.SetNZVC(bit(result&MaskWordMSB != 0), bit(result == 0), 0, -1)
		o.am.Set('W', d, result)
	default: // MUL/DIV/ASH/ASHC SS,R
		d := o.am.Get('W', source)
		res.Assembly = fmt.Sprintf("%s %s,R%d", name, d.Assembly, r)
		res.Operand1 = d.OperandWord
		switch opcode {
		case 0070000:
			o.MUL(r, d.Value)
		case 0071000:
			res.Report = o.DIV(r, d.Value)
		case 0072000:
			o.ASH(r, d.Value)
		case 0073000:
			o.ASHC(r, d.Value)
		}
	}
	return res
}
