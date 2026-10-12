// hardware_test.go - translation of test_hardware_reg, test_hardware_ram,
// test_hardware_psw and test_stack.
package pdp11

import "testing"

// ---------------------------------------------------------- test_hardware_reg

var regFx = newFixture()

func TestRegisters(t *testing.T) {
	for i := 0; i < 7; i++ {
		v := 0123223 + i
		regFx.reg.Set(i, v)
		eq(t, regFx.reg.Get(i), v, "register")
	}
}

func TestPC(t *testing.T) {
	v := 0165250
	regFx.reg.SetPC(v)
	eq(t, regFx.reg.GetPC(), v, "pc")
	regFx.reg.IncPC()
	eq(t, regFx.reg.GetPC(), v+2, "pc+2")
	regFx.reg.SetPC(v)
	regFx.reg.IncPC()
	eq(t, regFx.reg.GetPC(), v+2, "pc+2 again")
}

func TestSetPC2xOffset(t *testing.T) {
	// relies on TestPC having left PC at 0o165252
	regFx.reg.SetPC2xOffset(0123)
	eq(t, regFx.reg.GetPC(), 0165250+2+0123*2, "forward")
	regFx.reg.SetPC2xOffset(0277)
	eq(t, regFx.reg.GetPC(), 0165316, "backward")
}

func TestSP(t *testing.T) {
	regFx.reg.SetSP(0123221)
	eq(t, regFx.reg.GetSP(), 0123221, "sp")
}

// ---------------------------------------------------------- test_hardware_ram

var ramReg = NewRegisters()
var ramRam = NewRam(&Lock{}, ramReg, 16)

func TestRamSizes(t *testing.T) {
	for _, c := range []struct{ bits, top, io int }{
		{16, 0177777, 0160000}, {18, 0777777, 0760000}, {24, 077777777, 077760000}} {
		r := NewRam(&Lock{}, ramReg, c.bits)
		eq(t, r.TopOfMemory, c.top, "top_of_memory")
		eq(t, r.IOSpace, c.io, "io_space")
	}
}

func TestRamByte(t *testing.T) {
	for _, v := range []int{0107, 0270} {
		ramRam.WriteByteAt(04123, v)
		eq(t, int(ramRam.Memory[04123]), v, "memory")
		eq(t, ramRam.ReadByteAt(04123), v, "read_byte")
	}
}

func TestRamBytes(t *testing.T) {
	top := ramRam.TopOfMemory - 1
	ramRam.WriteByteAt(top, 0105)
	eq(t, ramRam.ReadByteAt(top), 0105, "top byte")
	for i := 0; i < 0277; i++ {
		ramRam.WriteByteAt(i, i)
	}
	for i := 0; i < 0277; i += 2 {
		eq(t, ramRam.ReadByteAt(i), i, "byte")
	}
}

func TestRamWords(t *testing.T) {
	for _, c := range []struct{ value, address int }{{0107070, 06234}, {0170707, 06234}, {0123232, ramRam.TopOfMemory - 1}} {
		ramRam.WriteWord(c.address, c.value)
		eq(t, int(ramRam.Memory[c.address]), c.value&MaskLowByte, "low byte")
		eq(t, int(ramRam.Memory[c.address+1]), (c.value&MaskHighByte)>>8, "high byte")
		eq(t, ramRam.ReadWord(c.address), c.value, "read_word")
	}
	for i := 1000; i < 2000; i += 2 {
		ramRam.WriteWord(i, 0123234+i)
	}
	for i := 1000; i < 2000; i += 2 {
		eq(t, ramRam.ReadWord(i), 0123234+i, "word")
	}
}

func TestRamWordByte(t *testing.T) {
	base := 010000
	for v := 0; v < 030000; v += 2 {
		ramRam.WriteWord(base+v, v)
	}
	for v := 0; v < 030000; v += 2 {
		lo, hi := ramRam.ReadByteAt(base+v), ramRam.ReadByteAt(base+v+1)
		eq(t, lo, v&MaskLowByte, "low byte")
		eq(t, hi, (v&MaskHighByte)>>8, "high byte")
		eq(t, lo+hi<<8, v, "sum")
	}
}

// ---------------------------------------------------------- test_hardware_psw

var pswFx = newFixture()

func addByte(p *PSW, b1, b2 int) int {
	r := b1 + b2
	if r > r&MaskLowByte {
		p.SetV(1)
	}
	if r == 0 {
		p.SetZ(1)
	}
	return r & MaskLowByte
}

func TestPSWAddress(t *testing.T) {
	eq(t, pswFx.psw.PSWAddress, pswFx.ram.TopOfMemory-1, "psw address")
}

func TestPSWSet(t *testing.T) {
	pswFx.psw.SetPSW(0123236)
	eq(t, pswFx.psw.PSW, 0123236, "psw")
}

func TestPSWFields(t *testing.T) {
	p := pswFx.psw
	p.SetMode(0)
	p.SetMode(3)
	eq(t, p.PSW&0140000, 0140000, "mode")
	p.SetPriority(0)
	p.SetPriority(15)
	eq(t, p.PSW&0000340, 0000340, "priority")
	p.SetTrap(0)
	p.SetTrap(1)
	eq(t, p.PSW&0000020, 0000020, "trap")
	for _, f := range []struct {
		set  func(int)
		mask int
	}{{p.SetN, 010}, {p.SetZ, 004}, {p.SetV, 002}, {p.SetC, 001}} {
		f.set(0)
		f.set(1)
		eq(t, p.PSW&f.mask, f.mask, "condition bit")
	}
}

func TestPSWAddb(t *testing.T) {
	pswFx.psw.SetPSW(0)
	eq(t, addByte(pswFx.psw, 3, 4), 7, "sum")
	eq(t, pswFx.psw.V(), 0, "v")
	eq(t, pswFx.psw.Z(), 0, "z")
	pswFx.psw.SetPSW(0)
	eq(t, addByte(pswFx.psw, 191, 191), 126, "sum")
	eq(t, pswFx.psw.V(), 1, "v")
	eq(t, pswFx.psw.Z(), 0, "z")
}

// ---------------------------------------------------------- test_stack

var stackFx = newFixture()

func TestJSR_R5(t *testing.T) {
	f := stackFx
	pc, sp, sbr, data0, data1 := 01000, 02000, 03000, 01234, 012345
	f.reg.Set(7, pc)
	f.reg.Set(6, sp)
	f.ram.WriteWord(sp, data0)
	f.reg.Set(5, data1)
	f.ram.WriteWord(pc+2, sbr)
	opcode := 04000 | 5<<6 | 1<<3 | 7 // JSR R5,@R7
	f.ram.WriteWord(pc, opcode)
	check(t, f.other.IsOtherOp(opcode), "is_other_op")
	f.reg.Set(7, pc+2)
	f.other.DoOtherOp(opcode)
	eq(t, f.reg.Get(7), pc+2, "R7")
	eq(t, f.reg.Get(6), sp-2, "R6")
	eq(t, f.reg.Get(5), pc+2, "R5")
	eq(t, f.ram.ReadWord(f.reg.Get(6)), data1, "top of stack")
}

func TestRTS(t *testing.T) {
	f := stackFx
	pc, sp, sbr, data0, data1 := 01002, 02000-2, 03000, 01234, 012345
	f.reg.Set(7, sbr)
	f.reg.Set(6, sp)
	f.ram.WriteWord(sp, data1)
	f.ram.WriteWord(sp+2, data0)
	opcode := 0000200 | 5
	f.ram.WriteWord(pc, opcode)
	check(t, f.other.IsOtherOp(opcode), "is_other_op")
	f.reg.Set(7, pc+2)
	f.other.DoOtherOp(opcode)
	eq(t, f.reg.Get(7), pc, "R7")
	eq(t, f.reg.Get(6), sp+2, "R6")
	eq(t, f.reg.Get(5), data1, "R5")
	eq(t, f.ram.ReadWord(f.reg.Get(6)), data0, "top of stack")
}
