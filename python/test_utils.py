import pdp11_util as u
import logging
from pdp11_logger import Logger

MASK_WORD = 0o177777
MASK_WORD_MSB = 0o100000
MASK_BYTE_MSB = 0o000200
MASK_LOW_BYTE = 0o000377
MASK_HIGH_BYTE = 0o177400

class TestClass():

    def test_oct3(self):
        assert u.oct3(0o27) == "027"

    def test_oct6(self):
        assert u.oct6(0o2127) == "002127"

    def test_pad(self):
        assert u.pad("arf", 6) == "arf   "

    def test_twosComplementNegative(self):
        assert u.twosComplementNegative(0) == 0
        assert u.twosComplementNegative(1) == 65535
        assert u.twosComplementNegative(2) == 65534
        assert u.twosComplementNegative(586) == 64950

#PDP11WordifyPythonInteger(source)
#extendSign(source):

    def test_pythonifyPDP11Word(self):
        assert u.pythonifyPDP11Word(0) == 0
        assert u.pythonifyPDP11Word(1) == 1
        assert u.pythonifyPDP11Word(65535) == -1
        assert u.pythonifyPDP11Word(65534) == -2
        assert u.pythonifyPDP11Word(64950) == -586

    def test_pythonifyPDP11Byte(self):
        print('\ntest_pythonifyPDP11Byte')
        assert u.pythonifyPDP11Byte(0) == 0
        assert u.pythonifyPDP11Byte(1) == 1
        assert u.pythonifyPDP11Byte(255) == -1
        assert u.pythonifyPDP11Byte(254) == -2
        assert u.pythonifyPDP11Byte(127) == 0o177

    def test_PDP11ifyPythonInt(self):
        assert u.PDP11WordifyPythonInteger(0) == 0
        assert u.PDP11WordifyPythonInteger(1) == 1
        assert u.PDP11WordifyPythonInteger(-1) == 65535
        assert u.PDP11WordifyPythonInteger(-2) == 65534
        assert u.PDP11WordifyPythonInteger(-586) == 64950

    def test_pythonifyPDP11Long(self):
        assert u.pythonifyPDP11Long(0o42400042) == 9044002
        assert u.pythonifyPDP11Long(0o37735377736) == -9044002
