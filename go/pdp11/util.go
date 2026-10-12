// util.go - translation of pdp11_util.c / pdp11_util.py, plus logging
// (pdp11_logger) and the panic helper used where the Python would raise.
package pdp11

import (
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"time"
)

const (
	MaskLong     = 037777777777 // 32 bits
	MaskLongMSB  = 020000000000
	MaskWord     = 0177777 // 16 bits
	MaskWordMSB  = 0100000
	MaskByte     = 0000377 // 8 bits
	MaskByteMSB  = 0000200
	MaskLowByte  = 0000377
	MaskHighByte = 0177400
)

// raise stops the emulator where the Python would raise an exception.
func raise(kind, msg string) {
	panic(fmt.Sprintf("%s: %s", kind, msg))
}

// PyOct formats like Python's oct(): 0o27, -0o5, 0o0.
func PyOct(v int) string {
	if v < 0 {
		return fmt.Sprintf("-0o%o", -v)
	}
	return fmt.Sprintf("0o%o", v)
}

// PyBin formats like Python's bin().
func PyBin(v int) string {
	if v < 0 {
		return fmt.Sprintf("-0b%b", -v)
	}
	return fmt.Sprintf("0b%b", v)
}

// pySlicePrefix returns s[0:n] with Python's handling of negative and out-of-range n.
func pySlicePrefix(s string, n int) string {
	if n < 0 {
		n += len(s)
	}
	if n < 0 {
		n = 0
	}
	if n > len(s) {
		n = len(s)
	}
	return s[:n]
}

// Oct3 formats an octal 3 digits wide: 0o27 -> 027.
func Oct3(word int) string {
	octal := PyOct(word)[2:]
	return pySlicePrefix("0000", 3-len(octal)) + octal
}

// Oct6 formats an octal 6 digits wide: 0o2127 -> 002127.
func Oct6(word int) string {
	octal := PyOct(word)
	return pySlicePrefix("000000", 8-len(octal)) + octal[2:]
}

// Pad right-pads a string to width characters (at most 20 spaces, as in the Python).
func Pad(s string, width int) string {
	return s + pySlicePrefix("                    ", width-len(s))
}

func TwosComplementNegative(source int) int     { return (^source + 1) & MaskWord }
func TwosComplementNegativeLong(source int) int { return (^source + 1) & MaskLong }

func PythonifyPDP11Byte(source int) int {
	result := source & MaskByte
	if MaskByteMSB&source == MaskByteMSB {
		result = -^((source | ^MaskByte) - 1)
	}
	return result
}

func PythonifyPDP11Word(source int) int {
	result := source
	if MaskWordMSB&source == MaskWordMSB {
		result = -^((source | ^MaskWord) - 1)
	}
	return result
}

func PythonifyPDP11Long(source int) int {
	result := source
	if MaskLongMSB&source == MaskLongMSB {
		result = -^((source | ^MaskLong) - 1)
	}
	return result
}

func PDP11WordifyPythonInteger(source int) int {
	if source >= 0 {
		return source & MaskWord
	}
	return TwosComplementNegative(-source)
}

func PDP11LongifyPythonInteger(source int) int {
	if source >= 0 {
		return source & MaskLong
	}
	return TwosComplementNegativeLong(-source)
}

var lowASCII = [32]string{
	"NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
	"BS", "HT", "LF", "VF", "FF", "CR", "SO", "SI",
	"DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
	"CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US"}

// SafeCharacter returns a control character's name, or the character itself.
func SafeCharacter(b int) string {
	switch {
	case b > 31 && b < 127:
		return string(rune(b))
	case b > 31:
		return fmt.Sprintf("\\x%02x", b)
	case b >= 0:
		return lowASCII[b]
	case b >= -32:
		return lowASCII[32+b]
	}
	raise("IndexError", "list index out of range")
	return ""
}

// ---------------------------------------------------------------- logging
// translation of pdp11_logger: ./pdp11.log, INFO level, reopened (truncated)
// by every Logger() call, as logging.basicConfig(force=True, filemode='w') does.

const (
	logDebug = 10
	logInfo  = 20
	logError = 40
)

var (
	logFile  *os.File
	logLevel = logInfo
	logPath  = "./pdp11.log"
)

// Logger (re)opens ./pdp11.log.
func Logger() {
	if logFile != nil {
		logFile.Close()
		logFile = nil
	}
	f, err := os.Create(logPath)
	if err != nil {
		return // logging stays off, as in the C when the file cannot be opened
	}
	logFile = f
	logLevel = logInfo
	logMessage(logInfo, "%s begins", logPath)
}

func logEnabled(level int) bool { return logFile != nil && level >= logLevel }

func logMessage(level int, format string, args ...any) {
	if !logEnabled(level) {
		return
	}
	now := time.Now().UTC()
	file, function := "?", "?"
	if pc, f, _, ok := runtime.Caller(2); ok {
		file = filepath.Base(f)
		if fn := runtime.FuncForPC(pc); fn != nil {
			name := fn.Name()
			function = name[strings.LastIndex(name, ".")+1:]
		}
	}
	name := map[int]string{logDebug: "DEBUG", logInfo: "INFO", logError: "ERROR"}[level]
	fmt.Fprintf(logFile, "%s.%03d000Z %s:%s %s : %s\n", now.Format("2006-01-02T15:04:05"),
		now.Nanosecond()/1000000, file, function, name, fmt.Sprintf(format, args...))
}

func logInfof(format string, args ...any) {
	if logEnabled(logInfo) {
		logMessage(logInfo, format, args...)
	}
}

func logDebugf(format string, args ...any) {
	if logEnabled(logDebug) {
		logMessage(logDebug, format, args...)
	}
}

func logErrorf(format string, args ...any) {
	if logEnabled(logError) {
		logMessage(logError, format, args...)
	}
}
