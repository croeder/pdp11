//! The DEC DL11 serial line interface: receiver status and buffer,
//! transmitter status and buffer, at four consecutive words.

use crate::util::{oct, safe_character};

/// RCSR bit 7, read only: a character has been received; cleared when RBUF is read.
pub const RCSR_RCVR_DONE: u16 = 0o200;
/// RCSR bit 6: interrupt enable (interrupts are not emulated).
pub const RCSR_INT_ENB: u16 = 0o100;
/// XCSR bit 7, read only: the transmitter can take another character.
pub const XCSR_XMIT_RDY: u16 = 0o200;
/// XCSR bit 6: interrupt enable (interrupts are not emulated).
pub const XCSR_XMIT_INT_ENB: u16 = 0o100;
/// XCSR bit 2: maintenance, loops the transmitter back to the receiver.
pub const XCSR_MAINT: u16 = 0o4;

/// The console's DL11 address.
pub const CONSOLE: u16 = 0o177560;

#[derive(Debug, Clone)]
pub struct Dl11 {
    pub base: u16,
    pub rcsr: u16,
    pub rbuf: u16,
    pub xcsr: u16,
    pub xbuf: u16,
}

impl Dl11 {
    pub fn new(base: u16) -> Dl11 {
        log::info!("initializing dl11({})", oct(base.into()));
        Dl11 {
            base,
            rcsr: 0,
            rbuf: 0,
            xcsr: XCSR_XMIT_RDY,
            xbuf: 0,
        }
    }

    pub fn rcsr_address(&self) -> u16 {
        self.base
    }
    pub fn rbuf_address(&self) -> u16 {
        self.base + 2
    }
    pub fn xcsr_address(&self) -> u16 {
        self.base + 4
    }
    pub fn xbuf_address(&self) -> u16 {
        self.base + 6
    }

    /// Whether a bus address is one of this interface's registers.
    pub fn decodes(&self, address: u16) -> bool {
        address & 1 == 0 && (self.base..self.base + 8).contains(&address)
    }

    /// A bus read of one of the registers.  Reading RBUF clears RCVR_DONE;
    /// reading XBUF takes the character and sets XMIT_RDY again.
    pub fn read(&mut self, address: u16) -> u16 {
        match address - self.base {
            0 => self.rcsr,
            2 => self.take_received(),
            4 => self.xcsr,
            _ => self.take_transmitted(),
        }
    }

    /// A bus write of one of the registers.
    pub fn write(&mut self, address: u16, value: u16) {
        match address - self.base {
            0 => self.rcsr = value,
            2 => self.receive(value),
            4 => self.xcsr = value,
            _ => self.transmit(value),
        }
    }

    /// The line delivers a character: it goes in RBUF and RCVR_DONE is set.
    pub fn receive(&mut self, c: u16) {
        log::info!("dl11 receive {} {}", oct(c.into()), safe_character(c));
        self.rbuf = c;
        self.rcsr |= RCSR_RCVR_DONE;
    }

    pub fn receiver_full(&self) -> bool {
        self.rcsr & RCSR_RCVR_DONE != 0
    }

    fn take_received(&mut self) -> u16 {
        self.rcsr &= !RCSR_RCVR_DONE;
        self.rbuf
    }

    /// The CPU loads XBUF: XMIT_RDY clears until the line takes the
    /// character.  In maintenance mode it loops back to the receiver.
    fn transmit(&mut self, c: u16) {
        self.xbuf = c;
        self.xcsr &= !XCSR_XMIT_RDY;
        if self.xcsr & XCSR_MAINT != 0 {
            self.receive(c);
        }
    }

    /// Whether a character is waiting in XBUF to go out on the line.
    pub fn transmitting(&self) -> bool {
        self.xcsr & XCSR_XMIT_RDY == 0
    }

    /// The line takes the transmitted character; XMIT_RDY sets again.
    pub fn take_transmitted(&mut self) -> u16 {
        self.xcsr |= XCSR_XMIT_RDY;
        self.xbuf
    }
}

#[cfg(test)]
mod tests {
    //! test_DL11: the registers as the CPU sees them on the bus.  The
    //! Python test used one random character; these use them all.
    use super::*;
    use crate::Machine;

    fn machine() -> (Machine, Dl11) {
        let mut m = Machine::new();
        let d = Dl11::new(CONSOLE);
        m.dl11 = Some(d.clone());
        (m, d)
    }

    #[test]
    fn receive() {
        let (mut m, d) = machine();
        for c in 32..=128 {
            assert_eq!(m.read_word(d.rcsr_address()), 0, "nothing received");
            m.write_word(d.rbuf_address(), c); // the line delivers a character
            assert_eq!(
                m.read_word(d.rcsr_address()) & RCSR_RCVR_DONE,
                RCSR_RCVR_DONE
            );
            assert_eq!(m.read_word(d.rbuf_address()), c);
            assert_eq!(
                m.read_word(d.rcsr_address()) & RCSR_RCVR_DONE,
                0,
                "receiver empty again"
            );
        }
    }

    #[test]
    fn transmit() {
        let (mut m, d) = machine();
        for c in 32..=128 {
            assert_eq!(
                m.read_word(d.xcsr_address()) & XCSR_XMIT_RDY,
                XCSR_XMIT_RDY,
                "ready"
            );
            m.write_word(d.xbuf_address(), c);
            assert_eq!(m.read_word(d.xcsr_address()) & XCSR_XMIT_RDY, 0, "busy");
            assert_eq!(m.read_word(d.xbuf_address()), c);
            assert_eq!(
                m.read_word(d.xcsr_address()) & XCSR_XMIT_RDY,
                XCSR_XMIT_RDY,
                "ready again"
            );
        }
    }

    #[test]
    fn maintenance_loops_back() {
        let (mut m, d) = machine();
        let xcsr = m.read_word(d.xcsr_address());
        m.write_word(d.xcsr_address(), xcsr | XCSR_MAINT);
        assert_eq!(m.read_word(d.xcsr_address()) & XCSR_MAINT, XCSR_MAINT);
        assert_eq!(m.read_word(d.rcsr_address()), 0, "nothing received");
        for c in 32..=128 {
            assert_eq!(m.read_word(d.xcsr_address()) & XCSR_XMIT_RDY, XCSR_XMIT_RDY);
            m.write_word(d.xbuf_address(), c);
            assert_eq!(m.read_word(d.xcsr_address()) & XCSR_XMIT_RDY, 0);
            assert_eq!(
                m.read_word(d.rcsr_address()) & RCSR_RCVR_DONE,
                RCSR_RCVR_DONE
            );
            assert_eq!(m.read_word(d.rbuf_address()), c);
            assert_eq!(m.read_word(d.xbuf_address()), c);
            assert_eq!(m.read_word(d.xcsr_address()) & XCSR_XMIT_RDY, XCSR_XMIT_RDY);
        }
    }

    #[test]
    fn byte_access() {
        let (mut m, d) = machine();
        m.write_byte(d.xbuf_address(), b'x'); // MOVB R0,2(R2) in hello world
        assert_eq!(
            m.read_byte(d.xcsr_address()),
            0,
            "TSTB sees the transmitter busy"
        );
        assert_eq!(m.read_word(d.xbuf_address()), u16::from(b'x'));
        assert_eq!(m.read_byte(d.xcsr_address()), 0o200);
    }
}
