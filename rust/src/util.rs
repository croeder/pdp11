//! Formatting helpers (pdp11_util).
//!
//! The Python's conversions between PDP-11 words and Python integers
//! (pythonifyPDP11Word, twosComplementNegative and the rest) are casts and
//! wrapping arithmetic in Rust, and its octal padding is `format!("{:06o}")`,
//! so only these remain.

/// Formats like Python's `oct()`: `0o27`, `-0o5`, `0o0`.
pub fn oct(value: i64) -> String {
    if value < 0 {
        format!("-0o{:o}", -value)
    } else {
        format!("0o{value:o}")
    }
}

const LOW_ASCII: [&str; 32] = [
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL", "BS", "HT", "LF", "VF", "FF", "CR",
    "SO", "SI", "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB", "CAN", "EM", "SUB", "ESC",
    "FS", "GS", "RS", "US",
];

/// A control character's name, or the character itself, for logging.
pub fn safe_character(c: u16) -> String {
    match c {
        0..=31 => LOW_ASCII[c as usize].to_string(),
        32..=126 => char::from(c as u8).to_string(),
        _ => format!("\\x{c:02x}"),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn oct_matches_python() {
        assert_eq!(oct(0o27), "0o27");
        assert_eq!(oct(0), "0o0");
        assert_eq!(oct(-5), "-0o5");
    }

    #[test]
    fn safe_character_names_controls() {
        assert_eq!(safe_character(13), "CR");
        assert_eq!(safe_character(u16::from(b'A')), "A");
        assert_eq!(safe_character(0o377), "\\xff");
    }

    // test_utils.py checked the Python integer conversions; these are the
    // Rust casts that replace them, on the same values.
    #[test]
    fn conversions() {
        for (positive, negative) in [(0u16, 0u16), (1, 65535), (2, 65534), (586, 64950)] {
            assert_eq!(positive.wrapping_neg(), negative);
        }
        for (word, signed) in [
            (0u16, 0i16),
            (1, 1),
            (65535, -1),
            (65534, -2),
            (64950, -586),
        ] {
            assert_eq!(word as i16, signed);
            assert_eq!(signed as u16, word);
        }
        for (byte, signed) in [(0u8, 0i8), (1, 1), (255, -1), (254, -2), (127, 0o177)] {
            assert_eq!(byte as i8, signed);
        }
        assert_eq!(0o42400042u32 as i32, 9044002);
        assert_eq!(0o37735377736u32 as i32, -9044002);
        assert_eq!(format!("{:03o}", 0o27), "027");
        assert_eq!(format!("{:06o}", 0o2127), "002127");
        assert_eq!(format!("{:<6}", "arf"), "arf   ");
    }
}
