// terminal.go - translation of pdp11_terminal as a stdin/stdout terminal:
// characters the PDP-11 transmits go to Out, keys read from In are
// delivered to the DL11 receiver.
package pdp11

import (
	"bufio"
	"io"
	"os"
	"os/signal"
	"syscall"
)

type Terminal struct {
	dl11   *DL11
	buffer int // for holding LF after CR was sent
	sw     *StopWatches
	Out    io.Writer
	keys   chan byte
}

func NewTerminal(dl11 *DL11, sw *StopWatches) *Terminal {
	logInfof("initializing terminal")
	return &Terminal{dl11: dl11, sw: sw, Out: os.Stdout}
}

// KeysFrom starts delivering keys from r (stdin, normally).  Without it the
// terminal has no keyboard.
func (t *Terminal) KeysFrom(r io.Reader) {
	t.keys = make(chan byte, 256)
	go func() {
		br := bufio.NewReader(r)
		for {
			b, err := br.ReadByte()
			if err != nil {
				close(t.keys)
				return
			}
			t.keys <- b
		}
	}()
}

// RawInput puts stdin into character-at-a-time, no-echo mode, restoring it
// when the process exits through Ctrl-C or SIGTERM.  It returns a function
// that restores the terminal.
func (t *Terminal) RawInput() (restore func()) {
	fd := int(os.Stdin.Fd())
	old, err := getTermios(fd)
	if err != nil {
		return func() {} // not a terminal: nothing to change
	}
	raw := *old
	cbreak(&raw)
	if setTermios(fd, &raw) != nil {
		return func() {}
	}
	restore = func() { setTermios(fd, old) }
	signals := make(chan os.Signal, 1)
	signal.Notify(signals, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		<-signals
		restore()
		os.Exit(130)
	}()
	return restore
}

// Cycle delivers output and polls for input once.
func (t *Terminal) Cycle() {
	// an attempt to make the terminal send LF after CR
	if t.buffer != 0 && t.dl11.RCSR&RCSRRcvrDone == 0 {
		t.dl11.WriteRBUF(t.buffer)
		t.buffer = 0
	}
	// if there's a character in the dl11 transmit buffer, then eat it
	if t.dl11.XCSR&XCSRXmitRdy == 0 {
		c := t.dl11.ReadXBUF()
		if c != 0 { // DL11 can send nulls; they are not shown
			logInfof("dl11 read_XBUF:%s", SafeCharacter(c))
			t.Out.Write([]byte{byte(c)})
		}
	}
	// take a key, without waiting, if the receiver has room for it
	if t.keys != nil && t.dl11.RCSR&RCSRRcvrDone == 0 {
		select {
		case k, ok := <-t.keys:
			if ok {
				t.dl11.WriteRBUF(int(k))
			} else {
				t.keys = nil // end of input
			}
		default:
		}
	}
}
