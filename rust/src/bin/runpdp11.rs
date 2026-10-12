//! Powers on the PDP-11 (M9301-YA boot ROM, console emulator) and runs it on
//! this terminal.
//!
//! Usage: `runpdp11 [-l listing] [instruction-limit]`
//!
//! - `-l listing`: also load a program ("address word" lines in octal, the
//!   format of `source/M9301-YA.txt`) and start at its first address.
//! - `instruction-limit`: default no limit; Ctrl-C stops.
//!
//! Run it from the directory that holds `config_default.json` and `source/`.

use std::io;
use std::process::ExitCode;

use pdp11::terminal::RawMode;
use pdp11::{logger, Machine};

const USAGE: &str = "usage: runpdp11 [-l listing] [instruction-limit]";

fn main() -> ExitCode {
    let mut args: Vec<String> = std::env::args().skip(1).collect();
    let listing = if args.first().map(String::as_str) == Some("-l") && args.len() >= 2 {
        let path = args.remove(1);
        args.remove(0);
        Some(path)
    } else {
        None
    };
    let limit = match args.as_slice() {
        [] => None,
        [n] => match n.parse::<u64>() {
            Ok(n) => Some(n),
            Err(_) => {
                eprintln!("{USAGE}");
                return ExitCode::from(2);
            }
        },
        _ => {
            eprintln!("{USAGE}");
            return ExitCode::from(2);
        }
    };
    // Without a log file the emulator runs without logging.
    let _ = logger::init("pdp11.log");
    let mut machine = match Machine::boot(".") {
        Ok(m) => m,
        Err(e) => {
            eprintln!(
                "runpdp11: {e} (run from the directory with config_default.json and source/)"
            );
            return ExitCode::FAILURE;
        }
    };
    if let Some(path) = listing {
        match machine.load_listing(&path) {
            Ok(base) => machine.reg.set_pc(base),
            Err(e) => {
                eprintln!("runpdp11: {path}: {e}");
                return ExitCode::FAILURE;
            }
        }
    }
    let _raw = RawMode::enable();
    machine.terminal.keys_from(io::stdin());
    machine.run(limit);
    ExitCode::SUCCESS
}
