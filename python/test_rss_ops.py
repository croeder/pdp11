import logging
import threading
import pytest # because pytest.mark.parametrize

import pdp11_util as u
from pdp11_hardware import Registers as reg
from pdp11_hardware import Ram
from pdp11_hardware import PSW
from pdp11_hardware import Stack
from pdp11_hardware import AddressModes as am

from pdp11_br_ops import br_ops
from pdp11_cc_ops import cc_ops
from pdp11_noopr_ops import noopr_ops
from pdp11_other_ops import other_ops
from pdp11_rss_ops import rss_ops
from pdp11_ss_ops import ss_ops
from pdp11_ssdd_ops import ssdd_ops

from stopwatches import StopWatches as sw

MASK_WORD = 0o177777
MASK_WORD_MSB = 0o100000
MASK_BYTE_MSB = 0o000200
MASK_LOW_BYTE = 0o000377
MASK_HIGH_BYTE = 0o177400

P = 0o764 # 500
p = 0o265 # 181
p1 = 1
n1 = u.twosComplementNegative(p1)
z = 0 # 0o0
n = u.twosComplementNegative(p)
N = u.twosComplementNegative(P)
# p * p = 32,761 which fits in a 16-bit word (32,768)
# p * P = 90,500 which fits in a 32-bit long word
# P * P = 250,000 which fits in a 32-bit long word
# *** need some longword overflow cases

# 'name, s, d, RS, RD, nzvc
multiplication_testcases = [
    ("pp", p, p, 2, 4, '0000'),
    ("pz", p, z, 2, 4, '0100'),
    ("pn", p, n, 2, 4, '1000'),
    ("zp", z, p, 2, 4, '0100'),
    ("zz", z, z, 2, 4, '0100'),
    ("zn", z, n, 2, 4, '0100'),
    ("p1p1", p1, p1, 2, 4, '0000'),
    ("p1n1", p1, n1, 2, 4, '1000'),
    ("n1p1", n1, p1, 2, 4, '1000'),
    ("n1n1", n1, n1, 2, 4, '0000'),
    ("np", n, p, 2, 4, '1000'),
    ("nz", n, z, 2, 4, '0100'),
    ("nn", n, n, 2, 4, '0000'),

    # FIX: C is set when the product does not fit in 16 bits, which is every
    # nonzero case with a P or N factor (the Python expected C=0).
    ("pP", p, P, 2, 4, '0001'),
    ("pN", p, N, 2, 4, '1001'),
    ("zP", z, P, 2, 4, '0100'),
    ("zN", z, N, 2, 4, '0100'),
    ("nP", n, P, 2, 4, '1001'),
    ("nN", n, N, 2, 4, '0001'),
    ("Pp", P, p, 2, 4, '0001'),
    ("Pz", P, z, 2, 4, '0100'),
    ("Pn", P, n, 2, 4, '1001'),
    ("Np", N, p, 2, 4, '1001'),
    ("Nz", N, z, 2, 4, '0100'),
    ("Nn", N, n, 2, 4, '0001'),

    ("PP", P, P, 2, 4, '0001'),
    ("PN", P, N, 2, 4, '1001'),
    ("NP", N, P, 2, 4, '1001'),
    ("NN", N, N, 2, 4, '0001'),
]

# There are limits on what sorts of numbers can be divided without blowing things up
smlPos = 0o44276
bigPos = 0o0042400042
smlNeg = u.twosComplementNegative(smlPos)
bigNeg = u.twosComplementNegativeLong(bigPos)

# name, numerator, denonminator, RS, RD, nzvc
# numerators are 32-bit pdp11 integers
division_testcases = [
    ("Pp", bigPos, smlPos, 2, 4, '0000'),
    ("Pn", bigPos, smlNeg, 2, 4, '1000'),
    ("Np", bigNeg, smlPos, 2, 4, '1000'),
    ("Nn", bigNeg, smlNeg, 2, 4, '0000'),
]

class TestClass():
    reg = reg()
    ram = Ram(threading.Lock(), reg, 16)
    psw = PSW(ram)
    stack = Stack(reg, ram, psw)
    am = am(reg, ram, psw)
    sw = sw()

    br_ops = br_ops(reg, ram, psw, sw)
    cc_ops = cc_ops(psw, sw)
    noopr_ops = noopr_ops(reg, ram, psw, stack, sw)
    other_ops = other_ops(reg, ram, psw, am, sw)
    rss_ops = rss_ops(reg, ram, psw, am, sw)
    ss_ops = ss_ops(reg, ram, psw, am, sw)
    ssdd_ops = ssdd_ops(reg, ram, psw, am, sw)

    def SS_part(self, mode, register):
        return (mode << 3 | register) << 6

    def DD_part(self, mode, register):
        return (mode << 3 | register)

    def make_ssdd_op(self, opcode, modeS=0, regS=0, modeD=0, regD=1):
        return opcode | self.SS_part(modeS, regS) | self.DD_part(modeD, regD)

    def make_rss_op(self, opcode, regD=0, modeS=0, regS=0):
        # documentation calls second parameter SS which here uses DD_part to make
        return opcode | regD << 6 | self.DD_part(modeS, regS)

    def test_BIC(self):
        #print('\ntest_BIC')
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0b1010101010101010)
        self.reg.set(2, 0b1111111111111111)

        instruction = self.make_ssdd_op(opcode=0o040000, modeS=0, regS=1, modeD=0, regD=2)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'BIC R1,R2'

        r2 = self.reg.get(2)
        assert r2 == 0b0101010101010101

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_BICB(self):
        #print('\ntest_BICB')
        self.psw.set_psw(psw=0)
        # evil test shows that The high byte is unaffected.
        self.reg.set(1, 0b1010101010101010)
        self.reg.set(2, 0b1111111111111111)


        instruction = self.make_ssdd_op(opcode=0o140000, modeS=0, regS=1, modeD=0, regD=2)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'BICB R1,R2'

        r2 = self.reg.get(2)
        assert r2 == 0b1111111101010101

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_BIC_2(self):
        #print('\ntest_BIC_2')
        # PDP-11/40 p. 4-21
        self.psw.set_psw(psw=0o777777)
        # evil test puts word data into a byte test and expects byte result
        self.reg.set(3, 0o001234)
        self.reg.set(4, 0o001111)

        instruction = self.make_ssdd_op(opcode=0o040000, modeS=0, regS=3, modeD=0, regD=4)
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "BIC R3,R4"

        assert self.reg.get(3) == 0o001234
        assert self.reg.get(4) == 0o000101

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0001"

    def test_BICB_2(self):
        #print('\ntest_BICB_2')
        # PDP-11/40 p. 4-21
        self.psw.set_psw(psw=0)
        # evil test puts word data into a byte test and expects byte result
        self.reg.set(3, 0o001234)
        self.reg.set(4, 0o001111)

        instruction = self.make_ssdd_op(opcode=0o140000, modeS=0, regS=3, modeD=0, regD=4)
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "BICB R3,R4"

        assert self.reg.get(3) == 0o001234
        assert self.reg.get(4) == 0o001101

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_MOV_0(self):
        #print('\ntest_MOV_0')
        self.psw.set_psw(psw=0o777777)
        self.reg.set(3,0o123456)
        self.reg.set(4,0o000000)
        instruction = self.make_ssdd_op(opcode=0o010000, modeS=0, regS=3, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "MOV R3,R4"

        r3 = self.reg.get(3)
        assert r3 == 0o123456

        r4 = self.reg.get(4)
        assert r4 == 0o123456

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"

    def test_MOV_2(self):
        #print('\ntest_MOV_2')
        # 010322 MOV R3,(R2)+
        # from M9001_YA that broke
        self.psw.set_psw(psw=0o777777)
        self.reg.set(3,0o177777)
        self.reg.set(2,0o000000)
        instruction = self.make_ssdd_op(opcode=0o010000, modeS=0, regS=3, modeD=2, regD=2)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        #print('setup done; running make_ssdd_op')
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "MOV R3,(R2)+"
        #print('make_ssdd_op done; checking results')

        r3 = self.reg.get(3)
        assert r3 == 0o177777

        r2 = self.reg.get(2)
        assert r2 == 0o02

        target = self.ram.read_word(0o000000)
        assert target == r3

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"

    def test_MOVB_01(self):
        #print('\ntest_MOVB_01')
        self.psw.set_psw(psw=0o777777)
        self.reg.set(3,0b1010011100101110)
        self.reg.set(4,0b0000000000000000)
        instruction = self.make_ssdd_op(opcode=0o110000, modeS=0, regS=3, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "MOVB R3,R4"

        r3 = self.reg.get(3)
        assert r3 == 0b1010011100101110

        r4 = self.reg.get(4)
        assert r4 == 0b0000000000101110

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0001"

    def test_MOVB_02(self):
        #print('\ntest_MOVB_02')
        self.psw.set_psw(psw=0o777777)
        self.reg.set(3,0b1010011110101110)
        self.reg.set(4,0b0000000000000000)
        instruction = self.make_ssdd_op(opcode=0o110000, modeS=0, regS=3, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "MOVB R3,R4"

        r3 = self.reg.get(3)
        assert r3 == 0b1010011110101110

        r4 = self.reg.get(4)
        assert r4 == 0b1111111110101110  # FIX: MOVB to a register sign-extends (Python: 0x00AE)

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"

    def test_MOVB_03(self):
        #print('\ntest_MOVB_03')
        self.psw.set_psw(psw=0o777777)
        self.reg.set(3,0b0000000010101110)
        self.reg.set(4,0b1010011100000000)
        instruction = self.make_ssdd_op(opcode=0o110000, modeS=0, regS=3, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "MOVB R3,R4"

        r3 = self.reg.get(3)
        assert r3 == 0b0000000010101110

        r4 = self.reg.get(4)
        assert r4 == 0b1111111110101110  # FIX: sign-extends (Python: 0xA7AE)

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"

    def test_MOVB_04(self):
        #print('\ntest_MOVB_04')
        # 165250 112512 MOVB (R5)+,@R2 from M9301-YA
        # MOVB (R5)+,@R2
        self.psw.set_psw(psw=0o777777)
        self.reg.set(2,0o000500)
        self.ram.write_word(0o000500, 0)
        assert self.ram.read_word(0o000500) == 0

        self.reg.set(5,0o165320)
        self.ram.write_word(0o165320, 0o177777)
        assert self.ram.read_word(self.reg.get(5)) == 0o177777

        instruction = 0o112512
        self.reg.set_pc(0o165250)
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "MOVB (R5)+,@R2"
        assert self.reg.get(5) == 0o165321
        atr2  = self.ram.read_byte(self.reg.get(2))
        #print(f'@R2={oct(atr2)} {bin(atr2)}')
        assert atr2 == 0o377

    @pytest.mark.parametrize('name, s, d, RS, RD, CC', multiplication_testcases)
    def test_MUL(self, name, s, d, RS, RD, CC):
        #print(f'\ntest_MUL assembly: MUL R{RS},R{RD} ; {oct(d)} * {oct(s)} = {u.pythonifyPDP11Word(d)} * {u.pythonifyPDP11Word(s)}')
        instruction = 0o070000
        self.psw.set_psw(psw=0)
        self.reg.set(RD, d)
        self.reg.set(RS, s)
        instruction = self.make_rss_op(instruction, regD=RD, modeS=0, regS=RS)
        #print(f'test_MUL instruction: {oct(instruction)}')  # 070RSS
        assert self.rss_ops.is_rss_op(instruction)
        self.rss_ops.do_rss_op(instruction)

        expected_product = u.PDP11LongifyPythonInteger(u.pythonifyPDP11Word(s) * u.pythonifyPDP11Word(d))
        #print(f'test_MUL expected_product:{expected_product}')

        high_word = self.reg.get(RD)
        low_word = self.reg.get(RD+1)
        #print(f'test_MUL high_word:{oct(high_word)} low_word:{oct(low_word)}')
        pdp11_actual_product = (high_word << 16) | low_word
        #print(f'test_MUL pdp11_actual_product:{pdp11_actual_product} = {oct(pdp11_actual_product)}')
        assert pdp11_actual_product == expected_product

        #print(f'test_MUL expected nzvc: {CC}');
        condition_codes = self.psw.get_nzvc()
        #print(f'test_MUL   actual nzvc: {condition_codes}');
        assert condition_codes == CC
        #print(f'test_MUL result:{u.pythonifyPDP11Long(pdp11_actual_product)} = {oct(pdp11_actual_product)}  CC:{condition_codes}')

    def not_test_constants(self):
        print(f'\ntest_constants')
        print(f'P: {P} = {oct(P)}')
        print(f'p: {p} = {oct(p)}')
        print(f'z: {z} = {oct(z)}')
        print(f'n: {u.pythonifyPDP11Word(n)} = {oct(n)}')
        print(f'N: {u.pythonifyPDP11Word(N)} = {oct(N)}')

        print(f'bigPos: {u.pythonifyPDP11Long(bigPos)} = {oct(bigPos)}')
        print(f'smlPos: {smlPos} = {oct(smlPos)}')
        print(f'smlNeg: {smlNeg} = {oct(smlNeg)}')
        print(f'bigNeg: {u.pythonifyPDP11Long(bigNeg)} = {oct(bigNeg)}')

    @pytest.mark.parametrize('name, numerator, denominator, RS, RD, CC', division_testcases)
    def test_DIV(self, name, numerator, denominator, RS, RD, CC):
        #DIV: (R, R+1) < (R, R+1) / (src)
        # The 32-bit two's complement integer in R and Rvl is divided by the source operand.
        # The quotient is left in R; the remainder in Rvl.
        # Division will be performed so that the remainder is of the same sign as the dividend.
        # R must be even.
        #
        # We reuse parameters and results from MUL
        py_numerator = u.pythonifyPDP11Long(numerator)
        py_denominator = u.pythonifyPDP11Word(denominator)
        #print(f'\ntest_DIV assembly: DIV R{RS},R{RD} ; {oct(numerator)} / {oct(denominator)} = {py_numerator} / {py_denominator}')
        # FIX: the PDP-11 truncates toward zero and gives the remainder the
        # sign of the dividend.  The Python used floor division plus 1
        # whenever an operand was negative, which is wrong when both are
        # negative: it expected 486 for case Nn, where the quotient is 485.
        assert py_denominator != 0
        py_expected_quotient = abs(py_numerator) // abs(py_denominator)
        if (py_numerator < 0) != (py_denominator < 0):
            py_expected_quotient = -py_expected_quotient
        py_expected_remainder = py_numerator - py_denominator * py_expected_quotient
        #print(f'test_DIV py_expected_quotient:{oct(py_expected_quotient)} = {py_expected_quotient}')
        #print(f'test_DIV py_expected_remainder:{oct(py_expected_remainder)} = {py_expected_remainder}')

        # set up the environment
        Rvl = RD + 1
        self.reg.set(RD, numerator >> 16)            # high-order word
        self.reg.set(Rvl, numerator & MASK_WORD)    # low-order word
        self.reg.set(RS, denominator)

        # set up the instruction
        instruction = 0o071000
        self.psw.set_psw(psw=0)
        instruction = self.make_rss_op(instruction, regD=RD, modeS=0, regS=RS)
        #print(f'test_DIV instruction: {oct(instruction)}')  # 070RSS
        assert self.rss_ops.is_rss_op(instruction)

        # run the instruction
        self.rss_ops.do_rss_op(instruction)

        # get the results
        py_actual_quotient = u.pythonifyPDP11Word(self.reg.get(RD))
        py_actual_remainder = u.pythonifyPDP11Word(self.reg.get(Rvl))
        #print(f'test_DIV py_actual_quotient:{oct(py_actual_quotient)} = {py_actual_quotient}')
        #print(f'test_DIV py_actual_remainder:{oct(py_actual_remainder)} = {py_actual_remainder}')

        assert py_actual_quotient == py_expected_quotient
        assert py_actual_remainder == py_expected_remainder

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == CC

