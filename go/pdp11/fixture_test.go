// fixture_test.go - shared test setup, the counterpart of the C tests'
// fixture.h and of the pytest class attributes.
//
// Go runs a file's tests in the order they are declared, and each test file
// keeps its own fixture in a package variable, so state carries from one test
// to the next as it does with pytest.  (Running a single test with -run can
// therefore behave differently, as with pytest -k.)
package pdp11

import (
	"os"
	"testing"
)

// TestMain runs the tests from the repository root, where
// config_default.json and source/ are.
func TestMain(m *testing.M) {
	if err := os.Chdir("../.."); err != nil {
		panic(err)
	}
	code := m.Run()
	os.Remove("pdp11.log")
	os.Exit(code)
}

type fixture struct {
	lock  *Lock
	reg   *Registers
	ram   *Ram
	psw   *PSW
	stack *Stack
	am    *AddressModes
	sw    *StopWatches
	br    *BrOps
	cc    *CcOps
	noopr *NooprOps
	other *OtherOps
	rss   *RssOps
	ss    *SsOps
	ssdd  *SsddOps
}

func newFixture() *fixture {
	f := &fixture{lock: &Lock{}, sw: NewStopWatches()}
	f.reg = NewRegisters()
	f.ram = NewRam(f.lock, f.reg, 16)
	f.psw = NewPSW(f.ram)
	f.stack = NewStack(f.reg, f.ram, f.psw)
	f.am = NewAddressModes(f.reg, f.ram, f.psw)
	f.br = NewBrOps(f.reg, f.psw, f.sw)
	f.cc = NewCcOps(f.psw, f.sw)
	f.noopr = NewNooprOps(f.reg, f.psw, f.stack, f.sw)
	f.other = NewOtherOps(f.reg, f.ram, f.psw, f.am, f.sw)
	f.rss = NewRssOps(f.reg, f.psw, f.am, f.sw)
	f.ss = NewSsOps(f.reg, f.psw, f.am, f.sw)
	f.ssdd = NewSsddOps(f.reg, f.psw, f.am, f.sw)
	return f
}

// eq stops the test at the first mismatch, as a failing pytest assert does.
func eq[T comparable](t *testing.T, got, want T, what string) {
	t.Helper()
	if got != want {
		t.Fatalf("%s: got %v, want %v", what, got, want)
	}
}

func check(t *testing.T, cond bool, what string) {
	t.Helper()
	if !cond {
		t.Fatalf("assert %s", what)
	}
}

// instruction builders used by several test files
func ss6(mode, register int) int { return (mode<<3 | register) << 6 }
func dd(mode, register int) int  { return mode<<3 | register }
func ssddOp(opcode, modeS, regS, modeD, regD int) int {
	return opcode | ss6(modeS, regS) | dd(modeD, regD)
}
