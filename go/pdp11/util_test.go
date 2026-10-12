// util_test.go - translation of test_utils.
package pdp11

import "testing"

func TestOct3(t *testing.T) { eq(t, Oct3(027), "027", "oct3") }
func TestOct6(t *testing.T) { eq(t, Oct6(02127), "002127", "oct6") }
func TestPad(t *testing.T)  { eq(t, Pad("arf", 6), "arf   ", "pad") }

func TestTwosComplementNegative(t *testing.T) {
	for _, c := range [][2]int{{0, 0}, {1, 65535}, {2, 65534}, {586, 64950}} {
		eq(t, TwosComplementNegative(c[0]), c[1], "twosComplementNegative")
	}
}

func TestPythonifyPDP11Word(t *testing.T) {
	for _, c := range [][2]int{{0, 0}, {1, 1}, {65535, -1}, {65534, -2}, {64950, -586}} {
		eq(t, PythonifyPDP11Word(c[0]), c[1], "pythonifyPDP11Word")
	}
}

func TestPythonifyPDP11Byte(t *testing.T) {
	for _, c := range [][2]int{{0, 0}, {1, 1}, {255, -1}, {254, -2}, {127, 0177}} {
		eq(t, PythonifyPDP11Byte(c[0]), c[1], "pythonifyPDP11Byte")
	}
}

func TestPDP11ifyPythonInt(t *testing.T) {
	for _, c := range [][2]int{{0, 0}, {1, 1}, {-1, 65535}, {-2, 65534}, {-586, 64950}} {
		eq(t, PDP11WordifyPythonInteger(c[0]), c[1], "PDP11WordifyPythonInteger")
	}
}

func TestPythonifyPDP11Long(t *testing.T) {
	eq(t, PythonifyPDP11Long(042400042), 9044002, "positive")
	eq(t, PythonifyPDP11Long(037735377736), -9044002, "negative")
}
