//go:build linux

package pdp11

import (
	"syscall"
	"unsafe"
)

func getTermios(fd int) (*syscall.Termios, error) {
	var t syscall.Termios
	if _, _, e := syscall.Syscall(syscall.SYS_IOCTL, uintptr(fd), syscall.TCGETS, uintptr(unsafe.Pointer(&t))); e != 0 {
		return nil, e
	}
	return &t, nil
}

func setTermios(fd int, t *syscall.Termios) error {
	if _, _, e := syscall.Syscall(syscall.SYS_IOCTL, uintptr(fd), syscall.TCSETS, uintptr(unsafe.Pointer(t))); e != 0 {
		return e
	}
	return nil
}

// cbreak: no line editing or echo; Return arrives as CR; Ctrl-C still signals.
func cbreak(t *syscall.Termios) {
	t.Lflag &^= syscall.ICANON | syscall.ECHO
	t.Iflag &^= syscall.ICRNL
	t.Cc[syscall.VMIN] = 1
	t.Cc[syscall.VTIME] = 0
}
