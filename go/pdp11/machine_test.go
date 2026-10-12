// machine_test.go - translation of test_config, test_DL11, test_hello_world
// and test_implemented: the tests that build the whole machine.
package pdp11

import (
	"bytes"
	"math/rand"
	"testing"
)

// ---------------------------------------------------------- test_config

func TestConfigDefault(t *testing.T) {
	eq[any](t, NewConfig("").Lookup("config", "name"), "default", "config.name")
}

func TestConfigRam(t *testing.T) {
	eq[any](t, NewConfig("").Lookup("ram", "bits"), float64(16), "ram.bits")
}

func TestConfigBarf(t *testing.T) {
	c := NewConfig("")
	eq[any](t, c.Lookup("config", "gook"), "ERROR", "config.gook")
	eq[any](t, c.Lookup("gobbledy", "gook"), "ERROR", "gobbledy.gook")
}

// ---------------------------------------------------------- test_DL11

var testCharacter int

func makeTestCharacter() int {
	testCharacter = 32 + rand.Intn(128-32+1) // random.randint(32,128)
	return testCharacter
}

// TestDL11Receive is whitebox testing of receive status and buffer.
func TestDL11Receive(t *testing.T) {
	p := New("")
	d, m := p.DL11, p.Ram
	eq(t, m.ReadWord(d.RCSRAddress), 0, "RCSR empty")
	m.WriteWord(d.RBUFAddress, makeTestCharacter()) // simulate receiving a byte
	eq(t, m.ReadWord(d.RCSRAddress)&RCSRRcvrDone, RCSRRcvrDone, "RCVR_DONE set")
	eq(t, m.ReadWord(d.RBUFAddress), testCharacter, "RBUF")
	eq(t, m.ReadWord(d.RCSRAddress)&RCSRRcvrDone, 0, "RCVR_DONE clear")
}

// TestDL11Transmit is whitebox testing of transmit status and buffer.
func TestDL11Transmit(t *testing.T) {
	p := New("")
	d, m := p.DL11, p.Ram
	eq(t, m.ReadWord(d.XCSRAddress)&XCSRXmitRdy, XCSRXmitRdy, "XMIT_RDY set")
	m.WriteWord(d.XBUFAddress, makeTestCharacter())
	eq(t, m.ReadWord(d.XCSRAddress)&XCSRXmitRdy, 0, "XMIT_RDY clear")
	eq(t, m.ReadWord(d.XBUFAddress), testCharacter, "XBUF")
	eq(t, m.ReadWord(d.XCSRAddress)&XCSRXmitRdy, XCSRXmitRdy, "XMIT_RDY set again")
}

// TestDL11Maintenance sets the maintenance bit, which connects the
// transmitter to the receiver, and checks that bytes go in and come out.
func TestDL11Maintenance(t *testing.T) {
	p := New("")
	d, m := p.DL11, p.Ram
	m.WriteWord(d.XCSRAddress, m.ReadWord(d.XCSRAddress)|XCSRMaint)
	eq(t, m.ReadWord(d.XCSRAddress)&XCSRMaint, XCSRMaint, "MAINT")
	eq(t, m.ReadWord(d.RCSRAddress), 0, "RCSR empty")
	for i := 0; i < 100; i++ {
		eq(t, m.ReadWord(d.XCSRAddress)&XCSRXmitRdy, XCSRXmitRdy, "XMIT_RDY set")
		m.WriteWord(d.XBUFAddress, makeTestCharacter())
		eq(t, m.ReadWord(d.XCSRAddress)&XCSRXmitRdy, 0, "XMIT_RDY clear")
		eq(t, m.ReadWord(d.RCSRAddress)&RCSRRcvrDone, RCSRRcvrDone, "RCVR_DONE set")
		eq(t, m.ReadWord(d.XBUFAddress), testCharacter, "XBUF")
		eq(t, m.ReadWord(d.XCSRAddress)&XCSRXmitRdy, XCSRXmitRdy, "XMIT_RDY set again")
	}
}

// ---------------------------------------------------------- test_hello_world
// The Python test loads the program and opens the VT52 window, with no
// assertion.  Here the program runs on the stdio terminal with its output
// captured, and the test checks that the message came out and the CPU halted.

func TestHelloWorld(t *testing.T) {
	p := New("True")
	boot := NewBoot(p.Reg, p.Ram)
	boot.LoadMachineCode([]int{
		0012702, // 2000 start:  MOV
		0177564, // 2002         serial+4,r2    ; r2 points to DL11
		0012701, // 2004         MOV
		0002032, // 2006         string,r1      ; r1 points to current char
		0112100, // 2010 nxtchr: MOVB (r1)+,r0  ; load xmt char
		0001405, // 2012         BEQ done       ; string is terminated with 0
		0110062, // 2014         MOVB r0,2(r2)  ; write char to transmit buffer
		0000002, // 2016
		0105712, // 2020 wait:   TSTB (r2)      ; character transmitted?
		0100376, // 2022         BPL wait       ; no, loop
		0000771, // 2024         BR nxtchr      ; transmit next character
		0000000, // 2026         halt
		0000763, // 2030         br start
	}, 02000)
	message := "Hello, World!\nHello, World!\nHello, World!"
	address := 02032
	for _, c := range []byte(message + "\x00") {
		p.Ram.WriteByteAt(address, int(c))
		address++
	}
	p.Reg.SetPC(02000)
	var out bytes.Buffer
	p.Terminal.Out = &out // no keyboard input
	p.Run(100000)
	eq(t, out.String(), message, "output")
	eq(t, p.Reg.GetPC(), 02030, "PC") // stopped just past the HALT at 2026
	check(t, p.CPUCycles < 100000, "halted, not stopped by the limit")
}

// ---------------------------------------------------------- test_implemented
// Generate all the opcodes and log which ones are not recognized.  Like the
// Python, this has no assertions; the results go to pdp11.log.

func TestImplemented(t *testing.T) {
	logInfof("testing opcodes")
	p := New("")
	var notOps []int
	for i := 0; i < 01777; i++ {
		opcode := i << 6
		name, kind := "", ""
		lookup := func(n string, ok bool, k string) {
			if ok {
				name, kind = n, k
			}
		}
		switch {
		case p.Br.IsBrOp(opcode):
			n, ok := BranchName(opcode)
			lookup(n, ok, "BR")
		case p.Cc.IsCcOp(opcode):
			name, kind = "CC", "CC"
		case p.Noopr.IsNooprOp(opcode):
			n, ok := NooprName(opcode)
			lookup(n, ok, "NOPR")
		case p.Rss.IsRssOp(opcode):
			n, ok := RssName(opcode & 0077000)
			lookup(n, ok, "RSS")
		case p.Ss.IsSsOp(opcode):
			n, ok := SingleOperandName(opcode & 0107700)
			lookup(n, ok, "SS")
		case p.Ssdd.IsSsddOp(opcode):
			n, ok := DoubleOperandName(opcode & 0070000)
			lookup(n, ok, "SSDD")
		default:
			// Python: `elif pdp11.other_ops.do_other_op(opcode & 0o777700):`
			// is a non-empty tuple, so always true; the call still executes.
			p.Other.DoOtherOp(opcode & 0777700)
			name, kind = "other", "other"
		}
		logInfof("%s,%s,%s,%s", PyOct(opcode), PyBin(opcode), kind, name)
	}
	// Every opcode lands in some branch above, so notOps stays empty, as
	// in the Python.
	logInfof("found these not-opcodes:")
	logInfof("%d not-opcodes", len(notOps))
}
