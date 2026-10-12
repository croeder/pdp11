"""PDP11 DL11 communications console"""
import logging
import pdp11_util as u

class DL11:
    """DEC DL11 serial interface emulator"""
    def __init__(self, ram, base_address):
        """dl11(ram object, base address for this device)"""
        logging.info(f'initializing dl11({oct(base_address)})')
        self.ram = ram
        logging.info(f'dl11 ram:{self.ram}')
        self.lock = ram.lock
        logging.info(f'dl11 lock:{self.lock}')
        self.RCSR_address = base_address
        self.RBUF_address = base_address + 2
        self.XCSR_address = base_address + 4
        self.XBUF_address = base_address + 6

        self.ram.register_io_writer(self.RCSR_address, self.write_RCSR)
        self.ram.register_io_writer(self.RBUF_address, self.write_RBUF)
        self.ram.register_io_writer(self.XCSR_address, self.write_XCSR)
        self.ram.register_io_writer(self.XBUF_address, self.write_XBUF)

        self.ram.register_io_reader(self.RCSR_address, self.read_RCSR)
        self.ram.register_io_reader(self.RBUF_address, self.read_RBUF)
        self.ram.register_io_reader(self.XCSR_address, self.read_XCSR)
        self.ram.register_io_reader(self.XBUF_address, self.read_XBUF)

        # RCSR Receiver Status Register bits
        # This is a DL11-B
        # no paper tape bits as for A and get_c.
        # no modem-control bits as for E
        self.RCSR_RCVR_ACT = 0o004000  # 11 RO - active (not implemented)
        self.RCSR_RCVR_DONE = 0o000200  # 7 RO
        #   set when character has been received,
        #   cleared when RBUF is read (addressed for read or write by CPU)
        self.RCSR_INT_ENB = 0o000100  # 6 - when set, enables interrupt (not implemented)
        self.RCSR = 0   # receive status register. Not active; no character yet
        self.RBUF = 0   # receive buffer

        # XCSR Transmitter Status Register bits
        # This is a DL11-B
        # No Break bit
        self.XCSR_XMIT_RDY = 0o000200  # 7 RO -
        #   set when transmitter can accept another character
        #   cleared by loading XBUF (write by CPU)
        self.XCSR_XMIT_INT_ENB = 0o000100  # 6 RW - when set, enables interrupt (not implemented)
        self.XCSR_MAINT = 0o000004  # 2 RW - maintenance loopback XBUF to RBUF
        self.XCSR = self.XCSR_XMIT_RDY   # transmit status register ready on init
        self.XBUF = 0   # transmit buffer

        self.i_set_lock = False
        logging.info(f'ram.lock: {self.lock}')
        logging.info('initializing dl11 done')

    # All reads and writes to IO buffers and CSRs must be protected.
    # CPU access to these routines is supposed to happen through hardware ram.read and ram.write methods.
    # DL11 access happens directly through these calls.

    def get_lock(self, whocalled=''):
        logging.info(f'get_lock called by {whocalled}')
        if not self.lock.locked(): #self.lock.is_set(): # lock was NOT set
            self.lock.acquire()
            logging.info('dl11 got lock')
            self.i_set_lock = True

    def release_lock(self, whocalled=''):
        logging.info(f'release_lock called by {whocalled}')
        if self.i_set_lock:
            self.lock.release()
            logging.info('dl11 released lock')
            self.i_set_lock = False

    def safe_character(self, byte):
        """return character if it is printable"""
        if byte > 31:
            result = chr(byte)
        else:
            low_ascii = ['NUL', 'SOH', 'STX', 'ETX', 'EOT', 'ENQ', 'ACK', 'BEL',
                         'BS', 'HT', 'LF', 'VF', 'FF', 'CR', 'SO', 'SI',
                         'DLE', 'DC1', 'DC2', 'DC3', 'DC4', 'NAK', 'SYN', 'ETB',
                         'CAN', 'EM', 'SUB', 'ESC', 'FS', 'GS', 'RS', 'US']
            result = low_ascii[byte]
        return result

    # DL11 Internal Interface to PDP-11 Emulator

    # Receiver receives characters from a terminal.
    # RCSR Receiver Status Register
    # 7: set when character has been received (ro)
    # 6: receiver interrupt enable (rw)
    # 0: reader enable (wo) clears 7
    def write_RCSR(self, byte):
        """write to receiver status register"""
        #logging.info(f' dl11.write_RCSR({oct(byte)})')
        self.get_lock('write_RCSR')
        self.RCSR = byte
        self.release_lock('write_RCSR')

    def read_RCSR(self):
        """read from receiver status register"""
        #logging.info(f' dl11.read_RCSR() returns {oct(self.RCSR)}')
        self.get_lock('read_RCSR')
        result = self.RCSR
        self.release_lock('read_RCSR')
        return result

    # RBUF receiver data buffer (ro)
    # 15: error
    # 14: overrun
    # 13: framing error
    # 12: receive parity error
    # 7-0 received data
    def write_RBUF(self, byte):
        """DL11 calls this to write to receiver buffer and set ready bit"""
        logging.info(f'dl11.write_RBUF {oct(byte)} {self.safe_character(byte)}')
        self.get_lock(f'write_RBUF({oct(byte)})')
        self.RBUF = byte
        self.RCSR = self.RCSR | self.RCSR_RCVR_DONE
        if (self.RCSR & self.RCSR_RCVR_DONE) != self.RCSR_RCVR_DONE:
            logging.error(f'XCSR {oct(self.RCSR)} was not set with {oct(self.RCSR_RCVR_DONE)}')
        self.release_lock('write_RBUF')
        logging.info('dl11.write_RBUF exit')

    def read_RBUF(self):
        """PDP11 calls this to read from receiver buffer. Read buffer and reset ready bit"""
        self.get_lock('read_RBUF')
        result = self.RBUF
        #logging.info(f'dl11.read_RBUF() returns {oct(result)} {self.safe_character(result)}')
        RCSR = self.RCSR & ~self.RCSR_RCVR_DONE
        self.RCSR = RCSR
        if (self.RCSR & self.RCSR_RCVR_DONE) == self.RCSR_RCVR_DONE:
            logging.error(f'XCSR {oct(self.RCSR)} was not cleared with {oct(self.RCSR_RCVR_DONE)}')
        #   cleared when RBUF is read
        self.release_lock('read_RBUF')
        return result

    # Transmitter enables CPU to send characters to a terminal.
    # XCSR transmit status register (rw)
    # 7: transmitter ready (ro).
    #   Cleared when XBUF is loaded.
    #   Set when XBUF can accept another character.
    # 6: transmit interrupt enable (rw)
    # 2: maintenance. when set sends serial output to serial input (rw)
    # 0: break. when set sends continuous space (rw)
    def write_XCSR(self, byte):
        """write to transmitter status register"""
        self.get_lock('write_XCSR')
        #logging.debug(f'dl11.write_XCSR({oct(byte)})') # often gives uninteresting results
        # make the RW and RO bits play nice
        # only two are implemented so far
        self.XCSR = byte
        self.release_lock('write_XCSR')

    def read_XCSR(self):
        """read from transitter status register"""
        self.get_lock('read_XCSR')
        result = self.XCSR
        self.release_lock('read_XCSR')
        #logging.debug(f'dl11.read_XCSR returns {oct(result)}')
        return result

    # XBUF transmit data buffer (wo)
    # 7-0 transmitted data buffer
    def write_XBUF(self, byte):
        """PDP11 calls this to write to transmitter buffer register."""
        self.get_lock('write_XBUF({oct(byte)})')
        #logging.debug(f'dl11.write_XBUF({oct(byte)}) {self.safe_character(byte)}"')
        self.XBUF = byte
        # self.XCSR_XMIT_RDY is cleared when XBUF is loaded
        XCSR = self.XCSR & ~self.XCSR_XMIT_RDY
        self.XCSR = XCSR
        if (self.XCSR & self.XCSR_XMIT_RDY) == self.XCSR_XMIT_RDY:
            logging.error(f'XCSR {oct(self.XCSR)} was not cleared with {oct(self.XCSR_XMIT_RDY)}')
        # check for Maintenance mode
        if (self.XCSR & self.XCSR_MAINT) == self.XCSR_MAINT:
            self.write_RBUF(byte)
        self.release_lock('write_XBUF')

    def read_XBUF(self):
        """DL11 calls this to read from transmitter buffer register."""
        self.get_lock('read_XBUF')
        result = self.XBUF
        # self.XCSR_XMIT_RDY is set when XBUF can accept another character
        XCSR = self.XCSR | self.XCSR_XMIT_RDY # this works
        self.XCSR = XCSR # This fails! *****
        if (self.XCSR & self.XCSR_XMIT_RDY) != self.XCSR_XMIT_RDY:
            logging.error(f'XCSR {oct(self.XCSR)} was not set with {oct(self.XCSR_XMIT_RDY)}')
        #logging.debug(f'dl11.read_XBUF returns {oct(result)} {self.safe_character(result)}')
        self.release_lock('read_XBUF')
        return result

