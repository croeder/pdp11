import logging
import threading

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

    def test_byte_mask_w(self):
        logging.info('\ntest_byte_mask_w')
        BW = 'W'
        value = 0b1010101010101010
        target = 0b0101010101010101
        return_value = self.ssdd_ops.byte_mask(BW, value, target)
        assert return_value == 0b1010101010101010

    def test_byte_mask_b1(self):
        logging.info('\ntest_byte_mask_b1')
        BW = 'B'
        value = 0b1111111100000000
        target = 0b0000000011111111
        return_value = self.ssdd_ops.byte_mask(BW, value, target)
        assert return_value == 0

    def test_byte_mask_b2(self):
        logging.info('\ntest_byte_mask_b2')
        BW = 'B'
        value = 0b0000000011111111
        target = 0b1111111100000000
        return_value = self.ssdd_ops.byte_mask(BW, value, target)
        assert return_value == 0b1111111111111111

    def test_byte_mask_b3(self):
        logging.info('\ntest_byte_mask_b3')
        BW = 'B'
        value = 0b1010101010101010
        target = 0b0101010101010101
        return_value = self.ssdd_ops.byte_mask(BW, value, target)
        assert return_value == 0b0101010110101010

    def test_byte_mask_b4(self):
        logging.info('\ntest_byte_mask_b4')
        BW = 'B'
        value = 0b0000000000000000
        target = 0b0101010101010101
        return_value = self.ssdd_ops.byte_mask(BW, value, target)
        assert return_value == 0b0101010100000000

    def test_CLR(self):
        logging.info('\ntest_CLR')
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0o013333)

        instruction = 0o05001  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "CLR R1"

        r1 = self.reg.get(1)
        assert r1 == 0o0

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0100"

    def test_COM_1(self):
        logging.info('\ntest_COM_1  complement')
        # 0b0001011011011011
        # 0b1110100100100100
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0o013333)

        instruction = 0o005101 # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "COM R1"

        r1 = self.reg.get(1)
        logging.info(f'R1:{oct(r1)}')
        assert r1 == 0o164444

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"

    def test_COM_2(self):
        logging.info('\ntest_COM_2  complement')
        # 0b1110100100100100
        # 0b0001011011011011
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0o164444)

        instruction = 0o005101 # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "COM R1"

        r1 = self.reg.get(1)
        assert r1 == 0o013333

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0001"

    def test_COMB_1(self):
        logging.info('\ntest_COMB_1  complement')
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0o000022)

        instruction = 0o105101 # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "COMB R1"

        r1 = self.reg.get(1)
        assert r1 == 0o355

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001"

    def test_INC(self):
        logging.info('\ntest_INC')
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0o013333)

        instruction = 0o05201  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "INC R1"

        r1 = self.reg.get(1)
        assert r1 == 0o013334

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_INCB(self):
        logging.info('\ntest_INCB')
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0o000033)

        instruction = 0o105201  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "INCB R1"

        r1 = self.reg.get(1)
        assert r1 == 0o000034

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_DEC_1(self):
        logging.info('\ntest_DEC_1')
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0o013333)

        instruction = 0o05301  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "DEC R1"

        r1 = self.reg.get(1)
        assert r1 == 0o013332

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_DEC_2(self):
        logging.info('\ntest_DEC_2')
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0o0001)

        instruction = 0o05301  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "DEC R1"

        r1 = self.reg.get(1)
        assert r1 == 0

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0100"

    def test_DEC_3(self):
        logging.info('\ntest_DEC_3')
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0)

        instruction = 0o05301  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "DEC R1"

        r1 = self.reg.get(1)
        assert r1 == 0o177777

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1000" # decrement doesn't set overflow

    def test_DEC_B1(self):
        logging.info('\ntest_DEC_B1')
        self.psw.set_psw(psw=0)
        self.reg.set(1, 0o000001)

        instruction = 0o105301  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "DECB R1"

        r1 = self.reg.get(1)
        assert r1 == 0o000000

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0100" #0010

    def test_SWAB_1(self):
        logging.info('\ntest_SWAB_1')
        # from pdp11 book 0111111111111111 -> 1111111101111111
        self.psw.set_psw(psw=0o0177777)
        self.reg.set(1, 0o077777)

        instruction = 0o000301  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "SWAB R1"

        r1 = self.reg.get(1)
        assert r1 == 0o177577

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"

    def test_SWAB_2(self):
        logging.info('\ntest_SWAB_2')
        # reverse of pdp11 book 1111111101111111 -> 0111111111111111
        self.psw.set_psw(psw=0o0177777)
        self.reg.set(1, 0o177577)

        instruction = 0o000301  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "SWAB R1"

        r1 = self.reg.get(1)
        assert r1 == 0o077777

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1000"

    def test_SWAB_3(self):
        logging.info('\ntest_SWAB_3')
        # BFI reverse bits 0000000011111111 -> 1111111100000000
        self.psw.set_psw(psw=0o0177777)
        # 0 000 000 011 111 111
        self.reg.set(1, 0o000377)

        instruction = 0o000301  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "SWAB R1"

        r1 = self.reg.get(1)
        # 1 111 111 100 000 000
        assert r1 == 0o177400

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0100" # NZVC

    def test_SWAB_4(self):
        logging.info('\ntest_SWAB_4')
        # BFI reverse bits back 1111111100000000 -> 0000000011111111
        self.psw.set_psw(psw=0o0177777)
        # 0 000 000 011 111 111
        self.reg.set(1, 0o177400)

        instruction = 0o000301  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "SWAB R1"

        r1 = self.reg.get(1)
        # 1 111 111 100 000 000
        assert r1 == 0o000377

        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1000" # NZVC

    def test_ROR_1(self):
        logging.info('\ntest_ROR_1')
        self.psw.set_psw(psw=0o0177777)
        self.reg.set(1, 0b1111111100000000)
        instruction = 0o006001  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "ROR R1"
        r1 = self.reg.get(1)
        assert r1 == 0b1111111110000000  # FIX: C=1 rotates into bit 15 (Python: 0x7F80, "0000")
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1010"  # NZVC

    def test_ROR_2(self):
        logging.info('\ntest_ROR_2')
        self.psw.set_psw(psw=0o0177777)
        self.reg.set(1, 0b0000000011111111)
        instruction = 0o006001  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "ROR R1"
        r1 = self.reg.get(1)
        assert r1 == 0b1000000001111111
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001" # NZVC

    def test_RORB_1(self):
        logging.info('\ntest_RORB_1')
        self.psw.set_psw(psw=0o0177777)
        self.reg.set(1, 0b0000111111110000)
        instruction = 0o106001  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "RORB R1"
        logging.info (report)
        r1 = self.reg.get(1)
        assert r1 == 0b0000111111111000  # FIX: only the low byte rotates, C into bit 7 (Python: 0x8778, "0011")
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1010"  # NZVC

    def test_RORB_2(self):
        logging.info('\ntest_RORB_2')
        self.psw.set_psw(psw=0o0177777)
        self.reg.set(1, 0b1111000000001111)
        instruction = 0o106001  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "RORB R1"
        logging.info (report)
        r1 = self.reg.get(1)
        assert r1 == 0b1111000010000111  # FIX: only the low byte rotates (Python: 0x7887)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001" # NZVC

    def test_ROL(self):
        logging.info('\ntest_ROL')
        self.psw.set_psw(psw=0o0177777)
        self.reg.set(1, 0b1100110000110011)
        instruction = 0o006101  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "ROL R1"
        r1 = self.reg.get(1)
        logging.info(f'r1:{bin(r1)}  {report}')
        assert r1 == 0b1001100001100111
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001" # NZVC

    def test_ROLB(self):
        logging.info('\ntest_ROLB')
        self.psw.set_psw(psw=0o0177777)
        self.reg.set(1, 0b1100110000110011)
        instruction = 0o106101  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "ROLB R1"
        r1 = self.reg.get(1)
        logging.info(f'r1:{bin(r1)}  {report}')
        assert r1 == 0b1100110001100111  # FIX: only the low byte rotates, C into bit 0 (Python: 0x9966, "0011")
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0000"  # NZVC

    def test_ASR(self):
        logging.info('\ntest_ASR')
        self.psw.set_psw(psw=0o0177777)
        self.reg.set(1, 0b1100110000110011)
        instruction = 0o006201  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "ASR R1"
        r1 = self.reg.get(1)
        logging.debug(f'r1:{bin(r1)}  {report}')
        assert r1 == 0b1110011000011001
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "1001" # NZVC

    def test_ASRB(self):
        logging.info('\ntest_ASRB')
        self.psw.set_psw(psw=0o0177777)
        self.reg.set(1, 0b1100110000110011)
        instruction = 0o106201  # mode 0 R1
        assert self.ss_ops.is_ss_op(instruction)
        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        assert assembly == "ASRB R1"
        r1 = self.reg.get(1)
        logging.info(f'r1:{bin(r1)}  {report}')
        assert r1 == 0b1100110000011001  # FIX: only the low byte shifts (Python: 0xE619)
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0011" # NZVC  I think it shoud be 1010
