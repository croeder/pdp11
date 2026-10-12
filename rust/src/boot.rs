//! Loading programs into memory (pdp11_boot): machine code from a slice, or
//! the addresses and words of a DEC assembly listing.

use std::fs;
use std::io;
use std::path::Path;

use crate::util::oct;
use crate::Machine;

/// The bootstrap loader from the PDP-11/40 handbook.
pub const BOOTSTRAP_LOADER: [u16; 15] = [
    0o016701, 0o000240, 0o012702, 0o000240, 0o005211, 0o105711, 0o100376, 0o116162, 0o000240,
    0o000240, 0o005267, 0o177756, 0o000765, 0o177560, 0o000000,
];
pub const BOOT_ADDRESS: u16 = 0o000744;

/// Copies each character typed on the console back to it.
pub const ECHO: [u16; 8] = [
    0o012700, 0o177560, // start: mov #kbs, r0
    0o105710, //           wait:  tstb (r0)       ; character received?
    0o100376, //                  bpl wait        ; no, loop
    0o016060, 0o000002, 0o000006, // mov 2(r0),6(r0) ; transmit data
    0o000772, //                  br wait         ; get next character
];
pub const ECHO_ADDRESS: u16 = 0o001000;

impl Machine {
    /// Copies `code` into memory at `base` and sets the PC to `base`.
    pub fn load_machine_code(&mut self, code: &[u16], base: u16) {
        let mut address = base;
        for &word in code {
            self.write_word(address, word);
            address = address.wrapping_add(2);
        }
        self.reg.set_pc(base);
    }

    /// Loads a DEC assembly listing: each line that starts with an octal
    /// address followed by an octal word stores that word there.  Returns
    /// the first address.
    pub fn load_listing(&mut self, path: impl AsRef<Path>) -> io::Result<u16> {
        let path = path.as_ref();
        let text = fs::read_to_string(path)?;
        let mut base = None;
        for line in text.lines() {
            let mut fields = line.split_whitespace();
            let Some(address) = fields.next().and_then(octal) else {
                continue;
            };
            base.get_or_insert(address);
            let value = fields.next().ok_or_else(|| {
                io::Error::new(
                    io::ErrorKind::InvalidData,
                    format!("{}: no value in {line:?}", path.display()),
                )
            })?;
            if let Some(value) = octal(value) {
                self.write_word(address, value);
            }
        }
        let base = base.unwrap_or(0);
        log::info!(
            "load_listing \"{}\" returns base address:{}",
            path.display(),
            oct(base.into())
        );
        Ok(base)
    }
}

/// An unsigned octal number, digits only.
fn octal(s: &str) -> Option<u16> {
    if s.bytes().all(|b| b.is_ascii_digit()) {
        u16::from_str_radix(s, 8).ok()
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn octal_fields() {
        assert_eq!(octal("165000"), Some(0o165000));
        assert_eq!(octal("DIAGS:"), None);
        assert_eq!(octal("+12"), None);
    }

    #[test]
    fn load_code() {
        let mut m = Machine::new();
        m.load_machine_code(&ECHO, ECHO_ADDRESS);
        assert_eq!(m.reg.pc(), ECHO_ADDRESS);
        assert_eq!(m.read_word(ECHO_ADDRESS + 2), 0o177560);
    }

    #[test]
    fn load_rom_listing() {
        let mut m = Machine::new();
        let base = m
            .load_listing(crate::repo_root().join(crate::m9301::ROM_FILE))
            .unwrap();
        assert_eq!(base, 0o165000);
        assert_eq!(m.read_word(0o165000), 0o005001); // CLR R1
        assert_eq!(m.read_word(0o165026), 0o005501); // ADC R1
    }
}
