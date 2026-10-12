// dl11.go - translation of pdp11_dl11: the DEC DL11 serial interface.
package pdp11

import "fmt"

const (
	RCSRRcvrAct    = 0004000 // 11 RO - active (not implemented)
	RCSRRcvrDone   = 0000200 // 7 RO - set when a character is received, cleared when RBUF is read
	RCSRIntEnb     = 0000100 // 6 - enables interrupt (not implemented)
	XCSRXmitRdy    = 0000200 // 7 RO - set when the transmitter can accept another character
	XCSRXmitIntEnb = 0000100 // 6 RW - enables interrupt (not implemented)
	XCSRMaint      = 0000004 // 2 RW - maintenance loopback XBUF to RBUF
)

type DL11 struct {
	ram                                                *Ram
	lock                                               *Lock
	RCSRAddress, RBUFAddress, XCSRAddress, XBUFAddress int
	RCSR, RBUF, XCSR, XBUF                             int
	iSetLock                                           bool
}

func NewDL11(ram *Ram, base int) *DL11 {
	logInfof("initializing dl11(%s)", PyOct(base))
	d := &DL11{ram: ram, lock: ram.lock,
		RCSRAddress: base, RBUFAddress: base + 2, XCSRAddress: base + 4, XBUFAddress: base + 6}
	ram.RegisterIOWriter(d.RCSRAddress, d.WriteRCSR, "write_RCSR")
	ram.RegisterIOWriter(d.RBUFAddress, d.WriteRBUF, "write_RBUF")
	ram.RegisterIOWriter(d.XCSRAddress, d.WriteXCSR, "write_XCSR")
	ram.RegisterIOWriter(d.XBUFAddress, d.WriteXBUF, "write_XBUF")
	ram.RegisterIOReader(d.RCSRAddress, d.ReadRCSR, "read_RCSR")
	ram.RegisterIOReader(d.RBUFAddress, d.ReadRBUF, "read_RBUF")
	ram.RegisterIOReader(d.XCSRAddress, d.ReadXCSR, "read_XCSR")
	ram.RegisterIOReader(d.XBUFAddress, d.ReadXBUF, "read_XBUF")
	d.XCSR = XCSRXmitRdy // transmit status register ready on init
	logInfof("initializing dl11 done")
	return d
}

// All reads and writes to the buffers and CSRs are protected.  The CPU
// reaches them through Ram; the terminal calls them directly.
func (d *DL11) getLock(who string) {
	logInfof("get_lock called by %s", who)
	if !d.lock.Locked() {
		d.lock.Acquire()
		logInfof("dl11 got lock")
		d.iSetLock = true
	}
}

func (d *DL11) releaseLock(who string) {
	logInfof("release_lock called by %s", who)
	if d.iSetLock {
		d.lock.Release()
		logInfof("dl11 released lock")
		d.iSetLock = false
	}
}

func (d *DL11) WriteRCSR(v int) { d.getLock("write_RCSR"); d.RCSR = v; d.releaseLock("write_RCSR") }

func (d *DL11) ReadRCSR() int {
	d.getLock("read_RCSR")
	r := d.RCSR
	d.releaseLock("read_RCSR")
	return r
}

// WriteRBUF puts a received character in the buffer and sets the done bit.
func (d *DL11) WriteRBUF(v int) {
	logInfof("dl11.write_RBUF %s %s", PyOct(v), SafeCharacter(v))
	d.getLock(fmt.Sprintf("write_RBUF(%s)", PyOct(v)))
	d.RBUF = v
	d.RCSR |= RCSRRcvrDone
	d.releaseLock("write_RBUF")
	logInfof("dl11.write_RBUF exit")
}

// ReadRBUF returns the received character and clears the done bit.
func (d *DL11) ReadRBUF() int {
	d.getLock("read_RBUF")
	r := d.RBUF
	d.RCSR &^= RCSRRcvrDone
	d.releaseLock("read_RBUF")
	return r
}

func (d *DL11) WriteXCSR(v int) { d.getLock("write_XCSR"); d.XCSR = v; d.releaseLock("write_XCSR") }

func (d *DL11) ReadXCSR() int {
	d.getLock("read_XCSR")
	r := d.XCSR
	d.releaseLock("read_XCSR")
	return r
}

// WriteXBUF loads the transmit buffer, clearing ready; in maintenance mode
// the character loops back to the receiver.
func (d *DL11) WriteXBUF(v int) {
	d.getLock("write_XBUF({oct(byte)})") // sic: not an f-string in the Python
	d.XBUF = v
	d.XCSR &^= XCSRXmitRdy
	if d.XCSR&XCSRMaint == XCSRMaint {
		d.WriteRBUF(v)
	}
	d.releaseLock("write_XBUF")
}

// ReadXBUF takes the transmitted character and sets ready again.
func (d *DL11) ReadXBUF() int {
	d.getLock("read_XBUF")
	r := d.XBUF
	d.XCSR |= XCSRXmitRdy
	d.releaseLock("read_XBUF")
	return r
}
