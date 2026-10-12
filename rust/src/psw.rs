//! The processor status word.
//!
//! ```text
//! 15 14 current mode     0o140000
//! 13 12 previous mode    0o030000
//!  7  6  5 priority      0o000340
//!  4 T trap              0o000020
//!  3 N  2 Z  1 V  0 C    condition codes
//! ```
//!
//! It is also readable at [`ADDRESS`] in the I/O page.

/// Where the PSW appears in the I/O page.
pub const ADDRESS: u16 = 0o177776;

const MODE: u16 = 0o140000;
const MODE_AND_PREVIOUS: u16 = 0o170000;
const PRIORITY: u16 = 0o000340;
const TRAP: u16 = 0o000020;
const N: u16 = 0o10;
const Z: u16 = 0o4;
const V: u16 = 0o2;
const C: u16 = 0o1;
/// The condition code bits.
pub const NZVC: u16 = 0o17;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Psw(pub u16);

impl Psw {
    pub fn get(self) -> u16 {
        self.0
    }

    pub fn set(&mut self, value: u16) {
        self.0 = value;
    }

    /// Sets the current mode; the old current mode becomes the previous mode.
    pub fn set_mode(&mut self, mode: u16) {
        let old = self.0 & MODE_AND_PREVIOUS;
        self.0 = (self.0 & !MODE) | (mode << 14) | (old >> 2);
    }

    pub fn set_priority(&mut self, priority: u16) {
        self.0 = (self.0 & !PRIORITY) | ((priority << 5) & PRIORITY);
    }

    pub fn set_trap(&mut self, trap: bool) {
        self.put(TRAP, trap);
    }

    pub fn n(self) -> bool {
        self.0 & N != 0
    }
    pub fn z(self) -> bool {
        self.0 & Z != 0
    }
    pub fn v(self) -> bool {
        self.0 & V != 0
    }
    pub fn c(self) -> bool {
        self.0 & C != 0
    }

    pub fn set_n(&mut self, on: bool) {
        self.put(N, on);
    }
    pub fn set_z(&mut self, on: bool) {
        self.put(Z, on);
    }
    pub fn set_v(&mut self, on: bool) {
        self.put(V, on);
    }
    pub fn set_c(&mut self, on: bool) {
        self.put(C, on);
    }

    /// Sets all four condition codes.
    pub fn set_nzvc(&mut self, n: bool, z: bool, v: bool, c: bool) {
        self.set_n(n);
        self.set_z(z);
        self.set_v(v);
        self.set_c(c);
    }

    /// Sets N and Z from a result of the given size.
    pub fn set_nz(&mut self, size: crate::Size, result: u16) {
        self.set_n(result & size.sign() != 0);
        self.set_z(result & size.mask() == 0);
    }

    /// Replaces the condition code bits.
    pub fn set_nzvc_bits(&mut self, bits: u16) {
        self.0 = (self.0 & !NZVC) | (bits & NZVC);
    }

    /// The condition codes as a string such as "0100".
    pub fn nzvc(self) -> String {
        [self.n(), self.z(), self.v(), self.c()]
            .iter()
            .map(|&b| if b { '1' } else { '0' })
            .collect()
    }

    fn put(&mut self, bit: u16, on: bool) {
        if on {
            self.0 |= bit;
        } else {
            self.0 &= !bit;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn set_whole() {
        let mut p = Psw::default();
        p.set(0o123236);
        assert_eq!(p.get(), 0o123236);
    }

    #[test]
    fn fields() {
        let mut p = Psw::default();
        p.set_mode(0);
        p.set_mode(3);
        assert_eq!(p.get() & 0o140000, 0o140000, "mode");
        p.set_priority(0);
        p.set_priority(7);
        assert_eq!(p.get() & 0o340, 0o340, "priority");
        p.set_trap(false);
        p.set_trap(true);
        assert_eq!(p.get() & 0o20, 0o20, "trap");
        type Setter = fn(&mut Psw, bool);
        let setters: [(Setter, u16); 4] = [
            (Psw::set_n, 0o10),
            (Psw::set_z, 0o4),
            (Psw::set_v, 0o2),
            (Psw::set_c, 0o1),
        ];
        for (set, mask) in setters {
            set(&mut p, false);
            assert_eq!(p.get() & mask, 0);
            set(&mut p, true);
            assert_eq!(p.get() & mask, mask);
        }
    }

    #[test]
    fn nzvc_string() {
        let mut p = Psw::default();
        p.set_nzvc(true, false, true, false);
        assert_eq!(p.nzvc(), "1010");
        p.set_nzvc_bits(0o5);
        assert_eq!(p.nzvc(), "0101");
    }

    // test_hardware_psw's add_byte: an 8-bit add that sets V on overflow
    // out of the byte and Z on zero.
    fn add_byte(p: &mut Psw, b1: u16, b2: u16) -> u16 {
        let r = b1 + b2;
        if r > 0o377 {
            p.set_v(true);
        }
        if r & 0o377 == 0 {
            p.set_z(true);
        }
        r & 0o377
    }

    #[test]
    fn addb() {
        let mut p = Psw::default();
        assert_eq!(add_byte(&mut p, 3, 4), 7);
        assert!(!p.v() && !p.z());
        p.set(0);
        assert_eq!(add_byte(&mut p, 191, 191), 126);
        assert!(p.v() && !p.z());
    }
}
