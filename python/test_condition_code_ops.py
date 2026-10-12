"""test condition code operators"""

# pdp11-40 ref p. 4-79
import logging
import threading

from pdp11_hardware import Registers as reg
from pdp11_hardware import Ram
from pdp11_hardware import PSW
from pdp11_cc_ops import cc_ops

from stopwatches import StopWatches as sw

class TestClass():
    reg = reg()
    ram = Ram(threading.Lock(), reg, 16)
    psw = PSW(ram)
    sw = sw()
    cc_ops = cc_ops(psw, sw)

    set_opcode = 0o000260
    clear_opcode = 0o000240
    psw_bits = 0o000017

    def CLX(self, cvzn):
        return self.clear_opcode | cvzn

    def SEX(self, cvzn):
        return self.set_opcode | cvzn

    def test_set_opcodes(self):
        test_code = 0o01
        while test_code <= 0o17:
            self.psw.set_psw(psw=0)
            instruction = self.SEX(test_code)
            assert self.cc_ops.is_cc_op(instruction)

            run, operand1, operand2, assembly, report = self.cc_ops.do_cc_op(instruction)

            result_code = self.psw.get_psw() & self.psw_bits
            logging.info(assembly)
            assert test_code == result_code

            test_code = test_code + 1


    def test_clr_opcodes(self):
        test_code = 0o00
        while test_code <= 0o17:
            self.psw.set_psw(psw=0o17)
            instruction = self.CLX(test_code)
            assert self.cc_ops.is_cc_op(instruction)

            expected = (self.psw.get_psw() & self.psw_bits) & ~test_code

            run, operand1, operand2, assembly, report = self.cc_ops.do_cc_op(instruction)

            result_code = self.psw.get_psw() & self.psw_bits
            logging.info(assembly)
            assert expected == result_code

            test_code = test_code + 1

