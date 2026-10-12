//! Timing of code sections by name (stopwatches), reported to the log.

use std::time::{Duration, Instant};

#[derive(Debug, Clone)]
pub struct StopWatch {
    pub id: &'static str,
    pub count: u64,
    pub min: Duration,
    pub max: Duration,
    pub sum: Duration,
    started: Instant,
}

impl StopWatch {
    fn new(id: &'static str) -> StopWatch {
        StopWatch {
            id,
            count: 0,
            min: Duration::MAX,
            max: Duration::ZERO,
            sum: Duration::ZERO,
            started: Instant::now(),
        }
    }

    pub fn mean(&self) -> Duration {
        if self.count == 0 {
            Duration::ZERO
        } else {
            self.sum / self.count as u32
        }
    }
}

/// Watches kept in first-started order.  There are a handful, so a vector
/// searched by name serves.
#[derive(Debug, Default)]
pub struct StopWatches {
    watches: Vec<StopWatch>,
}

impl StopWatches {
    pub fn new() -> StopWatches {
        StopWatches::default()
    }

    pub fn start(&mut self, id: &'static str) {
        let now = Instant::now();
        match self.watches.iter_mut().find(|w| w.id == id) {
            Some(w) => w.started = now,
            None => {
                let mut w = StopWatch::new(id);
                w.started = now;
                self.watches.push(w);
            }
        }
    }

    pub fn stop(&mut self, id: &'static str) {
        let now = Instant::now();
        match self.watches.iter_mut().find(|w| w.id == id) {
            Some(w) => {
                let d = now - w.started;
                w.min = w.min.min(d);
                w.max = w.max.max(d);
                w.sum += d;
                w.count += 1;
            }
            None => log::info!("WARN: StopWatches stop could not find id {id}"),
        }
    }

    pub fn watch(&self, id: &str) -> Option<&StopWatch> {
        self.watches.iter().find(|w| w.id == id)
    }

    /// Logs every watch, times in microseconds.
    pub fn report(&self) {
        log::info!("StopWatches Report (times in microseconds)");
        log::info!("id                      min        mean         max         sum       count");
        for w in &self.watches {
            let us = |d: Duration| d.as_secs_f64() * 1e6;
            log::info!(
                "{:<20}{:12.0}{:12.0}{:12.0}{:12.0}{:12}",
                w.id,
                us(w.min),
                us(w.mean()),
                us(w.max),
                us(w.sum),
                w.count
            );
        }
        log::info!("StopWatches Report end");
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn counts_and_orders() {
        let mut sw = StopWatches::new();
        sw.start("a");
        sw.stop("a");
        sw.start("b");
        sw.stop("b");
        sw.start("a");
        sw.stop("a");
        assert_eq!(sw.watch("a").unwrap().count, 2);
        assert_eq!(sw.watch("b").unwrap().count, 1);
        assert!(sw.watch("a").unwrap().min <= sw.watch("a").unwrap().max);
        assert!(sw.watch("c").is_none());
        assert_eq!(
            sw.watches.iter().map(|w| w.id).collect::<Vec<_>>(),
            ["a", "b"]
        );
    }
}
