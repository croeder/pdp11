//! A PDP-11/03 (LSI-11) emulator with the EIS instructions, a DL11 serial
//! console and the M9301-YA boot ROM.
//!
//! A Rust translation of the C and Go versions in this repository, which
//! translate the Python emulator at <https://github.com/timberwoof/PDP11>
//! with its instruction bugs fixed.  One [`Machine`] owns the registers,
//! memory, PSW and devices; the instruction groups are `impl Machine` blocks
//! in their own modules, named after the Python ones.

pub mod config;
pub mod logger;
pub mod stopwatches;
pub mod util;

pub mod psw;
pub mod ram;
pub mod registers;

pub mod address_modes;
pub mod machine;

pub mod br_ops;
pub mod cc_ops;
pub mod noopr_ops;
pub mod other_ops;
pub mod rss_ops;
pub mod ss_ops;
pub mod ssdd_ops;

pub mod boot;
pub mod dl11;
pub mod m9301;
pub mod terminal;

pub use machine::{Executed, Machine};

/// Operand size of an instruction.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Size {
    Word,
    Byte,
}

impl Size {
    /// The size given by bit 15 of an instruction.
    pub fn of(instruction: u16) -> Size {
        if instruction & 0o100000 != 0 {
            Size::Byte
        } else {
            Size::Word
        }
    }

    /// Mask of the bits an operand of this size has.
    pub fn mask(self) -> u16 {
        match self {
            Size::Word => 0o177777,
            Size::Byte => 0o377,
        }
    }

    /// The sign bit of an operand of this size.
    pub fn sign(self) -> u16 {
        match self {
            Size::Word => 0o100000,
            Size::Byte => 0o200,
        }
    }

    /// For byte operations only the low byte changes; the target's high byte
    /// is kept.  Word operations replace the whole word.
    pub fn merge(self, value: u16, target: u16) -> u16 {
        match self {
            Size::Word => value,
            Size::Byte => (value & 0o377) | (target & 0o177400),
        }
    }
}

/// Path of the directory that holds `config_default.json` and `source/`,
/// for tests.
#[cfg(test)]
pub(crate) fn repo_root() -> &'static std::path::Path {
    std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("the crate lives in a subdirectory of the repository")
}
