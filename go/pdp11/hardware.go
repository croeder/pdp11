// hardware.go - translation of pdp11_hardware: Registers, Ram, PSW, Stack,
// AddressModes.  Follows the fixed C version; FIX comments say what the
// Python did differently.
package pdp11

import (
	"fmt"
	"math"
	"strings"
	"sync"
)

func fixSign(word int) int {
	// fix a negative word so it will work with python math
	if word&MaskWordMSB != 0 {
		moreBits := math.MaxInt64 & ^MaskWord
		field := math.MaxInt64 - ((word & MaskWord) | moreBits) + 1
		return -field
	}
	return word
}

// addressOffset: address plus sign-fixed offset, wrapped to 16 bits.
// FIX: the Python lets this go negative or past 0o177777.
func addressOffset(address, offset int) int {
	return (address + fixSign(offset)) & MaskWord
}

// formattedOffset formats an index offset for disassembly.
func formattedOffset(offset int) string {
	formatted := offset
	if offset&MaskWordMSB == MaskWordMSB {
		formatted = -(077777 - (offset & 077777)) * 2
	}
	return PyOct(formatted)
}

// ================================================================ Lock

// Lock stands in for threading.Lock, including its locked() query.
type Lock struct {
	mu     sync.Mutex
	locked bool
}

func (l *Lock) Locked() bool { return l.locked }
func (l *Lock) Acquire()     { l.mu.Lock(); l.locked = true }
func (l *Lock) Release() {
	if !l.locked {
		raise("RuntimeError", "release unlocked lock")
	}
	l.locked = false
	l.mu.Unlock()
}

// =========================================================== Registers

type Registers struct {
	registers [8]int // R0 R1 R2 R3 R4 R5 R6 R7
	sp, pc    int    // R6 is the stack pointer, R7 the program counter
}

func NewRegisters() *Registers {
	logInfof("initializing pdp11_hardware registers")
	return &Registers{sp: 6, pc: 7}
}

func checkRegister(r int) int {
	// Python list indexing: -8..7 are valid
	if r < -8 || r > 7 {
		raise("IndexError", "list index out of range")
	}
	if r < 0 {
		return r + 8
	}
	return r
}

func (r *Registers) Get(reg int) int { return r.registers[checkRegister(reg)] }

// Set sets a register.  FIX: registers are 16 bits; the Python asserts
// value <= 0o177777 and so accepts negative values, and crashes on carries.
func (r *Registers) Set(reg, value int) { r.registers[checkRegister(reg)] = value & MaskWord }

func (r *Registers) GetPC() int { return r.registers[r.pc] }

// IncPC increments the program counter by 2.
func (r *Registers) IncPC() int {
	r.registers[r.pc] = (r.registers[r.pc] + 2) & MaskWord
	return r.registers[r.pc]
}

func (r *Registers) SetPC(newpc int) { r.registers[r.pc] = newpc & MaskWord }

// SetPC2xOffset adds twice a signed 8-bit offset to the PC.
func (r *Registers) SetPC2xOffset(offset int) {
	offset = 2 * offset
	if offset > MaskLowByte { // if we overflowed with the sign bit
		offset = fixSign(offset | MaskHighByte)
	}
	r.registers[r.pc] = (r.registers[r.pc] + offset) & MaskWord
}

func (r *Registers) GetSP() int      { return r.registers[r.sp] }
func (r *Registers) SetSP(newsp int) { r.registers[r.sp] = newsp & MaskWord }

func (r *Registers) String() string {
	var b strings.Builder
	for _, v := range r.registers {
		b.WriteString(" " + Oct6(v))
	}
	return b.String()
}

// ================================================================= Ram

type IOReader func() int
type IOWriter func(data int)

type ioEntry struct {
	read  IOReader
	write IOWriter
	rname string
	wname string
}

// Ram is byte-addressable memory including the I/O page.  Words are stored
// little-endian.  I/O handlers can only be registered in the I/O page, so
// they are kept in a slice indexed by (address - IOSpace).
type Ram struct {
	reg              *Registers
	TopOfMemory      int
	IOSpace          int
	Memory           []byte
	iomap            []ioEntry
	TopOfVectorSpace int
	lock             *Lock
	iSetLock         bool
}

func NewRam(lock *Lock, reg *Registers, bits int) *Ram {
	logInfof("initializing pdp11_hardware.Ram(%d)", bits)
	m := &Ram{reg: reg}
	m.TopOfMemory = (1 << bits) - 1
	// the io page is the top 4k words (8kB) of memory
	m.IOSpace = m.TopOfMemory - 017777
	logInfof("%d bits -> top_of_memory: %s = %d; io_space:%s io_space=%d",
		bits, PyOct(m.TopOfMemory), m.TopOfMemory, PyOct(m.IOSpace), m.IOSpace)
	m.Memory = make([]byte, m.TopOfMemory+1)
	m.iomap = make([]ioEntry, m.TopOfMemory-m.IOSpace+1)
	// the bottom area is io device handler vectors
	m.TopOfVectorSpace = 0274
	for a := 0; a < m.TopOfVectorSpace; a++ {
		m.WriteByteAt(a, 0277)
	}
	m.lock = lock
	logInfof("Ram init done. Lock object:%p", m.lock)
	return m
}

func (m *Ram) getLock(who string) {
	if !m.lock.Locked() {
		m.lock.Acquire()
		logInfof("%s got lock", who)
		m.iSetLock = true
	}
}

func (m *Ram) releaseLock(who string) {
	if m.iSetLock {
		m.lock.Release()
		logDebugf("%s released lock", who)
		m.iSetLock = false
	}
}

func (m *Ram) ioCheck(address int) {
	if address >= m.TopOfMemory || address < m.IOSpace {
		raise("AssertionError", fmt.Sprintf("io device address %s outside the I/O page", PyOct(address)))
	}
}

func (m *Ram) RegisterIOWriter(address int, method IOWriter, name string) {
	m.ioCheck(address)
	logInfof("register_io_writer(%s, %s)", PyOct(address), name)
	e := &m.iomap[address-m.IOSpace]
	e.write, e.wname = method, name
}

func (m *Ram) RegisterIOReader(address int, method IOReader, name string) {
	m.ioCheck(address)
	logInfof("register_io_reader(%s, %s)", PyOct(address), name)
	e := &m.iomap[address-m.IOSpace]
	e.read, e.rname = method, name
}

func (m *Ram) entry(address int) *ioEntry {
	if address < m.IOSpace || address > m.TopOfMemory {
		return nil
	}
	return &m.iomap[address-m.IOSpace]
}

func (m *Ram) checkAddress(address, limit int) {
	if address > limit || address < 0 {
		raise("AssertionError", fmt.Sprintf("address %s out of range", PyOct(address)))
	}
}

// WriteByteAt writes a byte; address can be even or odd.
func (m *Ram) WriteByteAt(address, data int) {
	m.checkAddress(address, m.TopOfMemory)
	data &= MaskLowByte
	if e := m.entry(address); e != nil && e.write != nil {
		if data != 0 {
			logDebugf("write_byte io self_lock:%v @%s, %s %s", m.lock.Locked(), PyOct(address), PyOct(data), SafeCharacter(data))
		}
		m.getLock("")
		e.write(data)
		m.releaseLock("")
		return
	}
	m.Memory[address] = byte(data)
}

// ReadByteAt reads a byte; address can be even or odd.
func (m *Ram) ReadByteAt(address int) int {
	m.checkAddress(address, m.TopOfMemory)
	if e := m.entry(address); e != nil && e.read != nil {
		m.getLock("")
		result := e.read()
		m.releaseLock("")
		if result != 0 {
			logDebugf("read_byte io self_lock:%v @%s returns %s %s", m.lock.Locked(), PyOct(address), PyOct(result), SafeCharacter(result))
		}
		return result
	}
	return int(m.Memory[address])
}

// WriteWord writes a word.  FIX: the PDP-11/03 (LSI-11) ignores bit 0 of a
// word address rather than trapping; the Python asserted the address was even.
func (m *Ram) WriteWord(address, data int) {
	address &^= 1
	m.checkAddress(address, m.TopOfMemory-1)
	if data > MaskWord {
		raise("AssertionError", "data <= MASK_WORD")
	}
	if e := m.entry(address); e != nil && e.write != nil {
		logDebugf("write_word io @%s, %s", PyOct(address), PyOct(data))
		m.getLock("")
		e.write(data)
		m.releaseLock("")
		return
	}
	m.Memory[address+1] = byte((data & MaskHighByte) >> 8)
	m.Memory[address] = byte(data & MaskLowByte)
}

// ReadWord reads a word (low byte at the even address).
func (m *Ram) ReadWord(address int) int {
	address &^= 1 // FIX: the 11/03 ignores bit 0 of a word address
	m.checkAddress(address, m.TopOfMemory-1)
	if e := m.entry(address); e != nil && e.read != nil {
		m.getLock("")
		result := e.read()
		m.releaseLock("")
		logDebugf("read word IO(%s) returns %s", Oct6(address), Oct6(result))
		return result
	}
	return int(m.Memory[address+1])<<8 + int(m.Memory[address])
}

// ReadWordFromPC reads the word at the PC and increments the PC.
func (m *Ram) ReadWordFromPC() int {
	result := m.ReadWord(m.reg.GetPC())
	m.reg.IncPC()
	return result
}

// Dump logs memory as octal words, octal bytes and ASCII.
func (m *Ram) Dump(start, stop int) {
	const displayBytes = 8
	start &= 0177770
	stop = (stop & 0177770) + 0010
	logInfof("dump(%s, %s)", PyOct(start), PyOct(stop))
	for row := start; row < stop; row += displayBytes {
		var b strings.Builder
		b.WriteString(Oct6(row) + "  ")
		for a := row; a < row+displayBytes; a += 2 {
			b.WriteString(Oct6(m.ReadWord(a)) + " ")
		}
		b.WriteString(" ")
		for a := row; a < row+displayBytes; a++ {
			b.WriteString(Oct3(m.ReadByteAt(a)) + " ")
		}
		b.WriteString(" ")
		for a := row; a < row+displayBytes; a++ {
			c := m.ReadByteAt(a)
			if c >= 16 && c < 127 {
				b.WriteByte(byte(c))
			} else {
				b.WriteByte('.')
			}
		}
		logInfof("%s", b.String())
	}
}

// ================================================================= PSW

// PSW bit meanings and masks
//
//	15 14 current mode        0o140000
//	13 12 previous mode       0o030000
//	7 6 5 priority            0o000340
//	4 T trap                  0o000020
//	3 N negative, 2 Z zero, 1 V overflow, 0 C carry
type PSW struct {
	ram        *Ram
	PSWAddress int
	PSW        int
}

const (
	cModeMask    = 0140000
	modeMask     = 0170000
	priorityMask = 0000340
	trapMask     = 0000020
	nMask        = 0000010
	zMask        = 0000004
	vMask        = 0000002
	cMask        = 0000001
)

func NewPSW(ram *Ram) *PSW {
	logInfof("initializing pdp11_hardware psw")
	p := &PSW{ram: ram, PSWAddress: ram.TopOfMemory - 1}
	// set up psw as an io device so we're not constantly writing to ram
	ram.RegisterIOReader(p.PSWAddress, func() int { return p.PSW }, "get_psw")
	logInfof("psw initilialized @%s", PyOct(p.PSWAddress))
	return p
}

func (p *PSW) Get() int { return p.PSW }

// The Python's set_psw(mode=-1, priority=-1, trap=-1, n=-1, z=-1, v=-1, c=-1,
// psw=-1) becomes one setter per field.
func (p *PSW) SetPSW(v int) { p.PSW = v }
func (p *PSW) SetMode(mode int) {
	old := p.PSW & modeMask
	p.PSW = (p.PSW &^ cModeMask) | mode<<14 | old>>2
}
func (p *PSW) SetPriority(v int) { p.PSW = (p.PSW &^ priorityMask) | v<<5 }
func (p *PSW) SetTrap(v int)     { p.PSW = (p.PSW &^ trapMask) | v<<4 }
func (p *PSW) SetN(v int)        { p.PSW = (p.PSW &^ nMask) | v<<3 }
func (p *PSW) SetZ(v int)        { p.PSW = (p.PSW &^ zMask) | v<<2 }
func (p *PSW) SetV(v int)        { p.PSW = (p.PSW &^ vMask) | v<<1 }
func (p *PSW) SetC(v int)        { p.PSW = (p.PSW &^ cMask) | v }

// SetNZVC sets condition codes; a -1 leaves that bit unchanged.
func (p *PSW) SetNZVC(n, z, v, c int) {
	if n > -1 {
		p.SetN(n)
	}
	if z > -1 {
		p.SetZ(z)
	}
	if v > -1 {
		p.SetV(v)
	}
	if c > -1 {
		p.SetC(c)
	}
}

// SetNZVCBits replaces the low four bits of the PSW.
func (p *PSW) SetNZVCBits(value int) { p.PSW = (p.PSW & 0177760) | (value & 017) }

// NFrom / ZFrom set N or Z from a value; b is 'B' for byte, anything else for word.
func (p *PSW) NFrom(b byte, value int) int {
	mask := MaskWordMSB
	if b == 'B' {
		mask = MaskByteMSB
	}
	n := bit(value&mask == mask)
	p.SetN(n)
	return n
}

func (p *PSW) ZFrom(b byte, value int) int {
	mask := MaskWord
	if b == 'B' {
		mask = MaskLowByte
	}
	z := bit(value&mask == 0)
	p.SetZ(z)
	return z
}

func (p *PSW) N() int { return (p.PSW & nMask) >> 3 }
func (p *PSW) Z() int { return (p.PSW & zMask) >> 2 }
func (p *PSW) V() int { return (p.PSW & vMask) >> 1 }
func (p *PSW) C() int { return p.PSW & cMask }

// NZVC returns the condition codes as a string such as "0100".
func (p *PSW) NZVC() string { return fmt.Sprintf("%d%d%d%d", p.N(), p.Z(), p.V(), p.C()) }

func bit(b bool) int {
	if b {
		return 1
	}
	return 0
}

// =============================================================== Stack

type Stack struct {
	reg *Registers
	ram *Ram
	psw *PSW
}

func NewStack(reg *Registers, ram *Ram, psw *PSW) *Stack {
	logInfof("initializing pdp11_hardware stack")
	return &Stack{reg, ram, psw}
}

func (s *Stack) Push(value int) {
	sp := (s.reg.GetSP() - 2) & MaskWord
	s.reg.SetSP(sp)
	s.ram.WriteWord(sp, value)
}

func (s *Stack) Pop() int {
	sp := s.reg.GetSP()
	result := s.ram.ReadWord(sp)
	s.reg.SetSP(sp + 2)
	return result
}

// Trap pushes PS and PC, then loads PC and PS from vector and vector+2.
func (s *Stack) Trap(vector int) {
	s.Push(s.psw.PSW & MaskWord)
	s.Push(s.reg.GetPC())
	s.reg.SetPC(s.ram.ReadWord(vector))
	s.psw.SetPSW(s.ram.ReadWord(vector + 2))
}

// ======================================================== AddressModes

// Operand is what addressing_mode_get returns: operand, register, address,
// operand_word, assembly, addressmode.
type Operand struct {
	Value       int
	Register    int
	Address     int
	OperandWord string
	Assembly    string
	Mode        int
}

type AddressModes struct {
	reg  *Registers
	ram  *Ram
	psw  *PSW
	Used [8]int
}

func NewAddressModes(reg *Registers, ram *Ram, psw *PSW) *AddressModes {
	logInfof("initializing pdp11_hardware addressModes")
	return &AddressModes{reg: reg, ram: ram, psw: psw}
}

// resolve evaluates an operand specifier.  With fetch false the effective
// address is computed but the operand is not read (for destinations that
// are only written, so a device register's read side effect is avoided).
func (am *AddressModes) resolve(b byte, modeRegister int, fetch bool) Operand {
	mode := (modeRegister & 070) >> 3
	am.Used[mode]++
	r := modeRegister & 07
	o := Operand{Register: r, Mode: mode}
	if mode != 0 && r == 7 {
		o.OperandWord = Oct6(am.ram.ReadWord(am.reg.Get(7)))
	}
	read := am.ram.ReadWord
	increment := 2
	if b == 'B' {
		read = am.ram.ReadByteAt
		increment = 1
	}
	if r == 6 || r == 7 {
		increment = 2
	}
	switch mode {
	case 0: // register contains operand
		o.Value = am.reg.Get(r)
		o.Assembly = fmt.Sprintf("R%d", r)
	case 1: // register deferred
		o.Address = am.reg.Get(r)
		o.Assembly = fmt.Sprintf("@R%d", r)
	case 2: // autoincrement
		o.Address = am.reg.Get(r)
		am.reg.Set(r, am.reg.Get(r)+increment)
		o.Assembly = fmt.Sprintf("(R%d)+", r)
	case 3: // autoincrement deferred
		o.Address = am.ram.ReadWord(am.reg.Get(r))
		am.reg.Set(r, am.reg.Get(r)+2)
		o.Assembly = fmt.Sprintf("@(R%d)+", r)
	case 4: // autodecrement
		am.reg.Set(r, am.reg.Get(r)-increment)
		o.Address = am.reg.Get(r)
		o.Assembly = fmt.Sprintf("-(R%d)", r)
	case 5: // autodecrement deferred
		am.reg.Set(r, am.reg.Get(r)-2)
		o.Address = am.ram.ReadWord(am.reg.Get(r))
		o.Assembly = fmt.Sprintf("@-(R%d)", r)
	case 6: // index: X, in the word after the instruction, plus the register
		x := am.ram.ReadWordFromPC()
		o.Address = addressOffset(am.reg.Get(r), x)
		o.OperandWord = Oct6(x)
		o.Assembly = fmt.Sprintf("%s(R%d)", formattedOffset(x), r)
	case 7: // index deferred
		x := am.ram.ReadWordFromPC()
		o.Address = am.ram.ReadWord(addressOffset(am.reg.Get(r), x))
		o.OperandWord = Oct6(x)
		o.Assembly = fmt.Sprintf("@%s(R%d)", formattedOffset(x), r)
	}
	if mode != 0 && fetch {
		o.Value = read(o.Address)
	}
	return o
}

// Get is addressing_mode_get: b is 'B' for byte, anything else for word.
func (am *AddressModes) Get(b byte, modeRegister int) Operand {
	return am.resolve(b, modeRegister, true)
}

// Address computes the effective address without reading the operand.
func (am *AddressModes) Address(b byte, modeRegister int) Operand {
	return am.resolve(b, modeRegister, false)
}

// Jump is addressing_mode_jmp: the jump target is the effective address
// (modes 1-7; mode 0 is illegal and stops the CPU).
// FIX: the Python reads one word too many in modes 4 and 5, and in mode 7
// reads a further word and adds the PC to it.
func (am *AddressModes) Jump(modeRegister int) (run bool, target int, operandWord, assembly string) {
	if modeRegister&070 == 0 {
		am.Used[0]++
		return false, 0, "", fmt.Sprintf("R%d", modeRegister&07) // *** this should call a trap
	}
	o := am.resolve(0, modeRegister, false)
	return true, o.Address, o.OperandWord, o.Assembly
}

// Set is addressing_mode_set: store result in the register or at the address.
func (am *AddressModes) Set(b byte, o Operand, result int) {
	switch {
	case o.Mode == 0:
		am.reg.Set(o.Register, result)
	case b == 'B':
		am.ram.WriteByteAt(o.Address, result)
	default:
		am.ram.WriteWord(o.Address, result&MaskWord)
	}
}

func (am *AddressModes) Report() {
	logInfof("address modes used:")
	for i, n := range am.Used {
		logInfof("%d:%d", i, n)
	}
}
