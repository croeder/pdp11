import logging
import threading

import pdp11_util as u
from pdp11_logger import Logger
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
MAX_POS_INT = 0o077777 # 32767
MAX_NEG_INT = 0o177777 # - 32768

P = 0o76400 # 32000
p = 0o37200 # 16000
z = 0 # 0o0
n = u.twosComplementNegative(p)
N = u.twosComplementNegative(P)

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

    def SS(self, mode, register):
        return (mode << 3 | register) << 6

    def DD(self, mode, register):
        return (mode << 3 | register)

    def op(self, opcode, modeS=0, regS=0, modeD=0, regD=1):
        return opcode | self.SS(modeS, regS) | self.DD(modeD, regD)

    def test_assert(self):
        assert "actual" != "expected"

    def test_byte_mask_w(self):
        Logger()
        logging.info('test_byte_mask_w')
        BW = 'W'
        value = 0b1010101010101010
        target = 0b0101010101010101
        return_value = self.ssdd_ops.byte_mask(BW, value, target)
        assert return_value == 0b1010101010101010

    def test_byte_mask_b1(self):
        logging.info('test_byte_mask_b1')
        BW = 'B'
        value = 0b1111111100000000
        target = 0b0000000011111111
        return_value = self.ssdd_ops.byte_mask(BW, value, target)
        assert return_value == 0

    def test_byte_mask_b2(self):
        logging.info('test_byte_mask_b2')
        BW = 'B'
        value = 0b0000000011111111
        target = 0b1111111100000000
        return_value = self.ssdd_ops.byte_mask(BW, value, target)
        assert return_value == 0b1111111111111111

    def test_byte_mask_b3(self):
        logging.info('test_byte_mask_b3')
        BW = 'B'
        value = 0b1010101010101010
        target = 0b0101010101010101
        return_value = self.ssdd_ops.byte_mask(BW, value, target)
        assert return_value == 0b0101010110101010

    def test_byte_mask_b4(self):
        logging.info('test_byte_mask_b4')
        BW = 'B'
        value = 0b0000000000000000
        target = 0b0101010101010101
        return_value = self.ssdd_ops.byte_mask(BW, value, target)
        assert return_value == 0b0101010100000000

    def test_BIC_1(self):
        logging.info('test_BIC_1')
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0b1010101010101010)
        self.reg.set(2, 0b1111111111111111)

        instruction = self.op(opcode=0o040000, modeS=0, regS=1, modeD=0, regD=2)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'BIC R1,R2'

        r2 = self.reg.get(2)
        assert r2 == 0b0101010101010101

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_BICB_1(self):
        logging.info('test_BICB_1')
        self.psw.set_psw(psw=0)
        # evil test shows that The high byte is unaffected.
        self.reg.set(1, 0b1010101010101010)
        self.reg.set(2, 0b1111111111111111)

        instruction = self.op(opcode=0o140000, modeS=0, regS=1, modeD=0, regD=2)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'BICB R1,R2'

        r2 = self.reg.get(2)
        assert r2 == 0b1111111101010101

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_BIC_2(self):
        logging.info('test_BIC_2')
        # PDP-11/40 p. 4-21
        self.psw.set_psw(psw=0o777777)
        # evil test puts word data into a byte test and expects byte result
        self.reg.set(3, 0o001234)
        self.reg.set(4, 0o001111)

        instruction = self.op(opcode=0o040000, modeS=0, regS=3, modeD=0, regD=4)
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "BIC R3,R4"

        assert self.reg.get(3) == 0o001234
        assert self.reg.get(4) == 0o000101

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0001"

    def test_BICB_2(self):
        logging.info('test_BICB_2')
        # PDP-11/40 p. 4-21
        self.psw.set_psw(psw=0)
        # evil test puts word data into a byte test and expects byte result
        self.reg.set(3, 0o001234)
        self.reg.set(4, 0o001111)

        instruction = self.op(opcode=0o140000, modeS=0, regS=3, modeD=0, regD=4)
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "BICB R3,R4"

        assert self.reg.get(3) == 0o001234
        assert self.reg.get(4) == 0o001101

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_MOV_0(self):
        logging.info('test_MOV_0')
        self.psw.set_psw(psw=0o777777)
        self.reg.set(3,0o123456)
        self.reg.set(4,0o000000)
        instruction = self.op(opcode=0o010000, modeS=0, regS=3, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "MOV R3,R4"

        r3 = self.reg.get(3)
        assert r3 == 0o123456

        r4 = self.reg.get(4)
        assert self.reg.get(4) == 0o123456

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"

    def test_MOVB_01(self):
        logging.info('test_MOVB_01')
        self.psw.set_psw(psw=0o777777)
        self.reg.set(3,0b1010011100101110)
        self.reg.set(4,0b0000000000000000)
        instruction = self.op(opcode=0o110000, modeS=0, regS=3, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "MOVB R3,R4"

        r3 = self.reg.get(3)
        assert r3 == 0b1010011100101110

        r4 = self.reg.get(4)
        assert self.reg.get(4) == 0b0000000000101110

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0001"

    def test_MOVB_02(self):
        logging.info('test_MOVB_02')
        self.psw.set_psw(psw=0o777777)
        self.reg.set(3,0b1010011110101110)
        self.reg.set(4,0b0000000000000000)
        instruction = self.op(opcode=0o110000, modeS=0, regS=3, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "MOVB R3,R4"

        r3 = self.reg.get(3)
        assert r3 == 0b1010011110101110

        r4 = self.reg.get(4)
        assert self.reg.get(4) == 0b1111111110101110  # FIX: MOVB to a register sign-extends (Python: 0x00AE)

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"

    def test_MOVB_03(self):
        logging.info('test_MOVB_03')
        self.psw.set_psw(psw=0o777777)
        self.reg.set(3,0b0000000010101110)
        self.reg.set(4,0b1010011100000000)
        instruction = self.op(opcode=0o110000, modeS=0, regS=3, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == "MOVB R3,R4"

        r3 = self.reg.get(3)
        assert r3 == 0b0000000010101110

        r4 = self.reg.get(4)
        assert self.reg.get(4) == 0b1111111110101110  # FIX: sign-extends (Python: 0xA7AE)

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"

    def test_MOVB_04(self):
        logging.info('test_MOVB_04')
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
        logging.info(f'@R2={oct(atr2)} {bin(atr2)}')
        assert atr2 == 0o377

    def test_ADD_PP_P(self):
        logging.info('test_ADD_PP_P')
        # positive and positive, positive result
        self.psw.set_psw(psw=0)
        self.reg.set(2, 486)
        self.reg.set(4, 692)
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')
        instruction = self.op(opcode=0o060000, modeS=0, regS=2, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'ADD R2,R4'

        r4 = self.reg.get(4)
        assert self.reg.get(4) == 1178

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_ADD_PP_Z(self):
        logging.info('test_ADD_PP_Z')
        # positive and positive, positive result
        self.psw.set_psw(psw=0)
        self.reg.set(2, 0)
        self.reg.set(4, 0)
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')
        instruction = self.op(opcode=0o060000, modeS=0, regS=2, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'ADD R2,R4'

        r4 = self.reg.get(4)
        assert self.reg.get(4) == 0

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0100"

    def test_ADD_PP_NVC(self):
        logging.info('test_ADD_PP_NVC')
        # positive and positive, negative result
        self.psw.set_psw(psw=0)
        bigpositive = MAX_POS_INT - 64
        self.reg.set(2, bigpositive)
        self.reg.set(4, bigpositive)
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')
        instruction = self.op(opcode=0o060000, modeS=0, regS=2, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'ADD R2,R4'

        r4 = self.reg.get(4)
        assert self.reg.get(4) == 0o177576

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1010"  # FIX: no carry out of bit 15 (Python: "1011")

    def test_ADD_NP_PC(self):
        logging.info('test_ADD_NP_PC')
        # negative and positive, positive result
        self.psw.set_psw(psw=0)
        self.reg.set(2, u.twosComplementNegative(286))
        self.reg.set(4, 384)
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')
        instruction = self.op(opcode=0o060000, modeS=0, regS=2, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'ADD R2,R4'

        r4 = self.reg.get(4)
        assert self.reg.get(4) == 98

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0001"

    def test_ADD_NP_ZC(self):
        logging.info('test_ADD_NP_ZC')
        # negative and positive, zero result
        self.psw.set_psw(psw=0)
        self.reg.set(2, u.twosComplementNegative(286))
        self.reg.set(4, 286)
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')
        instruction = self.op(opcode=0o060000, modeS=0, regS=2, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'ADD R2,R4'

        r4 = self.reg.get(4)
        assert self.reg.get(4) == 0

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0101"

    def test_ADD_NP_N(self):
        logging.info('test_ADD_NP_N')
        # negative and positive, negative result
        self.psw.set_psw(psw=0)
        self.reg.set(2, u.twosComplementNegative(286))
        self.reg.set(4, 3)
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')
        instruction = self.op(opcode=0o060000, modeS=0, regS=2, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'ADD R2,R4'

        r4 = self.reg.get(4)
        assert self.reg.get(4) == 65253 # -283

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1000"

    def test_ADD_NN_N(self):
        logging.info('test_ADD_NN_N')
        # negative and negative, negative result
        self.psw.set_psw(psw=0)
        self.reg.set(2, u.twosComplementNegative(286))
        self.reg.set(4, u.twosComplementNegative(286))
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')
        instruction = self.op(opcode=0o060000, modeS=0, regS=2, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'ADD R2,R4'
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')

        r4 = self.reg.get(4)
        assert self.reg.get(4) == u.twosComplementNegative(572)

        condition_codes = self.psw.get_nzvc()
        logging.info(f'condition_codes:{condition_codes}')
        assert condition_codes == "1001"  # FIX: carry out (Python: "1000")

    def test_ADD_NN_VC(self):
        logging.info('test_ADD_NN_V')
        # negative and negative, negative result
        # Rule: Internal representation is limited to 2 bytes.
        self.psw.set_psw(psw=0)
        bignegative = u.twosComplementNegative(MAX_POS_INT - 64)
        logging.info(f'bignegative:{oct(bignegative)}')
        self.reg.set(2, bignegative)
        self.reg.set(4, bignegative)
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')
        instruction = self.op(opcode=0o060000, modeS=0, regS=2, modeD=0, regD=4)   # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert assembly == 'ADD R2,R4'
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')
        r4 = self.reg.get(4)
        assert self.reg.get(4) == 0o202
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0011"

    def test_10_70(self):
        logging.info('test_10_70')
        # from a sutuation in M9301-YA
        self.psw.set_psw(psw=0)
        self.reg.set(2, 0o177771)
        self.reg.set(4, 0o10)
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')
        instruction = self.op(opcode=0o060000, modeS=0, regS=2, modeD=0, regD=4)   # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        logging.info(f'R2:{oct(self.reg.get(2))} R4:{oct(self.reg.get(4))}')
        r4 = self.reg.get(4)
        assert self.reg.get(4) == 0o01
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0001"

    def test_verify_negatives(self):
        logging.info('test_verify_negatives')
        logging.info(f'P:{P} {oct(P)}')
        logging.info(f'p:{p} {oct(p)}')
        logging.info(f'z:{z} {oct(z)}')
        python_n = u.pythonifyPDP11Word(n)
        python_N = u.pythonifyPDP11Word(N)
        logging.info(f'n:{python_n} {oct(n)}')
        logging.info(f'N:{python_N} {oct(N)}')
        assert n == u.twosComplementNegative(p)
        assert N == u.twosComplementNegative(P)
        assert P > p
        assert p > z
        assert z > python_n
        assert python_n > python_N

    def test_SUB_Assembly(self):
        self.psw.set_psw(psw=0)
        self.reg.set(6, N) # dest
        self.reg.set(4, n) # source
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        assert self.ssdd_ops.is_ssdd_op(instruction)
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        logging.info('assembly:'+assembly)
        assert assembly == 'SUB R4,R6'

    def test_SUB_zp_N(self): #1 zero minus small positive
        logging.info('test_SUB_zp_N')
        self.psw.set_psw(psw=0)
        self.reg.set(6, z) # dest
        self.reg.set(4, p) # source
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(z - p)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"  # FIX: borrow (Python: "1000")

    def test_SUB_Pp_P(self): #2 big positive minus small positive
        logging.info('test_SUB_Pp_P')
        self.psw.set_psw(psw=0)
        self.reg.set(6, P)
        self.reg.set(4, p)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(P - p)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_SUB_PP_Z(self): #
        logging.info('test_SUB_PP_Z')
        self.psw.set_psw(psw=0)
        self.reg.set(6, P)
        self.reg.set(4, P)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == 0
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0100"

    def test_SUB_pP_N(self): #4
        logging.info('test_SUB_pP_N')
        self.psw.set_psw(psw=0)
        self.reg.set(6, p)
        self.reg.set(4, P)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(p - P)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"  # FIX: borrow (Python: "1000")

    def test_SUB_Nz_N(self): #5
        logging.info('test_SUB_Nz_N')
        self.psw.set_psw(psw=0)
        self.reg.set(6, N)
        self.reg.set(4, z)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(N)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1000"

    def test_SUB_Np_VC(self): #6
        logging.info('test_SUB_Np_VC')
        self.psw.set_psw(psw=0)
        self.reg.set(6, N)
        self.reg.set(4, p)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(N - p)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0010"  # FIX: overflow, no borrow (Python: "0011")

    def test_SUB_NP_V(self): #7
        logging.info('test_SUB_NP_V')
        self.psw.set_psw(psw=0)
        self.reg.set(6, N)
        self.reg.set(4, P)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(N - P)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0010"  # FIX: overflow, no borrow (Python: "0011")

    def not_test_SUB_Np_N(self): #8
        logging.info('not_test_SUB_Np_N')
        self.psw.set_psw(psw=0)
        self.reg.set(6, N)
        self.reg.set(4, p)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(N - p)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1000"

    def test_SUB_zP_N(self): #9
        logging.info('test_SUB_zP_N')
        self.psw.set_psw(psw=0)
        self.reg.set(6, z)
        self.reg.set(4, P)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(-P)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"  # FIX: borrow (Python: "1000")

    def test_SUB_Nn_N(self): #10
        logging.info('test_SUB_Nn_N')
        self.psw.set_psw(psw=0)
        self.reg.set(6, N)
        self.reg.set(4, n)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(N - n)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"  # FIX: borrow (Python: "1000")

    def test_SUB_NN_Z(self): #11
        logging.info('test_SUB_NN_Z')
        self.psw.set_psw(psw=0)
        self.reg.set(6, N)
        self.reg.set(4, N)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == 0
        condition_codes = self.psw.get_nzvc()
        logging.info(f'condition_codes:{condition_codes}')
        assert condition_codes == "0100"

    def test_SUB_nN_P(self): #12
        logging.info('test_SUB_nN_P')
        self.psw.set_psw(psw=0)
        self.reg.set(6, n)
        self.reg.set(4, N)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(n - N)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"  

    def test_SUB_Zn_P(self): #13
        logging.info('test_SUB_Nn_P')
        self.psw.set_psw(psw=0)
        self.reg.set(6, z)
        self.reg.set(4, n)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(-n)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0001"  # FIX: borrow (Python: "0000")
        
    def test_SUB_nP_V(self): #14
        logging.info('test_SUB_nP_V')
        self.psw.set_psw(psw=0)
        self.reg.set(6, n)
        self.reg.set(4, P)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(n - P)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0010"  # FIX: overflow, no borrow (Python: "0011")

    def test_SUB_NP_V(self): #15
        logging.info('test_SUB_NP_V')
        self.psw.set_psw(psw=0)
        self.reg.set(6, N)
        self.reg.set(4, P)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(N - P)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0010"  # FIX: overflow, no borrow (Python: "0011")

    def test_SUB_Pn_V(self): #16
        logging.info('test_SUB_Pn_V')
        self.psw.set_psw(psw=0)
        self.reg.set(6, P)
        self.reg.set(4, N)
        logging.info(f'R4:{oct(self.reg.get(4))} R6:{oct(self.reg.get(6))}')
        instruction = self.op(opcode=0o160000, modeS=0, regS=4, modeD=0, regD=6)  # mode 0 R1
        run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)
        assert self.reg.get(6) == u.PDP11WordifyPythonInteger(P - N)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1011"

