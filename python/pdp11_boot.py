"""PDP11 bootstrap utilities"""
import logging

#from pdp11_hardware import Registers as reg
#from pdp11_hardware import Ram
class pdp11Boot:
    """load machine code file or assembly file with machine ccode into pdp11 ram"""
    def __init__(self, reg, ram):
        logging.info('initializing pdp11Boot')
        self.reg = reg
        self.ram = ram

    # from pdp-11/40 book
    bootstrap_loader = [0o016701,  # MOV 0o67:0o0 0o1:0o0
                        0o000240,  # 0o000026
                        0o012702,  # MOV 0o27:0o0 0o2:0o0
                        0o000240,  # 0o000352
                        0o005211,  # INC 0o0 0o0 incomplete
                        0o105711,  # CLR 0o0 0o1
                        0o100376,  # BPL 0o376
                        0o116162,  # MOVB 0o61:0o377 0o62:0o377
                        0o000240,  # 0o000002 RTI
                        0o000240,  # BR 0o0
                        0o005267,  # INC 0o770 0o5267
                        0o177756,
                        0o000765,  # BR 0o365
                        0o177560,  # 0o177560
                        0o000000]  #
    # NOP 0o000240
    # 0o000400 BR 00 - what's at 00 now?
    bootaddress = 0o000744


    echo = [0o012700,  0o177560,  # start: mov #kbs, r0
            0o105710,             # wait: tstb (r0)       ; character received?
            0o100376,             # bpl wait        ; no, loop
            0o016060, 0o000002, 0o000006,  # mov 2(r0),6(r0) ; transmit data
            0o000772]             # br wait         ; get next character
    echo_address = 0o001000

    def load_machine_code(self, code, base):
        """Copy the pdp-11 machine code found at code into the address in base

        :param code:
        :param base:
        :return:
        """
        # logging.info(f'load_machine_code({base})')
        address = base
        for instruction in code:
            # logging.info()
            # logging.info(f'bootaddress:{oct(address)}  instruction: {oct(instruction)}')
            self.ram.write_word(address, instruction)
            # logging.info(f'    {oct(address)}:{oct(self.ram.read_word(address))}')
            address = address + 2
        self.reg.set_pc(base, "load_machine_code")

    def octal_to_decimal(self, octal_value):
        """convert an octal masquerading as an integer to decimal"""
        decimal_value = 0
        base = 1
        while octal_value:
            last_digit = octal_value % 10
            octal_value = int(octal_value / 10)
            decimal_value += last_digit * base
            base = base * 8
        return decimal_value

    def read_pdp11_assembly_file(self, file):
        """read a DEC-formatted assembly file with machine code
        :param file: path to file
        :return: address specified in the file
        """
        # logging.info(f'read_pdp11_assembly_file "{file}"')
        base = 0
        with open(file, 'r', encoding="utf-8") as text:
            for line in text:
                #if line.strip() != "":
                #    logging.info(line.strip())
                parts = line.split()

                # if the line is empty, slip it
                if len(parts) == 0:
                    continue
                part0 = parts[0]

                # if the line starts with ;, skip it
                if part0 == ';':
                    continue

                # first item is the address
                if part0.isnumeric():
                    address = self.octal_to_decimal(int(part0))
                    # the first address is the base address
                    if base == 0:
                        base = address

                # if it doesn't make sense, skip it
                else:
                    continue

                # get the next value
                part1 = parts[1]
                if part1.isnumeric():
                    value1 = self.octal_to_decimal(int(part1))
                    # log what we got. octal, octal, decimal, decimal
                    # logging.info(part0, part1, address, value1)
                    self.ram.write_word(address, value1)

        logging.info(f'read_pdp11_assembly_file "{file}" returns base address:{oct(base)}')
        return base
