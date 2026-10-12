//! Configuration (pdp11_config): a JSON file whose top level maps device
//! names to objects of parameters, such as
//!
//! ```json
//! { "config": { "name": "default" }, "ram": { "bits": 16 } }
//! ```

use std::fs;
use std::io;
use std::path::Path;

use serde_json::Value;

/// The file read when no other is named.
pub const DEFAULT_FILE: &str = "config_default.json";

#[derive(Debug, Clone)]
pub struct Config {
    values: Value,
}

impl Config {
    /// Reads a configuration file.  A file that is not valid JSON is logged
    /// and treated as empty, as the Python did.
    pub fn load(path: impl AsRef<Path>) -> io::Result<Config> {
        let path = path.as_ref();
        log::info!("configuration file: {}", path.display());
        let text = fs::read_to_string(path)?;
        let values = serde_json::from_str(&text).unwrap_or_else(|e| {
            log::error!("There was an error while reading {}: {e}", path.display());
            Value::Null
        });
        Ok(Config { values })
    }

    /// A parameter's value, or `None` if the device or parameter is missing
    /// (the Python returned the string "ERROR").
    pub fn lookup(&self, device: &str, parameter: &str) -> Option<&Value> {
        let value = self.values.get(device).and_then(|d| d.get(parameter));
        if value.is_none() {
            log::error!("could not find value for {device}:{parameter}");
        }
        value
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::repo_root;

    fn default() -> Config {
        Config::load(repo_root().join(DEFAULT_FILE)).unwrap()
    }

    #[test]
    fn default_name() {
        assert_eq!(
            default().lookup("config", "name").and_then(Value::as_str),
            Some("default")
        );
    }

    #[test]
    fn ram_bits() {
        assert_eq!(
            default().lookup("ram", "bits").and_then(Value::as_u64),
            Some(16)
        );
    }

    #[test]
    fn missing_values() {
        let config = default();
        assert_eq!(config.lookup("config", "gook"), None);
        assert_eq!(config.lookup("gobbledy", "gook"), None);
    }
}
