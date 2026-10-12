//! The Rust side of the comparison with SIMH: the counterpart of
//! `c/tests/differential/simh_ops.c`, with the same input and output, so
//! `c/tests/differential/simh_check.py` drives it unchanged.
//!
//! Reads one case per line: R0-R7 and the PSW in decimal, then octal
//! (address value) pairs to deposit.  Runs one instruction on a machine
//! with no devices and prints R0-R7, the PSW, whether the I/O page was
//! touched, and the words at 0o400-0o476 and 0o1000-0o1176, in octal.  A
//! panic is reported as CRASH.

use std::io::{self, BufRead, BufWriter, Write};
use std::panic::{self, AssertUnwindSafe};

use pdp11::Machine;

const REGIONS: [(u16, u16); 2] = [(0o400, 0o500), (0o1000, 0o1200)];

fn run_case(line: &str) -> Result<String, String> {
    let fields: Vec<&str> = line.split_whitespace().collect();
    let number = |i: usize, radix: u32| -> Result<u16, String> {
        let field = fields.get(i).ok_or_else(|| format!("field {i} missing"))?;
        u16::from_str_radix(field, radix).map_err(|e| format!("field {i} {field:?}: {e}"))
    };
    let mut m = Machine::new();
    m.ram.clear(); // the SIMH script clears memory; Ram::new fills the vectors
    for i in (9..fields.len().saturating_sub(1)).step_by(2) {
        let (address, value) = (number(i, 8)?, number(i + 1, 8)?);
        m.write_word(address, value);
    }
    for r in 0..8 {
        m.reg.set(r, number(r, 10)?);
    }
    m.psw.set(number(8, 10)?);
    m.io_touched = false;

    let instruction = m.fetch();
    m.execute(instruction);

    let mut out: Vec<String> = (0..8).map(|r| format!("{:06o}", m.reg.get(r))).collect();
    out.push(format!("{:06o}", m.psw.get()));
    out.push(u8::from(m.io_touched).to_string());
    for (start, end) in REGIONS {
        for a in (start..end).step_by(2) {
            out.push(format!("{:06o}", m.read_word(a)));
        }
    }
    Ok(out.join(" "))
}

fn main() -> io::Result<()> {
    panic::set_hook(Box::new(|_| {})); // reported as CRASH below
    let stdout = io::stdout();
    let mut out = BufWriter::new(stdout.lock());
    for line in io::stdin().lock().lines() {
        let line = line?;
        match panic::catch_unwind(AssertUnwindSafe(|| run_case(&line))) {
            Ok(Ok(result)) => writeln!(out, "{result}")?,
            Ok(Err(e)) => writeln!(out, "CRASH {e}")?,
            Err(_) => writeln!(out, "CRASH panic")?,
        }
    }
    out.flush()
}
