// boot.go - translation of pdp11_boot: load machine code, or a DEC listing
// with machine code, into memory.
package pdp11

import (
	"bufio"
	"os"
	"strconv"
	"strings"
)

type Boot struct {
	reg *Registers
	ram *Ram
}

func NewBoot(reg *Registers, ram *Ram) *Boot {
	logInfof("initializing pdp11Boot")
	return &Boot{reg, ram}
}

// BootstrapLoader is from the PDP-11/40 book.
var BootstrapLoader = []int{0016701, 0000240, 0012702, 0000240, 0005211, 0105711, 0100376,
	0116162, 0000240, 0000240, 0005267, 0177756, 0000765, 0177560, 0000000}

const BootAddress = 0000744

// Echo copies each character typed on the console back to it.
var Echo = []int{
	0012700, 0177560, // start: mov #kbs, r0
	0105710,                   // wait: tstb (r0)       ; character received?
	0100376,                   // bpl wait        ; no, loop
	0016060, 0000002, 0000006, // mov 2(r0),6(r0) ; transmit data
	0000772} // br wait         ; get next character

const EchoAddress = 0001000

// LoadMachineCode copies code into memory at base and sets the PC to base.
func (b *Boot) LoadMachineCode(code []int, base int) {
	address := base
	for _, w := range code {
		b.ram.WriteWord(address, w)
		address += 2
	}
	b.reg.SetPC(base)
}

// OctalToDecimal converts an octal masquerading as a decimal integer.
func OctalToDecimal(octal int) int {
	decimal, base := 0, 1
	for octal != 0 {
		decimal += (octal % 10) * base
		octal /= 10
		base *= 8
	}
	return decimal
}

func isNumeric(s string) bool {
	if s == "" {
		return false
	}
	for _, c := range s {
		if c < '0' || c > '9' {
			return false
		}
	}
	return true
}

// ReadPDP11AssemblyFile reads "address value" pairs from a DEC listing and
// returns the first address.
func (b *Boot) ReadPDP11AssemblyFile(file string) int {
	f, err := os.Open(file)
	if err != nil {
		raise("FileNotFoundError", file)
	}
	defer f.Close()
	base := 0
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		parts := strings.Fields(scanner.Text())
		if len(parts) == 0 || parts[0] == ";" || !isNumeric(parts[0]) {
			continue
		}
		n, _ := strconv.Atoi(parts[0])
		address := OctalToDecimal(n)
		if base == 0 {
			base = address
		}
		if len(parts) < 2 {
			raise("IndexError", "list index out of range")
		}
		if isNumeric(parts[1]) {
			v, _ := strconv.Atoi(parts[1])
			b.ram.WriteWord(address, OctalToDecimal(v))
		}
	}
	logInfof("read_pdp11_assembly_file \"%s\" returns base address:%s", file, PyOct(base))
	return base
}
