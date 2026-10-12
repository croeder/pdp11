//! The M9301 bootstrap/terminator card with the M9301-YA ROM.
//!
//! At power-up the CPU starts at the address set on the card's switches;
//! with them all off that is the ROM's console emulator and CPU diagnostics
//! at 0o173000.  Reading 0o173024 returns the switch settings.
//!
//! (The Python's RK11 disk interface was a stub that nothing attached; it is
//! not translated.)

pub const BASE_ADDRESS: u16 = 0o173000;
pub const SWITCH_ADDRESS: u16 = 0o173024;
/// The ROM listing, relative to the repository root.
pub const ROM_FILE: &str = "source/M9301-YA.txt";

#[derive(Debug, Clone)]
pub struct M9301 {
    /// The start address the switches select (address switches 81-3 to 81-10
    /// all off, power-up reboot switch 81-2 on).
    pub switch_settings: u16,
}

impl M9301 {
    pub fn new() -> M9301 {
        M9301 {
            switch_settings: BASE_ADDRESS,
        }
    }

    pub fn read_switches(&self) -> u16 {
        log::info!(
            "                                  ; read_dip_switches returns 0o{:o}",
            self.switch_settings
        );
        self.switch_settings
    }
}

impl Default for M9301 {
    fn default() -> M9301 {
        M9301::new()
    }
}
