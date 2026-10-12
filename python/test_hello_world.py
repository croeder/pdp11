# test_hello_world.py
# FIX: the Python opened the VT52 window, with no assertion.  Here the
# program runs on the stdio terminal with its output captured, and the test
# checks that the message came out and the CPU halted.
import io
# test the pdp11_hardware.py module using pytest
# pip3 install --upgrade pip
# pip install -U pytest

import logging

from pdp11 import PDP11
from pdp11 import pdp11Run
from pdp11_boot import pdp11Boot

MASK_WORD = 0o177777
MASK_WORD_MSB = 0o100000
MASK_BYTE_MSB = 0o000200
MASK_LOW_BYTE = 0o000377
MASK_HIGH_BYTE = 0o177400

               #0o62510, 0o66154, 0o26157, 0o73440, 0o71157, 0o62154, 0o6441, 0o012]
# Following are bytes, not words.
# .ascii /Hello, world!/
# 0o110,    0o145,
# 0o154,    0o154,
# 0o157,    0o054,
# 0o040,    0o167,
# 0o157,    0o162,
# 0o154,    0o144,
# 0o041,    0o015,
# 0o012,    0o000]    # lf, end



class TestClass():

    def test_hello_world(self):
        pdp11 = PDP11(True)
        out = io.BytesIO()
        pdp11.terminal.out = out
        logging.info('test_hello_world begins')

        logging.info('test_hello_world pdp11Boot()')
        boot = pdp11Boot(pdp11.reg, pdp11.ram)

        logging.info('test_hello_world load_machine_code()')
        # http://www.retrocmp.com/how-tos/interfacing-to-a-pdp-1105/146-interfacing-with-a-pdp-1105-test-programs-and-qhello-worldq
        hello_world = [0o012702,  # 2000 start:  MOV 0o27 0o2
                       0o177564,  # 2002         serial+4,r2    ; r2 points to DL11
                       0o012701,  # 2004         MOV
                       0o002032,  # 2006         string,r1      ; r1 points to current char
                       0o112100,  # 2010 nxtchr: MOVB (r1)+,r0) ; load xmt char
                       0o001405,  # 2012         BEQ done       ; string is terminated with 0
                       0o110062,  # 2014         MOVB r0,2(r2)  ; write char to transmit buffer
                       0o000002,  # 2016
                       0o105712,  # 2020 wait:   TSTB (r2)      ; character transmitted?
                       0o100376,  # 2022         BPL wait       ; no, loop
                       0o000771,  # 2024         BR nxtchr      ; transmit next character
                       0o000000,  # 2026         halt
                       0o000763]  # 2030         br start
                                  # 2032         "Hello, World!" ; message
        program_address = 0o2000
        boot.load_machine_code(hello_world, program_address)

        message_address = 0o2032
        message_text = "Hello, World!\nHello, World!\nHello, World!" + chr(0)
        for i in range(0, len(message_text)):
            pdp11.ram.write_byte(message_address, ord(message_text[i]))
            message_address = message_address + 1

        pdp11.ram.dump(program_address, message_address)

        pdp11.reg.set_pc(0o2000, "load_machine_code")
        run = pdp11Run(pdp11)
        run.run(limit=100000)
        assert out.getvalue() == message_text[:-1].encode()
        assert pdp11.reg.get_pc() == 0o2030  # stopped at the HALT
        assert pdp11.CPU_cycles < 100000
        logging.info('test_hello_world done')
