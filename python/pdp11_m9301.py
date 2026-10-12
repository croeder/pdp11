"""PDP11 M9301 Boot Rom"""
import logging

class M9301:
    """Emulates PDP11 M9301 boot ROM card"""
    def __init__(self, reg, ram, boot):
        logging.info('initializing M9301')
        # 7.3 Micrositch Settings. See p. 7-2 of
        # PDP11 M9301 bootstrap/terminator module maintenance and operator's manual
        # https://gunkies.org/wiki/M9301_ROM

        # M9301-YA - (ROMs 034A9, 035A9, 036A9, 037A9) -
        # PDP-11/04 and PDP-11/34 OEM version; had basic diagnostics,
        # console emulator, booted from various devices
        # (RK11 and RP11 disks, TC11 DECtape, TM11 magnetic tape, serial line,
        # PC11 high-speed paper-tape reader, TA11 casette tape, RX11 8-inch floppy disk),
        # supported auto-boot on power on, and also power-fail restart

        # M9301-YB - (ROMs 038A9, 039A9, 040A9, 041A9) -
        # /04 and /34 end user version; had basic diagnostics,
        # console emulator, booted from various devices
        # (RK11, RP11, TC11, TM11, TA11, RX11, serial line,
        # high-speed paper-tape reader, RJS, RJP, TJU),
        # also power-fail restart

        # M9301-YF - (ROMs 480A9, 481A9, 482A9, 483A9) -
        # All models (auto-start not available on PDP-11/45, PDP-11/50);
        # had basic diagnostics, console emulator, booted from various devices
        # (RK11, RK06, RP11, TC11, TM11, TA11, RX11, serial line,
        # high-speed paper-tape reader, RJS, RJP, TJU),
        # supported auto-boot on power on, and also power-fail restart

        # M9301-YH - (ROMs 332A9, 333A9, 334A9, 335A9) -
        # /60 and /70 version; contained basic CPU, cache and memory diagnostics,
        # booted from various devices
        # (TM11, TC11, RK11, RP11, RK06, RJS, RJP, TJU, RX11, high-speed paper-tape reader)

        self.ram = ram
        #m9301_start_address = ram.top_of_memory - 0o4777  # *** Only supports 16-bit address for now.

        self.base_address = 0o173000
        self.switch_address = 0o173024
        self.switch_settings = 0o0

        #switch81_1 = True  # low rom enable ON = True for normal operation.
        # ON: get PC and PSW from 0o173024 and 0o173026
        # OFF: get PC and PSW from 0o024 and 0o026,
        # Do not respond to address requests between 0o765000 and 0o765777

        switch81_2 = True  # power up reboot enable ON = True
        # The boot switch is on the console
        # If this is on (True) then processor will trap to a selectable address.
        # Switches 3-10 set the address.
        # If this is off (False) then  processor will trap to location 24 on startup.
        # The new PC will be the logical OR of the contents of ROM location 773024
        # and the eight microswitches on the M9301-YA
        # 173024 173000
        # eight switches and one implied 0 is 9 bits or 3 octal digits.
        # So they set those last zeroes.
        #
        # Entry point -- Console emulator w/ CPU Diagnostics 1-5
        # DIP switches xx11111111  (173000)
        # 173000 000574		BR	GODIAG
        #
        # Entry point -- Console Emulator
        # DIP switches xx11110011  (173030)
        # 173372 000137	GODIAG:	JMP	@#DIAGS		; Jump to diagnostics
        #
        # Entry point -- RK11 Boot w/ Diagnostic 1-5
        # DIP switches xx01101111  (173440)
        # 173440 000754		BR	GODIAG
        #
        # Entry point -- DL11 Boot w/ Diagnostic 1-5
        # DIP switches xx00101011  (173650)
        # 173650 000650		BR	GODIAG
        # 0b1111011-11010100-0

        # Address switches. Configure this by setting the on/off variable to on or off.
        # These switches are at 0o173024
        on = 0o000000 # 0 bits in switches
        off = 0o000777 # 1 bits in switchws
        #  	DIP switches xx01101111  (173440) - ran all the diagnosics and barfed on final boot
        switch81_3 = 0o0400 & on    # bus address bit 8
        switch81_4 = 0o0200 & on   # bus address bit 7
        switch81_5 = 0o0100 & on   # bus address bit 6
        switch81_6 = 0o0040 & on    # bus address bit 5
        switch81_7 = 0o0020 & on   # bus address bit 4
        switch81_8 = 0o0010 & on   # bus address bit 3
        switch81_9 = 0o0004 & on   # bus address bit 2
        switch81_10 = 0o0002 & on  # bus address bit 1
        # Set the switches you want to have '1' digits to on.
        # Where the DEC manual says to have a switch OFF, set off.
        # Thus none of the switches results in CPU Diagnostics with Console Emulator:
        # PC = m9301_start_address
        # console boot switch causes M9301 to be activated
        # causes normal boot-up sequence
        # new PC address is logical OR of 0o173000 and selected microswitches
        # To set CPU diag. Boot RK11:
        # PC = m9301_start_address + switch81_3 + switch81_6
        if switch81_2:
            # Load boot rom image into RAM (165000 - 173776 length 0o6776
            # Manual says  773000 through 773776 length 0o776
            # That's an offset of about 0o600000
            boot.read_pdp11_assembly_file('source/M9301-YA.txt')
            self.switch_settings = switch81_3 + switch81_4 + switch81_5 + switch81_6 + \
                 switch81_7 + switch81_8 + switch81_9 + switch81_10 + self.base_address
            logging.info(f'switch_address:{oct(self.switch_address)} switch_settings:{oct(self.switch_settings)}')
            reg.set_pc(self.switch_settings, 'M9301 switch81_2 True')
        else:
            reg.set_pc(0o24,"M9301 else switch81_2 False")

        # Register address 173024 like an io device
        self.ram.register_io_reader(self.switch_address, self.read_dip_switches)

        logging.info(f'initializing M9301 done. PC:{oct(reg.get_pc())}')

    # Pylint complains that this has too few public methods.
    # That's perfectly acceptable as this only does anything on startup.

    def read_dip_switches(self):
        '''io device method returns the dip switch positions'''
        logging.info(f'                                  ; read_dip_switches returns {oct(self.switch_settings)}')
        return self.switch_settings
