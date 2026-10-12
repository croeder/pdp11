// address_modes_test.go - translation of test_address_modes.
package pdp11

import "testing"

var amFx = newFixture()

func addOp(modeS, regS, modeD, regD int) int { return ssddOp(0060000, modeS, regS, modeD, regD) }

func amDo(t *testing.T, instruction int) {
	t.Helper()
	check(t, amFx.ssdd.IsSsddOp(instruction), "is_ssdd_op")
	r := amFx.ssdd.DoSsddOp(instruction)
	logInfof("%s", r.Assembly)
}

func amNZVC(t *testing.T, want string) { t.Helper(); eq(t, amFx.psw.NZVC(), want, "nzvc") }

// Mode 0
func TestMode0SD(t *testing.T) {
	f := amFx
	a, b := 3, 1
	f.psw.SetPSW(0)
	f.reg.Set(1, a)
	f.reg.Set(2, b)
	amDo(t, addOp(0, 1, 0, 2))
	eq(t, f.reg.Get(2), a+b, "R2")
	amNZVC(t, "0000")
}

// Mode 1 Register Deferred
func TestMode1S(t *testing.T) {
	f := amFx
	address, a, b := 02764, 0101, 02136
	f.ram.WriteWord(address, a)
	f.reg.Set(1, address)
	f.reg.Set(2, b)
	amDo(t, addOp(1, 1, 0, 2))
	eq(t, f.reg.Get(2), a+b, "R2")
	amNZVC(t, "0000")
}

func TestMode1D(t *testing.T) {
	f := amFx
	address, a, b := 02764, 0101, 02136
	f.ram.WriteWord(address, a)
	f.reg.Set(2, address)
	f.reg.Set(1, b)
	amDo(t, addOp(0, 1, 1, 2))
	eq(t, f.ram.ReadWord(address), a+b, "memory")
	amNZVC(t, "0000")
}

func TestMode1SD(t *testing.T) {
	f := amFx
	addressA, A, addressB, B := 02762, 0101, 01234, 02136
	f.ram.WriteWord(addressA, A)
	f.reg.Set(1, addressA)
	f.ram.WriteWord(addressB, B)
	f.reg.Set(2, addressB)
	amDo(t, addOp(1, 1, 1, 2))
	eq(t, f.ram.ReadWord(addressB), A+B, "memory")
	amNZVC(t, "0000")
}

// Mode 2 Autoincrement
func TestMode2S(t *testing.T) {
	f := amFx
	address, a, b := 02762, 0101, 02136
	f.ram.WriteWord(address, a)
	f.reg.Set(1, address)
	f.reg.Set(2, b)
	amDo(t, addOp(2, 1, 0, 2))
	eq(t, f.reg.Get(2), a+b, "R2")
	eq(t, f.reg.Get(1), address+2, "R1")
	amNZVC(t, "0000")
}

func TestMode2D(t *testing.T) {
	f := amFx
	a, b, address := 0101, 02136, 02762
	f.reg.Set(1, a)
	f.ram.WriteWord(address, b)
	f.reg.Set(2, address)
	amDo(t, addOp(0, 1, 2, 2))
	eq(t, f.ram.ReadWord(address), a+b, "memory")
	eq(t, f.reg.Get(2), address+2, "R2")
	amNZVC(t, "0000")
}

func TestMode2SD(t *testing.T) {
	f := amFx
	a, b, src, dst := 0101, 02136, 02762, 01312
	f.reg.Set(1, src)
	f.ram.WriteWord(src, a)
	f.reg.Set(2, dst)
	f.ram.WriteWord(dst, b)
	amDo(t, addOp(2, 1, 2, 2))
	eq(t, f.ram.ReadWord(dst), a+b, "memory")
	eq(t, f.reg.Get(1), src+2, "R1")
	eq(t, f.reg.Get(2), dst+2, "R2")
}

// Mode 3 Autoincrement Deferred.  As in the Python, this test uses mode 2
// for the source.
func TestMode3S(t *testing.T) {
	f := amFx
	address, a, b := 02762, 0101, 02136
	f.ram.WriteWord(address, a)
	f.reg.Set(1, address)
	f.reg.Set(2, b)
	amDo(t, addOp(2, 1, 0, 2))
	eq(t, f.reg.Get(2), a+b, "R2")
	eq(t, f.reg.Get(1), address+2, "R1")
	amNZVC(t, "0000")
}

// Mode 4 Autodecrement -(Rn)
func TestMode4(t *testing.T) {
	f := amFx
	address, a, b := 02762, 0101, 02136
	f.ram.WriteWord(address, a)
	f.reg.Set(1, address+2)
	f.reg.Set(2, b)
	amDo(t, addOp(4, 1, 0, 2))
	eq(t, f.reg.Get(2), a+b, "R2")
	eq(t, f.reg.Get(1), address, "R1")
	amNZVC(t, "0000")
}

// Mode 5 Autodecrement Deferred
func TestMode5(t *testing.T) {
	f := amFx
	operanda, addressa, pointera, operandb := 0101, 02762, 01172, 02136
	f.ram.WriteWord(addressa, operanda)
	f.ram.WriteWord(pointera, addressa)
	f.reg.Set(1, pointera+2)
	f.reg.Set(2, operandb)
	amDo(t, addOp(5, 1, 0, 2))
	eq(t, f.reg.Get(2), operanda+operandb, "R2")
	eq(t, f.reg.Get(1), pointera, "R1")
	amNZVC(t, "0000")
}

// Mode 6 Index X(Rn); also used for test_PC_mode_6, which is the same test.
func indexTest(t *testing.T) {
	f := amFx
	instruction := addOp(6, 1, 0, 2)
	check(t, f.ssdd.IsSsddOp(instruction), "is_ssdd_op")
	ia, X, operanda, operandb := 02762, 02050, 0100, 02136
	f.ram.WriteWord(ia, instruction)
	f.ram.WriteWord(ia+2, X)
	f.reg.Set(1, operanda)
	f.ram.WriteWord(X+operanda, operanda)
	f.reg.Set(2, operandb)
	f.reg.SetPC(ia + 2)
	amDo(t, instruction)
	eq(t, f.reg.Get(2), operanda+operandb, "R2")
	amNZVC(t, "0000")
}

func TestMode6(t *testing.T) { indexTest(t) }

// Mode 7 Index Deferred @X(Rn)
func TestMode7(t *testing.T) {
	f := amFx
	instruction := addOp(7, 1, 0, 2)
	ia, X, operanda, operandAddress, operandb := 02762, 02050, 0100, 05432, 02136
	f.ram.WriteWord(ia, instruction)
	f.ram.WriteWord(ia+2, X)
	f.reg.Set(1, operanda)
	f.ram.WriteWord(operandAddress, operanda)
	f.ram.WriteWord(X+operanda, operandAddress)
	f.reg.Set(2, operandb)
	f.reg.SetPC(ia + 2)
	amDo(t, instruction)
	eq(t, f.reg.Get(2), operanda+operandb, "R2")
	amNZVC(t, "0000")
}

// Program Counter and Address Modes

func TestPCMode0(t *testing.T) { TestMode0SD(t) }

func TestPCMode2(t *testing.T) {
	f := amFx
	// ADD #10,R0
	instruction := addOp(2, 7, 0, 0)
	eq(t, instruction, 0062700, "instruction")
	address := 01020
	f.ram.WriteWord(address, instruction)
	f.ram.WriteWord(address+2, 0000010)
	f.reg.Set(0, 0000020)
	f.reg.SetPC(address + 2)
	amDo(t, instruction)
	eq(t, f.reg.Get(0), 0000030, "R0")
	eq(t, f.reg.GetPC(), 01024, "PC")
	amNZVC(t, "0000")
}

func TestPCMode3(t *testing.T) {
	f := amFx
	// ADD @#2000,R3
	instruction := addOp(3, 7, 0, 3)
	eq(t, instruction, 0063703, "instruction")
	a, b, pc, address := 000300, 000500, 020, 02000
	f.ram.WriteWord(pc, instruction)
	f.ram.WriteWord(pc+2, address)
	f.ram.WriteWord(address, a)
	f.reg.Set(3, b)
	f.reg.SetPC(pc + 2)
	amDo(t, instruction)
	eq(t, f.reg.Get(3), a+b, "R3")
	eq(t, f.reg.GetPC(), pc+4, "PC")
	amNZVC(t, "0000")
}

func TestPCMode4(t *testing.T) { TestMode4(t) }

func TestPCMode5(t *testing.T) {
	f := amFx
	// INC A. See p. 3-16 of LSI-11 PDP-11/03 Processor Handbook
	instruction := 0005267
	check(t, f.ss.IsSsOp(instruction), "is_ss_op")
	a, pc, address := 0000054, 01020, 01100
	f.ram.WriteWord(pc, instruction)
	f.ram.WriteWord(pc+2, a)
	f.ram.WriteWord(address, 0)
	f.reg.SetPC(pc + 2)
	f.ss.DoSsOp(instruction)
	eq(t, f.reg.GetPC(), 01024, "PC")
	eq(t, f.ram.ReadWord(address), 1, "A")
	amNZVC(t, "0000")
}

func TestPCMode6(t *testing.T) { indexTest(t) }

func TestPCMode7(t *testing.T) {
	f := amFx
	// CLR @A. See p. 3-16 of LSI-11 PDP-11/03 Processor Handbook
	instruction := 0005077
	check(t, f.ss.IsSsOp(instruction), "is_ss_op")
	a, pc, pointer, address := 0000020, 01020, 01044, 01060
	f.ram.WriteWord(pc, instruction)
	f.ram.WriteWord(pc+2, a)
	f.ram.WriteWord(pointer, address)
	f.ram.WriteWord(address, 0100001)
	f.reg.SetPC(pc + 2)
	f.ss.DoSsOp(instruction)
	eq(t, f.reg.GetPC(), 01024, "PC")
	eq(t, f.ram.ReadWord(address), 0, "A")
	amNZVC(t, "0100")
}

// ---------------------------------------------------------- test_condition_code_ops
// pdp11-40 ref p. 4-79

var ccFx = newFixture()

func TestSetOpcodes(t *testing.T) {
	for code := 01; code <= 017; code++ {
		ccFx.psw.SetPSW(0)
		instruction := 0000260 | code
		check(t, ccFx.cc.IsCcOp(instruction), "is_cc_op")
		ccFx.cc.DoCcOp(instruction)
		eq(t, ccFx.psw.Get()&017, code, "SEx")
	}
}

func TestClrOpcodes(t *testing.T) {
	for code := 0; code <= 017; code++ {
		ccFx.psw.SetPSW(017)
		instruction := 0000240 | code
		check(t, ccFx.cc.IsCcOp(instruction), "is_cc_op")
		ccFx.cc.DoCcOp(instruction)
		eq(t, ccFx.psw.Get()&017, 017&^code, "CLx")
	}
}
