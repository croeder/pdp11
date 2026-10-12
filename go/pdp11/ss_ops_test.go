// ss_ops_test.go - translation of test_ss_ops and test_ssdd_ops.
package pdp11

import "testing"

var ssFx = newFixture()

// byte_mask tests (both Python files call ssdd_ops.byte_mask)
func TestByteMask(t *testing.T) {
	for _, c := range []struct {
		b                   byte
		value, target, want int
	}{
		{'W', 0xAAAA, 0x5555, 0xAAAA},
		{'B', 0xFF00, 0x00FF, 0},
		{'B', 0x00FF, 0xFF00, 0xFFFF},
		{'B', 0xAAAA, 0x5555, 0x55AA},
		{'B', 0x0000, 0x5555, 0x5500},
	} {
		eq(t, ssFx.ssdd.ByteMask(c.b, c.value, c.target), c.want, "byte_mask")
	}
}

// The single-operand tests all follow one pattern: set the PSW and R1,
// run, and check the assembly, R1 and NZVC.
func TestSsOps(t *testing.T) {
	f := ssFx
	for _, c := range []struct {
		name         string
		psw, r1, ins int
		asm          string
		r1out        int
		nzvc         string
	}{
		{"CLR", 0, 0013333, 005001, "CLR R1", 0, "0100"},
		{"COM_1", 0, 0013333, 0005101, "COM R1", 0164444, "1001"},
		{"COM_2", 0, 0164444, 0005101, "COM R1", 0013333, "0001"},
		{"COMB_1", 0, 0000022, 0105101, "COMB R1", 0355, "1001"},
		{"INC", 0, 0013333, 005201, "INC R1", 0013334, "0000"},
		{"INCB", 0, 0000033, 0105201, "INCB R1", 0000034, "0000"},
		{"DEC_1", 0, 0013333, 005301, "DEC R1", 0013332, "0000"},
		{"DEC_2", 0, 1, 005301, "DEC R1", 0, "0100"},
		{"DEC_3", 0, 0, 005301, "DEC R1", 0177777, "1000"}, // decrement doesn't set overflow
		{"DEC_B1", 0, 1, 0105301, "DECB R1", 0, "0100"},
		{"SWAB_1", 0177777, 0077777, 0000301, "SWAB R1", 0177577, "0000"},
		{"SWAB_2", 0177777, 0177577, 0000301, "SWAB R1", 0077777, "1000"},
		{"SWAB_3", 0177777, 0000377, 0000301, "SWAB R1", 0177400, "0100"},
		{"SWAB_4", 0177777, 0177400, 0000301, "SWAB R1", 0000377, "1000"},
		{"ROR_1", 0177777, 0xFF00, 0006001, "ROR R1", 0xFF80, "1010"}, // C=1 rotates into bit 15 (Python: 0x7F80, "0000")
		{"ROR_2", 0177777, 0x00FF, 0006001, "ROR R1", 0x807F, "1001"},
		{"RORB_1", 0177777, 0x0FF0, 0106001, "RORB R1", 0x0FF8, "1010"}, // only the low byte rotates (Python: 0x8778, "0011")
		{"RORB_2", 0177777, 0xF00F, 0106001, "RORB R1", 0xF087, "1001"}, // (Python: 0x7887)
		{"ROL", 0177777, 0xCC33, 0006101, "ROL R1", 0x9867, "1001"},
		{"ROLB", 0177777, 0xCC33, 0106101, "ROLB R1", 0xCC67, "0000"}, // (Python: 0x9966, "0011")
		{"ASR", 0177777, 0xCC33, 0006201, "ASR R1", 0xE619, "1001"},
		{"ASRB", 0177777, 0xCC33, 0106201, "ASRB R1", 0xCC19, "0011"}, // (Python: 0xE619)
	} {
		t.Run(c.name, func(t *testing.T) {
			f.psw.SetPSW(c.psw)
			f.reg.Set(1, c.r1)
			check(t, f.ss.IsSsOp(c.ins), "is_ss_op")
			r := f.ss.DoSsOp(c.ins)
			eq(t, r.Assembly, c.asm, "assembly")
			eq(t, f.reg.Get(1), c.r1out, "R1")
			eq(t, f.psw.NZVC(), c.nzvc, "nzvc")
		})
	}
}

// ---------------------------------------------------------- test_ssdd_ops

var ssddFx = newFixture()

const (
	maxPosInt = 0077777
	bigP      = 076400 // 32000
	smallP    = 037200 // 16000
	zero      = 0
)

var (
	smallN = TwosComplementNegative(smallP)
	bigN   = TwosComplementNegative(bigP)
)

// rrCase is a register-to-register double-operand case: set the PSW and
// two registers, run, and check the assembly, both registers and NZVC.
type rrCase struct {
	name    string
	psw     int
	rs, src int
	rd, dst int
	opcode  int
	asm     string
	result  int
	nzvc    string
}

func runRR(t *testing.T, f *fixture, cases []rrCase) {
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			f.psw.SetPSW(c.psw)
			f.reg.Set(c.rs, c.src)
			f.reg.Set(c.rd, c.dst)
			ins := ssddOp(c.opcode, 0, c.rs, 0, c.rd)
			check(t, f.ssdd.IsSsddOp(ins), "is_ssdd_op")
			r := f.ssdd.DoSsddOp(ins)
			eq(t, r.Assembly, c.asm, "assembly")
			eq(t, f.reg.Get(c.rs), c.src, "source")
			eq(t, f.reg.Get(c.rd), c.result, "result")
			eq(t, f.psw.NZVC(), c.nzvc, "nzvc")
		})
	}
}

func TestSsddRegister(t *testing.T) {
	neg := TwosComplementNegative
	runRR(t, ssddFx, []rrCase{
		{"BIC_1", 0, 1, 0xAAAA, 2, 0xFFFF, 0040000, "BIC R1,R2", 0x5555, "0000"},
		{"BICB_1", 0, 1, 0xAAAA, 2, 0xFFFF, 0140000, "BICB R1,R2", 0xFF55, "0000"},
		{"BIC_2", 0777777, 3, 0001234, 4, 0001111, 0040000, "BIC R3,R4", 0000101, "0001"},
		{"BICB_2", 0, 3, 0001234, 4, 0001111, 0140000, "BICB R3,R4", 0001101, "0000"},
		{"MOV_0", 0777777, 3, 0123456, 4, 0, 0010000, "MOV R3,R4", 0123456, "1001"},
		{"MOVB_01", 0777777, 3, 0xA72E, 4, 0, 0110000, "MOVB R3,R4", 0x002E, "0001"},
		{"MOVB_02", 0777777, 3, 0xA7AE, 4, 0, 0110000, "MOVB R3,R4", 0xFFAE, "1001"},      // sign-extends (Python: 0x00AE)
		{"MOVB_03", 0777777, 3, 0x00AE, 4, 0xA700, 0110000, "MOVB R3,R4", 0xFFAE, "1001"}, // sign-extends (Python: 0xA7AE)
		{"ADD_PP_P", 0, 2, 486, 4, 692, 0060000, "ADD R2,R4", 1178, "0000"},
		{"ADD_PP_Z", 0, 2, 0, 4, 0, 0060000, "ADD R2,R4", 0, "0100"},
		{"ADD_PP_NVC", 0, 2, maxPosInt - 64, 4, maxPosInt - 64, 0060000, "ADD R2,R4", 0177576, "1010"}, // (Python: "1011")
		{"ADD_NP_PC", 0, 2, neg(286), 4, 384, 0060000, "ADD R2,R4", 98, "0001"},
		{"ADD_NP_ZC", 0, 2, neg(286), 4, 286, 0060000, "ADD R2,R4", 0, "0101"},
		{"ADD_NP_N", 0, 2, neg(286), 4, 3, 0060000, "ADD R2,R4", 65253, "1000"},
		{"ADD_NN_N", 0, 2, neg(286), 4, neg(286), 0060000, "ADD R2,R4", neg(572), "1001"}, // (Python: "1000")
		{"ADD_NN_VC", 0, 2, neg(maxPosInt - 64), 4, neg(maxPosInt - 64), 0060000, "ADD R2,R4", 0202, "0011"},
	})
}

func TestMOVB04(t *testing.T) {
	f := ssddFx
	// 165250 112512 MOVB (R5)+,@R2 from M9301-YA
	f.psw.SetPSW(0777777)
	f.reg.Set(2, 0000500)
	f.ram.WriteWord(0000500, 0)
	eq(t, f.ram.ReadWord(0000500), 0, "@R2")
	f.reg.Set(5, 0165320)
	f.ram.WriteWord(0165320, 0177777)
	eq(t, f.ram.ReadWord(f.reg.Get(5)), 0177777, "@R5")
	ins := 0112512
	f.reg.SetPC(0165250)
	check(t, f.ssdd.IsSsddOp(ins), "is_ssdd_op")
	r := f.ssdd.DoSsddOp(ins)
	eq(t, r.Assembly, "MOVB (R5)+,@R2", "assembly")
	eq(t, f.reg.Get(5), 0165321, "R5")
	eq(t, f.ram.ReadByteAt(f.reg.Get(2)), 0377, "@R2")
}

func Test10_70(t *testing.T) {
	f := ssddFx
	// from a situation in M9301-YA
	f.psw.SetPSW(0)
	f.reg.Set(2, 0177771)
	f.reg.Set(4, 010)
	f.ssdd.DoSsddOp(ssddOp(0060000, 0, 2, 0, 4))
	eq(t, f.reg.Get(4), 1, "R4")
	eq(t, f.psw.NZVC(), "0001", "nzvc")
}

func TestVerifyNegatives(t *testing.T) {
	pn, pN := PythonifyPDP11Word(smallN), PythonifyPDP11Word(bigN)
	check(t, bigP > smallP, "P > p")
	check(t, smallP > zero, "p > z")
	check(t, zero > pn, "z > n")
	check(t, pn > pN, "n > N")
}

func TestSUBAssembly(t *testing.T) {
	f := ssddFx
	f.psw.SetPSW(0)
	f.reg.Set(6, bigN)
	f.reg.Set(4, smallN)
	ins := ssddOp(0160000, 0, 4, 0, 6)
	check(t, f.ssdd.IsSsddOp(ins), "is_ssdd_op")
	eq(t, f.ssdd.DoSsddOp(ins).Assembly, "SUB R4,R6", "assembly")
}

// SUB cases: R6 = dest, R4 = source
func TestSUB(t *testing.T) {
	f := ssddFx
	W := PDP11WordifyPythonInteger
	P, p, z, N, n := bigP, smallP, zero, bigN, smallN
	for _, c := range []struct {
		name           string
		dest, src, exp int
		nzvc           string
	}{
		{"zp_N", z, p, W(z - p), "1001"}, // borrow (Python: "1000")
		{"Pp_P", P, p, W(P - p), "0000"},
		{"PP_Z", P, P, 0, "0100"},
		{"pP_N", p, P, W(p - P), "1001"}, // borrow (Python: "1000")
		{"Nz_N", N, z, W(N), "1000"},
		{"Np_VC", N, p, W(N - p), "0010"}, // overflow, no borrow (Python: "0011")
		{"NP_V", N, P, W(N - P), "0010"},  // (Python: "0011")
		{"zP_N", z, P, W(-P), "1001"},     // (Python: "1000")
		{"Nn_N", N, n, W(N - n), "1001"},  // (Python: "1000")
		{"NN_Z", N, N, 0, "0100"},
		{"nN_P", n, N, W(n - N), "0000"},
		{"Zn_P", z, n, W(-n), "0001"},    // (Python: "0000")
		{"nP_V", n, P, W(n - P), "0010"}, // (Python: "0011")
		{"Pn_V", P, N, W(P - N), "1011"},
	} {
		t.Run(c.name, func(t *testing.T) {
			f.psw.SetPSW(0)
			f.reg.Set(6, c.dest)
			f.reg.Set(4, c.src)
			f.ssdd.DoSsddOp(ssddOp(0160000, 0, 4, 0, 6))
			eq(t, f.reg.Get(6), c.exp, "R6")
			eq(t, f.psw.NZVC(), c.nzvc, "nzvc")
		})
	}
}
