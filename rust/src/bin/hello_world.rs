//! Loads the "Hello, World!" program and runs it until it halts.
//! Run it from the directory that holds `config_default.json` and `source/`.

use std::process::ExitCode;

use pdp11::{logger, Machine};

/// From <http://www.retrocmp.com/how-tos/interfacing-to-a-pdp-1105/146-interfacing-with-a-pdp-1105-test-programs-and-qhello-worldq>
const HELLO_WORLD: [u16; 13] = [
    0o012702, 0o177564, // 2000 start:  MOV #serial+4,R2  ; R2 points to the DL11 XCSR
    0o012701, 0o002032, // 2004         MOV #string,R1    ; R1 points to the current char
    0o112100, //           2010 nxtchr: MOVB (R1)+,R0     ; load xmt char
    0o001405, //           2012         BEQ done          ; string is terminated with 0
    0o110062, 0o000002, // 2014         MOVB R0,2(R2)     ; write char to transmit buffer
    0o105712, //           2020 wait:   TSTB (R2)         ; character transmitted?
    0o100376, //           2022         BPL wait          ; no, loop
    0o000771, //           2024         BR nxtchr         ; transmit next character
    0o000000, //           2026 done:   HALT
    0o000763, //           2030         BR start
]; //                      2032 string: "Hello, World!"

fn main() -> ExitCode {
    let _ = logger::init("pdp11.log");
    let mut machine = match Machine::boot(".") {
        Ok(m) => m,
        Err(e) => {
            eprintln!(
                "hello_world: {e} (run from the directory with config_default.json and source/)"
            );
            return ExitCode::FAILURE;
        }
    };
    machine.load_machine_code(&HELLO_WORLD, 0o2000);
    let message = b"Hello, World!\nHello, World!\nHello, World!\0";
    for (address, &c) in (0o2032u16..).zip(message.iter()) {
        machine.write_byte(address, c);
    }
    machine.ram.dump(0o2000, 0o2032 + message.len() as u32);
    machine.run(Some(100_000));
    ExitCode::SUCCESS
}
