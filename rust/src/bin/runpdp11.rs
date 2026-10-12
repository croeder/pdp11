//! Powers on the PDP-11 (M9301-YA boot ROM, console emulator) and runs it on
//! this terminal.
//!
//! Usage: `runpdp11 [instruction-limit]` (default: no limit; Ctrl-C stops).
//! Run it from the directory that holds `config_default.json` and `source/`.

use std::io;
use std::process::ExitCode;

use pdp11::terminal::RawMode;
use pdp11::{logger, Machine};

fn main() -> ExitCode {
    let limit = match std::env::args().nth(1).map(|a| a.parse::<u64>()) {
        None => None,
        Some(Ok(n)) => Some(n),
        Some(Err(_)) => {
            eprintln!("usage: runpdp11 [instruction-limit]");
            return ExitCode::from(2);
        }
    };
    if let Err(e) = logger::init("pdp11.log") {
        eprintln!("runpdp11: pdp11.log: {e} (not logging)");
    }
    let mut machine = match Machine::boot(".") {
        Ok(m) => m,
        Err(e) => {
            eprintln!(
                "runpdp11: {e} (run from the directory with config_default.json and source/)"
            );
            return ExitCode::FAILURE;
        }
    };
    let _raw = RawMode::enable();
    machine.terminal.keys_from(io::stdin());
    machine.run(limit);
    ExitCode::SUCCESS
}
