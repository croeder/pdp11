//! The console terminal on stdin and stdout (pdp11_terminal, without the
//! VT52 window): characters the PDP-11 transmits are written out, and keys
//! read on another thread are delivered to the DL11 receiver.

use std::io::{self, BufReader, Read, Write};
use std::sync::mpsc::{self, Receiver, TryRecvError};
use std::sync::OnceLock;
use std::thread;

use crate::dl11::Dl11;
use crate::util::safe_character;

enum Output {
    Stdout(io::Stdout),
    Captured(Vec<u8>),
}

pub struct Terminal {
    out: Output,
    keys: Option<Receiver<u8>>,
}

impl Default for Terminal {
    fn default() -> Terminal {
        Terminal::new()
    }
}

impl Terminal {
    /// Writes to stdout, with no keyboard until [`keys_from`](Terminal::keys_from).
    pub fn new() -> Terminal {
        Terminal {
            out: Output::Stdout(io::stdout()),
            keys: None,
        }
    }

    /// Keeps the output for [`captured`](Terminal::captured) instead of
    /// writing it.
    pub fn capture(&mut self) {
        self.out = Output::Captured(Vec::new());
    }

    /// The output kept since [`capture`](Terminal::capture).
    pub fn captured(&self) -> &[u8] {
        match &self.out {
            Output::Captured(v) => v,
            Output::Stdout(_) => &[],
        }
    }

    /// Delivers keys read from `input` (stdin, normally) on a thread of its
    /// own, until it ends.
    pub fn keys_from(&mut self, input: impl Read + Send + 'static) {
        let (tx, rx) = mpsc::sync_channel(256);
        thread::spawn(move || {
            // A read returns whatever is available, so buffering does not
            // hold keys back.
            for byte in BufReader::new(input).bytes() {
                match byte {
                    Ok(b) if tx.send(b).is_ok() => {}
                    _ => break,
                }
            }
        });
        self.keys = Some(rx);
    }

    /// Passes characters once: the one in the transmit buffer, if any, goes
    /// out, and a waiting key goes in if the receiver is empty.
    pub fn cycle(&mut self, dl11: &mut Dl11) {
        if dl11.transmitting() {
            let c = dl11.take_transmitted();
            if c != 0 {
                // The DL11 can send nulls; they are not shown.
                log::info!("dl11 transmitted:{}", safe_character(c));
                self.write(c as u8);
            }
        }
        if let Some(keys) = &self.keys {
            if !dl11.receiver_full() {
                match keys.try_recv() {
                    Ok(key) => dl11.receive(key.into()),
                    Err(TryRecvError::Empty) => {}
                    Err(TryRecvError::Disconnected) => self.keys = None,
                }
            }
        }
    }

    fn write(&mut self, c: u8) {
        match &mut self.out {
            Output::Stdout(out) => {
                // The ROM's prompt has no newline, so every character is
                // flushed.  A closed stdout is not the emulator's concern.
                let _ = out.write_all(&[c]).and_then(|()| out.flush());
            }
            Output::Captured(v) => v.push(c),
        }
    }
}

/// The terminal settings to restore, for the signal handler.
static SAVED: OnceLock<libc::termios> = OnceLock::new();

/// Character-at-a-time input without echo on stdin, restored when dropped
/// or when the process is stopped by Ctrl-C or SIGTERM.  Return arrives as
/// CR, which the ROM expects.
pub struct RawMode {
    saved: libc::termios,
}

impl RawMode {
    /// Changes the terminal; `None` if stdin is not a terminal.
    pub fn enable() -> Option<RawMode> {
        let fd = libc::STDIN_FILENO;
        // SAFETY: tcgetattr fills the termios it is given, which is plain data.
        let mut saved: libc::termios = unsafe { std::mem::zeroed() };
        if unsafe { libc::tcgetattr(fd, &mut saved) } != 0 {
            return None;
        }
        let mut raw = saved;
        raw.c_lflag &= !(libc::ICANON | libc::ECHO);
        raw.c_iflag &= !libc::ICRNL;
        raw.c_cc[libc::VMIN] = 1;
        raw.c_cc[libc::VTIME] = 0;
        let _ = SAVED.set(saved);
        // SAFETY: the handler only calls async-signal-safe functions.
        let handler: extern "C" fn(libc::c_int) = restore_and_exit;
        unsafe {
            libc::signal(libc::SIGINT, handler as libc::sighandler_t);
            libc::signal(libc::SIGTERM, handler as libc::sighandler_t);
        }
        // SAFETY: raw is a valid termios from tcgetattr.
        if unsafe { libc::tcsetattr(fd, libc::TCSANOW, &raw) } != 0 {
            return None;
        }
        Some(RawMode { saved })
    }
}

impl Drop for RawMode {
    fn drop(&mut self) {
        // SAFETY: saved came from tcgetattr.
        unsafe {
            libc::tcsetattr(libc::STDIN_FILENO, libc::TCSANOW, &self.saved);
        }
    }
}

extern "C" fn restore_and_exit(_signal: libc::c_int) {
    if let Some(saved) = SAVED.get() {
        // SAFETY: tcsetattr and _exit are async-signal-safe.
        unsafe {
            libc::tcsetattr(libc::STDIN_FILENO, libc::TCSANOW, saved);
        }
    }
    unsafe { libc::_exit(130) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::dl11::{Dl11, CONSOLE};

    #[test]
    fn output_and_input() {
        let mut t = Terminal::new();
        t.capture();
        t.keys_from(&b"ab"[..]);
        let mut d = Dl11::new(CONSOLE);
        d.write(d.xbuf_address(), u16::from(b'H'));
        // Keys arrive from another thread; wait for the first.
        while !d.receiver_full() {
            t.cycle(&mut d);
        }
        assert_eq!(t.captured(), b"H");
        assert!(!d.transmitting());
        assert_eq!(d.read(d.rbuf_address()), u16::from(b'a'));
        while !d.receiver_full() {
            t.cycle(&mut d);
        }
        assert_eq!(d.read(d.rbuf_address()), u16::from(b'b'));
    }

    #[test]
    fn nulls_are_not_shown() {
        let mut t = Terminal::new();
        t.capture();
        let mut d = Dl11::new(CONSOLE);
        d.write(d.xbuf_address(), 0);
        t.cycle(&mut d);
        assert!(t.captured().is_empty());
        assert!(!d.transmitting());
    }
}
