//! Memory: bytes, with words stored low byte first.  The top 4K words are
//! the I/O page, where [`Machine`](crate::Machine) sends accesses to device
//! registers before falling back to this memory.

#[derive(Debug, Clone)]
pub struct Ram {
    memory: Vec<u8>,
    /// Highest byte address.
    pub top_of_memory: u32,
    /// First address of the I/O page.
    pub io_space: u32,
}

/// The vector area at the bottom of memory, filled with 0o277 at power-up.
pub const TOP_OF_VECTOR_SPACE: u32 = 0o274;

impl Ram {
    /// Memory addressed by `bits` address bits (16, 18 or 22 on real
    /// PDP-11s).
    pub fn new(bits: u32) -> Ram {
        let top_of_memory = (1u32 << bits) - 1;
        let io_space = top_of_memory - 0o17777;
        log::info!(
            "Ram: {bits} bits -> top_of_memory: 0o{top_of_memory:o}; io_space: 0o{io_space:o}"
        );
        let mut memory = vec![0; top_of_memory as usize + 1];
        memory[..TOP_OF_VECTOR_SPACE as usize].fill(0o277);
        Ram {
            memory,
            top_of_memory,
            io_space,
        }
    }

    pub fn read_byte(&self, address: u32) -> u8 {
        self.memory[address as usize]
    }

    pub fn write_byte(&mut self, address: u32, value: u8) {
        self.memory[address as usize] = value;
    }

    /// Reads a word.  The PDP-11/03 ignores bit 0 of a word address (the
    /// Python asserted the address was even).
    pub fn read_word(&self, address: u32) -> u16 {
        let a = (address & !1) as usize;
        u16::from_le_bytes([self.memory[a], self.memory[a + 1]])
    }

    pub fn write_word(&mut self, address: u32, value: u16) {
        let a = (address & !1) as usize;
        self.memory[a..a + 2].copy_from_slice(&value.to_le_bytes());
    }

    /// Clears memory below the I/O page.
    pub fn clear(&mut self) {
        let io = self.io_space as usize;
        self.memory[..io].fill(0);
    }

    /// Logs memory from `start` to `stop` as octal words, octal bytes and
    /// ASCII, eight bytes a line.
    pub fn dump(&self, start: u32, stop: u32) {
        let start = start & !0o7;
        let stop = ((stop & !0o7) + 0o10).min(self.top_of_memory + 1);
        log::info!("dump(0o{start:o}, 0o{stop:o})");
        for row in (start..stop).step_by(8) {
            let bytes = &self.memory[row as usize..row as usize + 8];
            let words: Vec<String> = bytes
                .chunks(2)
                .map(|w| format!("{:06o}", u16::from_le_bytes([w[0], w[1]])))
                .collect();
            let octal: Vec<String> = bytes.iter().map(|b| format!("{b:03o}")).collect();
            let ascii: String = bytes
                .iter()
                .map(|&b| {
                    if (16..127).contains(&b) {
                        b as char
                    } else {
                        '.'
                    }
                })
                .collect();
            log::info!(
                "{row:06o}  {}  {}  {ascii}",
                words.join(" "),
                octal.join(" ")
            );
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sizes() {
        for (bits, top, io) in [
            (16, 0o177777, 0o160000),
            (18, 0o777777, 0o760000),
            (24, 0o77777777, 0o77760000),
        ] {
            let r = Ram::new(bits);
            assert_eq!(r.top_of_memory, top);
            assert_eq!(r.io_space, io);
        }
    }

    #[test]
    fn vectors_filled() {
        let r = Ram::new(16);
        assert_eq!(r.read_byte(0), 0o277);
        assert_eq!(r.read_byte(TOP_OF_VECTOR_SPACE - 1), 0o277);
        assert_eq!(r.read_byte(TOP_OF_VECTOR_SPACE), 0);
    }

    #[test]
    fn byte() {
        let mut r = Ram::new(16);
        for v in [0o107, 0o270] {
            r.write_byte(0o4123, v);
            assert_eq!(r.read_byte(0o4123), v);
        }
    }

    #[test]
    fn bytes() {
        let mut r = Ram::new(16);
        let top = r.top_of_memory - 1;
        r.write_byte(top, 0o105);
        assert_eq!(r.read_byte(top), 0o105);
        for i in 0..0o277u32 {
            r.write_byte(i, i as u8);
        }
        for i in (0..0o277u32).step_by(2) {
            assert_eq!(u32::from(r.read_byte(i)), i);
        }
    }

    #[test]
    fn words() {
        let mut r = Ram::new(16);
        let top = r.top_of_memory - 1;
        for (value, address) in [(0o107070, 0o6234), (0o170707, 0o6234), (0o123232, top)] {
            r.write_word(address, value);
            assert_eq!(r.read_byte(address), (value & 0o377) as u8, "low byte");
            assert_eq!(r.read_byte(address + 1), (value >> 8) as u8, "high byte");
            assert_eq!(r.read_word(address), value);
        }
        for i in (1000..2000u32).step_by(2) {
            r.write_word(i, 0o123234 + i as u16);
        }
        for i in (1000..2000u32).step_by(2) {
            assert_eq!(r.read_word(i), 0o123234 + i as u16);
        }
    }

    #[test]
    fn word_byte() {
        let mut r = Ram::new(16);
        let base = 0o10000;
        for v in (0..0o30000u32).step_by(2) {
            r.write_word(base + v, v as u16);
        }
        for v in (0..0o30000u32).step_by(2) {
            let (lo, hi) = (
                u32::from(r.read_byte(base + v)),
                u32::from(r.read_byte(base + v + 1)),
            );
            assert_eq!(lo, v & 0o377);
            assert_eq!(hi, v >> 8);
            assert_eq!(lo | hi << 8, v);
        }
    }

    #[test]
    fn odd_word_address() {
        let mut r = Ram::new(16);
        r.write_word(0o1001, 0o123456);
        assert_eq!(r.read_word(0o1000), 0o123456);
        assert_eq!(r.read_word(0o1001), 0o123456);
    }
}
