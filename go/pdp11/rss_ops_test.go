// rss_ops_test.go - translation of test_rss_ops and test_jump.
package pdp11

import "testing"

var rssFx = newFixture()

func rssOp(opcode, regD, modeS, regS int) int { return opcode | regD<<6 | dd(modeS, regS) }

func TestRssRegister(t *testing.T) {
	runRR(t, rssFx, []rrCase{
		{"BIC", 0, 1, 0xAAAA, 2, 0xFFFF, 0040000, "BIC R1,R2", 0x5555, "0000"},
		{"BICB", 0, 1, 0xAAAA, 2, 0xFFFF, 0140000, "BICB R1,R2", 0xFF55, "0000"},
		{"BIC_2", 0777777, 3, 0001234, 4, 0001111, 0040000, "BIC R3,R4", 0000101, "0001"},
		{"BICB_2", 0, 3, 0001234, 4, 0001111, 0140000, "BICB R3,R4", 0001101, "0000"},
		{"MOV_0", 0777777, 3, 0123456, 4, 0, 0010000, "MOV R3,R4", 0123456, "1001"},
	})
}

func TestMOV2(t *testing.T) {
	f := rssFx
	// 010322 MOV R3,(R2)+ from M9001_YA that broke
	f.psw.SetPSW(0777777)
	f.reg.Set(3, 0177777)
	f.reg.Set(2, 0)
	ins := ssddOp(0010000, 0, 3, 2, 2)
	check(t, f.ssdd.IsSsddOp(ins), "is_ssdd_op")
	eq(t, f.ssdd.DoSsddOp(ins).Assembly, "MOV R3,(R2)+", "assembly")
	eq(t, f.reg.Get(3), 0177777, "R3")
	eq(t, f.reg.Get(2), 2, "R2")
	eq(t, f.ram.ReadWord(0), 0177777, "@0")
	eq(t, f.psw.NZVC(), "1001", "nzvc")
}

func TestRssMOVB(t *testing.T) {
	runRR(t, rssFx, []rrCase{
		{"MOVB_01", 0777777, 3, 0xA72E, 4, 0, 0110000, "MOVB R3,R4", 0x002E, "0001"},
		{"MOVB_02", 0777777, 3, 0xA7AE, 4, 0, 0110000, "MOVB R3,R4", 0xFFAE, "1001"},
		{"MOVB_03", 0777777, 3, 0x00AE, 4, 0xA700, 0110000, "MOVB R3,R4", 0xFFAE, "1001"},
	})
}

const (
	mulP  = 0764 // 500
	mulp  = 0265 // 181
	mulp1 = 1
	// There are limits on what sorts of numbers can be divided without
	// blowing things up.
	smlPos = 044276
	bigPos = 0042400042
)

func TestMUL(t *testing.T) {
	f := rssFx
	neg := TwosComplementNegative
	P, p, p1, z := mulP, mulp, mulp1, 0
	N, n, n1 := neg(P), neg(p), neg(p1)
	for _, c := range []struct {
		name   string
		s, d   int
		rs, rd int
		cc     string
	}{
		{"pp", p, p, 2, 4, "0000"}, {"pz", p, z, 2, 4, "0100"}, {"pn", p, n, 2, 4, "1000"},
		{"zp", z, p, 2, 4, "0100"}, {"zz", z, z, 2, 4, "0100"}, {"zn", z, n, 2, 4, "0100"},
		{"p1p1", p1, p1, 2, 4, "0000"}, {"p1n1", p1, n1, 2, 4, "1000"},
		{"n1p1", n1, p1, 2, 4, "1000"}, {"n1n1", n1, n1, 2, 4, "0000"},
		{"np", n, p, 2, 4, "1000"}, {"nz", n, z, 2, 4, "0100"}, {"nn", n, n, 2, 4, "0000"},
		// C is set when the product does not fit in 16 bits, which is every
		// nonzero case with a P or N factor (Python: C always 0)
		{"pP", p, P, 2, 4, "0001"}, {"pN", p, N, 2, 4, "1001"},
		{"zP", z, P, 2, 4, "0100"}, {"zN", z, N, 2, 4, "0100"},
		{"nP", n, P, 2, 4, "1001"}, {"nN", n, N, 2, 4, "0001"},
		{"Pp", P, p, 2, 4, "0001"}, {"Pz", P, z, 2, 4, "0100"}, {"Pn", P, n, 2, 4, "1001"},
		{"Np", N, p, 2, 4, "1001"}, {"Nz", N, z, 2, 4, "0100"}, {"Nn", N, n, 2, 4, "0001"},
		{"PP", P, P, 2, 4, "0001"}, {"PN", P, N, 2, 4, "1001"},
		{"NP", N, P, 2, 4, "1001"}, {"NN", N, N, 2, 4, "0001"},
	} {
		t.Run(c.name, func(t *testing.T) {
			f.psw.SetPSW(0)
			f.reg.Set(c.rd, c.d)
			f.reg.Set(c.rs, c.s)
			ins := rssOp(0070000, c.rd, 0, c.rs)
			check(t, f.rss.IsRssOp(ins), "is_rss_op")
			f.rss.DoRssOp(ins)
			want := PDP11LongifyPythonInteger(PythonifyPDP11Word(c.s) * PythonifyPDP11Word(c.d))
			got := f.reg.Get(c.rd)<<16 | f.reg.Get(c.rd+1)
			eq(t, got, want, "product")
			eq(t, f.psw.NZVC(), c.cc, "nzvc")
		})
	}
}

func TestDIV(t *testing.T) {
	f := rssFx
	smlNeg := TwosComplementNegative(smlPos)
	bigNeg := TwosComplementNegativeLong(bigPos)
	for _, c := range []struct {
		name                   string
		numerator, denominator int
		rs, rd                 int
		cc                     string
	}{
		{"Pp", bigPos, smlPos, 2, 4, "0000"},
		{"Pn", bigPos, smlNeg, 2, 4, "1000"},
		{"Np", bigNeg, smlPos, 2, 4, "1000"},
		{"Nn", bigNeg, smlNeg, 2, 4, "0000"},
	} {
		t.Run(c.name, func(t *testing.T) {
			// DIV: (R, R+1) <- (R, R+1) / (src).  The PDP-11 truncates toward
			// zero and gives the remainder the sign of the dividend, as Go's
			// / and % do.  (The Python test expected 486 for case Nn, where
			// the quotient is 485.)
			num, den := PythonifyPDP11Long(c.numerator), PythonifyPDP11Word(c.denominator)
			check(t, den != 0, "denominator")
			rvl := c.rd + 1
			f.reg.Set(c.rd, c.numerator>>16)
			f.reg.Set(rvl, c.numerator&MaskWord)
			f.reg.Set(c.rs, c.denominator)
			f.psw.SetPSW(0)
			ins := rssOp(0071000, c.rd, 0, c.rs)
			check(t, f.rss.IsRssOp(ins), "is_rss_op")
			f.rss.DoRssOp(ins)
			eq(t, PythonifyPDP11Word(f.reg.Get(c.rd)), num/den, "quotient")
			eq(t, PythonifyPDP11Word(f.reg.Get(rvl)), num%den, "remainder")
			eq(t, f.psw.NZVC(), c.cc, "nzvc")
		})
	}
}

// ---------------------------------------------------------- test_jump
// pdp11-40.pdf page 4-56: PC<-(dst); bits 15-6 are 0o0001; bits 5-0 are
// mode, register.  JMP with mode 0 causes "illegal instruction" condition.

var jmpFx = newFixture()

func jmpOp(mode, register int) int { return 0000100 | dd(mode, register) }

func jmpRun(t *testing.T, instruction int) OpResult {
	t.Helper()
	check(t, jmpFx.ss.IsSsOp(instruction), "is_ss_op")
	return jmpFx.ss.DoSsOp(instruction)
}

func TestJmpMode0(t *testing.T) {
	pc := 01020
	jmpFx.reg.SetPC(pc)
	r := jmpRun(t, jmpOp(0, 0))
	// JMP to a register is illegal: the CPU stops without jumping (the
	// Python jumped to 0; real hardware traps, which is not emulated)
	eq(t, r.Run, false, "run")
	eq(t, jmpFx.reg.GetPC(), pc, "PC")
}

func TestJmpMode1(t *testing.T) {
	// Register Indirect - JMP (R1)
	f := jmpFx
	pc, target := 01000, 02000
	ins := jmpOp(1, 1)
	f.reg.SetPC(pc)
	f.reg.Set(1, target)
	f.ram.WriteWord(pc, ins)
	jmpRun(t, ins)
	eq(t, f.reg.GetPC(), target, "PC")
}

func TestJmpMode2PC(t *testing.T) {
	f := jmpFx
	pc, target := 01000, 02000
	ins := jmpOp(2, 7)
	f.reg.SetPC(pc)
	f.reg.Set(7, target)
	f.ram.WriteWord(pc, ins)
	jmpRun(t, ins)
	eq(t, f.reg.GetPC(), target, "PC")
}

func TestJmpMode2R(t *testing.T) {
	f := jmpFx
	pc, target := 01000, 02000
	ins := jmpOp(2, 1)
	f.reg.SetPC(pc)
	f.reg.Set(1, target)
	f.ram.WriteWord(pc, ins)
	jmpRun(t, ins)
	eq(t, f.reg.GetPC(), target, "PC")
	eq(t, f.reg.Get(1), target+2, "R1")
}

func TestJmpMode3(t *testing.T) {
	// JMP @#FOO: mode 3, register 7, the second word is the address of FOO.
	f := jmpFx
	pc, target := 01000, 02000
	ins := jmpOp(3, 7)
	f.ram.WriteWord(pc, ins)
	f.ram.WriteWord(pc+2, target)
	f.reg.SetPC(pc + 2)
	jmpRun(t, ins)
	eq(t, f.reg.GetPC(), target, "PC")
}

func TestJumpMode6(t *testing.T) {
	// Mode 6, Indexed - JMP 20(PC)
	f := jmpFx
	pc, target := 0001000, 0001200
	ins := jmpOp(6, 7)
	f.ram.WriteWord(pc, ins)
	f.ram.WriteWord(pc+2, target-pc-4)
	f.reg.SetPC(pc + 2)
	jmpRun(t, ins)
	eq(t, f.reg.GetPC(), target, "PC")
}

func TestJumpMode7(t *testing.T) {
	// As in the Python, this test runs CLR @A.
	f := jmpFx
	ins := 0005077
	a, pc, pointer, address := 0000020, 01020, 01044, 01060
	f.ram.WriteWord(pc, ins)
	f.ram.WriteWord(pc+2, a)
	f.ram.WriteWord(pointer, address)
	f.ram.WriteWord(address, 0100001)
	f.reg.SetPC(pc + 2)
	jmpRun(t, ins)
	eq(t, f.reg.GetPC(), 01024, "PC")
	eq(t, f.ram.ReadWord(address), 0, "A")
	eq(t, f.psw.NZVC(), "0100", "nzvc")
}
