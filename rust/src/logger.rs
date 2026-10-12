//! The log file (pdp11_logger): `pdp11.log`, truncated when opened, at INFO
//! level, one line per record:
//!
//! ```text
//! 2026-10-12T17:04:05.123000Z machine.rs:212 INFO : run: begin PDP11 emulator
//! ```
//!
//! The emulator logs through the `log` macros; until [`init`] is called
//! (as in the tests) nothing is written.

use std::fs::File;
use std::io::{self, LineWriter, Write};
use std::path::Path;
use std::sync::Mutex;
use std::time::{SystemTime, UNIX_EPOCH};

use log::{Level, Log, Metadata, Record};

struct FileLogger {
    file: Mutex<LineWriter<File>>,
    level: Level,
}

impl Log for FileLogger {
    fn enabled(&self, metadata: &Metadata) -> bool {
        metadata.level() <= self.level
    }

    fn log(&self, record: &Record) {
        if !self.enabled(record.metadata()) {
            return;
        }
        let file = record
            .file()
            .and_then(|f| Path::new(f).file_name())
            .and_then(|f| f.to_str())
            .unwrap_or("?");
        let line = format!(
            "{} {}:{} {} : {}\n",
            timestamp(SystemTime::now()),
            file,
            record.line().unwrap_or(0),
            record.level(),
            record.args()
        );
        if let Ok(mut f) = self.file.lock() {
            let _ = f.write_all(line.as_bytes());
        }
    }

    fn flush(&self) {
        if let Ok(mut f) = self.file.lock() {
            let _ = f.flush();
        }
    }
}

/// Opens (truncating) the log file and sends INFO and above to it.  Only the
/// first call in a process takes effect.
pub fn init(path: impl AsRef<Path>) -> io::Result<()> {
    init_at(path.as_ref(), Level::Info)
}

/// As [`init`], at the given level (`Level::Debug` logs every instruction).
pub fn init_at(path: &Path, level: Level) -> io::Result<()> {
    let file = File::create(path)?;
    let logger = FileLogger {
        file: Mutex::new(LineWriter::new(file)),
        level,
    };
    if log::set_boxed_logger(Box::new(logger)).is_ok() {
        log::set_max_level(level.to_level_filter());
        log::info!("{} begins", path.display());
    }
    Ok(())
}

/// ISO 8601 UTC with microseconds, as Python's logging was configured to write.
fn timestamp(t: SystemTime) -> String {
    let d = t.duration_since(UNIX_EPOCH).unwrap_or_default();
    let secs = d.as_secs();
    let (year, month, day) = civil_from_days((secs / 86_400) as i64);
    let s = secs % 86_400;
    format!(
        "{year:04}-{month:02}-{day:02}T{:02}:{:02}:{:02}.{:06}Z",
        s / 3600,
        s / 60 % 60,
        s % 60,
        d.subsec_micros()
    )
}

/// Days since 1970-01-01 to (year, month, day), from Howard Hinnant's
/// civil_from_days.
fn civil_from_days(days: i64) -> (i64, u32, u32) {
    let z = days + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z.rem_euclid(146_097);
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let day = (doy - (153 * mp + 2) / 5 + 1) as u32;
    let month = if mp < 10 { mp + 3 } else { mp - 9 } as u32;
    let year = yoe + era * 400 + i64::from(month <= 2);
    (year, month, day)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::Duration;

    #[test]
    fn timestamps() {
        assert_eq!(timestamp(UNIX_EPOCH), "1970-01-01T00:00:00.000000Z");
        let t = UNIX_EPOCH + Duration::from_micros(1_791_824_645_123_456);
        assert_eq!(timestamp(t), "2026-10-12T17:04:05.123456Z");
        assert_eq!(civil_from_days(-1), (1969, 12, 31));
        assert_eq!(civil_from_days(11_016), (2000, 2, 29));
    }
}
