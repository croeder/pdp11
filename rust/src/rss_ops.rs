//! The EIS instructions, XOR and SOB, `07 0R SS` - `07 7R SS`
//! (pdp11_rss_ops).  Semantics follow the DEC processor handbook, and SIMH
//! for the DIV error flags; the notes say where the Python differed.

use crate::util::oct;
use crate::{Executed, Machine, Size};

/// The mnemonic of `instruction & 0o077000`.
pub fn rss_name(opcode: u16) -> Option<&'static str> {
    Some(match opcode {
        0o070000 => "MUL",
        0o071000 => "DIV",
        0o072000 => "ASH",
        0o073000 => "ASHC",
        0o074000 => "XOR",
        0o077000 => "SOB",
        _ => return None,
    })
}

pub fn is_rss_op(instruction: u16) -> bool {
    instruction & 0o100000 == 0
        && instruction & 0o070000 == 0o070000
        && rss_name(instruction & 0o077000).is_some()
}

/// Shifts the low `bits` bits of `value` by a 6-bit signed count (-32 to
/// 31; negative shifts right).  Returns the result, whether the sign changed
/// during a left shift (V), and the last bit shifted out (C).
fn arith_shift(value: u32, bits: u32, count: u16) -> (u32, bool, bool) {
    let mask = if bits == 32 {
        u32::MAX
    } else {
        (1 << bits) - 1
    };
    let sign = 1 << (bits - 1);
    let n = count & 0o77;
    let mut value = value & mask;
    let (mut v, mut c) = (false, false);
    if n & 0o40 != 0 {
        for _ in 0..(0o100 - n) {
            c = value & 1 != 0;
            value = value >> 1 | value & sign;
        }
    } else {
        for _ in 0..n {
            c = value & sign != 0;
            let shifted = (value << 1) & mask;
            v |= shifted & sign != value & sign;
            value = shifted;
        }
    }
    (value, v, c)
}

impl Machine {
    /// MUL: (R, R|1) <- R * src, high word to R, low to R|1 (an odd R gets
    /// the low word).  C is set if the product does not fit in 16 bits.
    /// (The Python never set C.)
    fn mul(&mut self, r: usize, source: u16) {
        let p = i32::from(self.reg.get(r) as i16) * i32::from(source as i16);
        self.reg.set(r, (p >> 16) as u16);
        self.reg.set(r | 1, p as u16);
        self.psw
            .set_nzvc(p < 0, p == 0, false, i16::try_from(p).is_err());
    }

    /// DIV: (R, R|1) / src.  Truncates toward zero; the remainder takes the
    /// dividend's sign.  Divide by zero sets V and C, and quotient overflow
    /// sets V; both leave the registers unchanged.  (The Python tested the
    /// numerator for zero, raised on a zero divisor, and corrected its
    /// quotient wrongly for exact and doubly negative cases.)
    fn div(&mut self, r: usize, divisor: u16) {
        if divisor == 0 {
            log::info!("DIV divide by zero");
            self.psw.set_nzvc(false, true, true, true);
            return;
        }
        let dividend = (u32::from(self.reg.get(r)) << 16 | u32::from(self.reg.get(r | 1))) as i32;
        let (n, d) = (i64::from(dividend), i64::from(divisor as i16));
        let q = n / d;
        let Ok(quotient) = i16::try_from(q) else {
            log::info!("DIV overflow");
            self.psw.set_nzvc(q < 0, false, true, false);
            return;
        };
        self.reg.set(r, quotient as u16);
        self.reg.set(r | 1, (n % d) as u16);
        self.psw.set_nzvc(q < 0, q == 0, false, false);
    }

    /// ASH.  (The Python took the sign from the register number and never
    /// set C.)
    fn ash(&mut self, r: usize, count: u16) {
        let (result, v, c) = arith_shift(self.reg.get(r).into(), 16, count);
        let result = result as u16;
        self.reg.set(r, result);
        self.psw.set_nzvc(result & 0o100000 != 0, result == 0, v, c);
    }

    /// ASHC: (R, R|1) shifted as one 32-bit value; for an odd R, R ends up
    /// with the low word.  (The Python shifted R right by the whole source.)
    fn ashc(&mut self, r: usize, count: u16) {
        let value = u32::from(self.reg.get(r)) << 16 | u32::from(self.reg.get(r | 1));
        let (result, v, c) = arith_shift(value, 32, count);
        self.reg.set(r, (result >> 16) as u16);
        self.reg.set(r | 1, result as u16);
        self.psw
            .set_nzvc(result & 0x8000_0000 != 0, result == 0, v, c);
    }

    pub fn do_rss_op(&mut self, instruction: u16) -> Executed {
        let opcode = instruction & 0o077000;
        let r = usize::from((instruction >> 6) & 7);
        let field = instruction & 0o77;
        let Some(name) = rss_name(opcode) else {
            return Executed::stopped(format!("Error: EIS opcode 0o{instruction:o} not found"));
        };
        match opcode {
            // SOB R,NN: R <- R - 1; if R != 0, PC <- PC - 2 * NN.  (The
            // Python multiplied R by the operand.)
            0o077000 => {
                let v = self.reg.get(r).wrapping_sub(1);
                self.reg.set(r, v);
                if v != 0 {
                    self.reg.set_pc(self.reg.pc().wrapping_sub(2 * field));
                }
                Executed::running(format!("SOB R{r},{}", oct(field.into())))
            }
            // XOR R,DD: R is read before the destination is evaluated, so XOR
            // R1,(R1)+ uses R1's value from before the increment, as on the
            // 11/03.  C is unaffected.  (The Python wrote the result into R
            // and did not clear V.)
            0o074000 => {
                let value = self.reg.get(r);
                let dest = self.operand(Size::Word, field);
                let result = value ^ dest.value;
                self.psw.set_nz(Size::Word, result);
                self.psw.set_v(false);
                self.store(Size::Word, &dest, result);
                Executed::running(format!("XOR R{r},{dest}"))
            }
            // MUL, DIV, ASH, ASHC SS,R.  (The Python fetched the source as a
            // byte and stored every result in R.)
            _ => {
                let source = self.operand(Size::Word, field);
                match opcode {
                    0o070000 => self.mul(r, source.value),
                    0o071000 => self.div(r, source.value),
                    0o072000 => self.ash(r, source.value),
                    _ => self.ashc(r, source.value),
                }
                Executed::running(format!("{name} {source},R{r}"))
            }
        }
    }
}

#[cfg(test)]
mod tests {
    //! test_rss_ops
    use super::*;

    fn rss(opcode: u16, r: u16, source_register: u16) -> u16 {
        opcode | r << 6 | source_register
    }

    #[test]
    fn mul() {
        // p and P are small and big positives, n and N their negatives, z
        // zero, p1 and n1 one and minus one.
        let (p_, p, p1, z) = (0o764i32, 0o265i32, 1i32, 0i32); // 500, 181, 1, 0
        let (n_, n, n1) = (-p_, -p, -p1);
        // C is set when the product does not fit in 16 bits, which is every
        // nonzero case with a 500 factor (Python: C always 0).
        let cases = [
            (p, p, "0000"),
            (p, z, "0100"),
            (p, n, "1000"),
            (z, p, "0100"),
            (z, z, "0100"),
            (z, n, "0100"),
            (p1, p1, "0000"),
            (p1, n1, "1000"),
            (n1, p1, "1000"),
            (n1, n1, "0000"),
            (n, p, "1000"),
            (n, z, "0100"),
            (n, n, "0000"),
            (p, p_, "0001"),
            (p, n_, "1001"),
            (z, p_, "0100"),
            (z, n_, "0100"),
            (n, p_, "1001"),
            (n, n_, "0001"),
            (p_, p, "0001"),
            (p_, z, "0100"),
            (p_, n, "1001"),
            (n_, p, "1001"),
            (n_, z, "0100"),
            (n_, n, "0001"),
            (p_, p_, "0001"),
            (p_, n_, "1001"),
            (n_, p_, "1001"),
            (n_, n_, "0001"),
        ];
        for (s, d, nzvc) in cases {
            let mut m = Machine::new();
            m.reg.set(4, d as u16);
            m.reg.set(2, s as u16);
            let instruction = rss(0o070000, 4, 2);
            assert!(is_rss_op(instruction));
            let e = m.do_rss_op(instruction);
            assert_eq!(e.assembly, "MUL R2,R4");
            let product = u32::from(m.reg.get(4)) << 16 | u32::from(m.reg.get(5));
            assert_eq!(product as i32, s * d, "{s} * {d}");
            assert_eq!(m.psw.nzvc(), nzvc, "{s} * {d}");
        }
    }

    #[test]
    fn div() {
        // The Python test expected 486 for -9044002 / -18622; the quotient
        // is 485.
        let (big, small) = (0o42400042i32, 0o44276i32); // 9044002, 18622
        let cases = [
            (big, small, "0000"),
            (big, -small, "1000"),
            (-big, small, "1000"),
            (-big, -small, "0000"),
        ];
        for (n, d, nzvc) in cases {
            let mut m = Machine::new();
            m.reg.set(4, (n >> 16) as u16);
            m.reg.set(5, n as u16);
            m.reg.set(2, d as u16);
            assert!(is_rss_op(rss(0o071000, 4, 2)));
            m.do_rss_op(rss(0o071000, 4, 2));
            assert_eq!(m.reg.get(4) as i16 as i32, n / d, "{n} / {d}");
            assert_eq!(m.reg.get(5) as i16 as i32, n % d, "{n} % {d}");
            assert_eq!(m.psw.nzvc(), nzvc);
        }
    }

    #[test]
    fn div_errors() {
        let mut m = Machine::new();
        m.reg.set(0, 0o1234);
        m.reg.set(1, 0o5670);
        m.do_rss_op(rss(0o071000, 0, 2)); // R2 = 0
        assert_eq!(m.psw.nzvc(), "0111");
        assert_eq!((m.reg.get(0), m.reg.get(1)), (0o1234, 0o5670));
        m.reg.set(2, 1);
        m.do_rss_op(rss(0o071000, 0, 2)); // quotient does not fit
        assert_eq!(m.psw.nzvc(), "0010");
        assert_eq!((m.reg.get(0), m.reg.get(1)), (0o1234, 0o5670));
        m.reg.set(0, 0o100000);
        m.reg.set(1, 0);
        m.reg.set(2, 0o177777);
        m.do_rss_op(rss(0o071000, 0, 2)); // -2^31 / -1
        assert_eq!(m.psw.nzvc(), "0010");
    }

    #[test]
    fn shifts() {
        assert_eq!(arith_shift(0o1, 16, 3), (0o10, false, false));
        assert_eq!(arith_shift(0o100000, 16, 0o77), (0o140000, false, false)); // right 1, sign kept
        assert_eq!(arith_shift(0o40000, 16, 1), (0o100000, true, false));
        assert_eq!(arith_shift(0o3, 16, 0o77), (0o1, false, true));
        assert_eq!(arith_shift(0x8000_0000, 32, 1), (0, true, true));
        let mut m = Machine::new();
        m.reg.set(0, 0o1);
        m.reg.set(1, 0o100000);
        m.reg.set(2, 1);
        m.do_rss_op(rss(0o073000, 0, 2)); // ASHC R2,R0: left 1
        assert_eq!((m.reg.get(0), m.reg.get(1)), (0o3, 0o0));
        assert_eq!(m.psw.nzvc(), "0000");
        m.reg.set(2, 0o77); // right 1
        m.do_rss_op(rss(0o072000, 0, 2)); // ASH R2,R0
        assert_eq!(m.reg.get(0), 0o1);
        assert_eq!(
            m.psw.nzvc(),
            "0001",
            "C is the bit shifted out; no sign change"
        );
    }

    #[test]
    fn sob() {
        let mut m = Machine::new();
        m.reg.set(3, 2);
        m.reg.set_pc(0o1010);
        let e = m.do_rss_op(rss(0o077000, 3, 3));
        assert_eq!(e.assembly, "SOB R3,0o3");
        assert_eq!((m.reg.get(3), m.reg.pc()), (1, 0o1002));
        m.reg.set_pc(0o1010);
        m.do_rss_op(rss(0o077000, 3, 3));
        assert_eq!((m.reg.get(3), m.reg.pc()), (0, 0o1010));
    }

    #[test]
    fn xor_reads_register_first() {
        let mut m = Machine::new();
        m.reg.set(1, 0o1000);
        m.write_word(0o1000, 0o1777);
        m.psw.set(0o1);
        let e = m.do_rss_op(rss(0o074000, 1, 0o21)); // XOR R1,(R1)+
        assert_eq!(e.assembly, "XOR R1,(R1)+");
        assert_eq!(m.read_word(0o1000), 0o777);
        assert_eq!(m.reg.get(1), 0o1002);
        assert_eq!(m.psw.nzvc(), "0001", "C unaffected");
    }
}
