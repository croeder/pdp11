// config.go - translation of pdp11_config: reads a JSON configuration file
// whose top level maps device names to objects of parameters.
package pdp11

import (
	"encoding/json"
	"fmt"
	"os"
	"strconv"
)

type Config struct {
	automationConfig map[string]map[string]any
}

// NewConfig reads filename, or config_default.json when filename is "".
func NewConfig(filename string) *Config {
	if filename == "" {
		filename = "config_default.json"
	}
	Logger()
	logInfof("configuration file: %s", filename)
	c := &Config{automationConfig: map[string]map[string]any{}}
	data, err := os.ReadFile(filename)
	if err != nil {
		raise("FileNotFoundError", filename) // open() raises in the Python
	}
	if err := json.Unmarshal(data, &c.automationConfig); err != nil {
		logErrorf("There was an error while reading %s", filename)
	}
	return c
}

// Lookup returns the parameter's value (string, float64, bool, nil), or the
// string "ERROR" if the device or parameter is missing.
func (c *Config) Lookup(device, parameter string) any {
	if params, ok := c.automationConfig[device]; ok {
		if value, ok := params[parameter]; ok {
			return value
		}
	}
	logErrorf("could not find value for %s:%s", device, parameter)
	return "ERROR"
}

// configInt applies Python's int() to a looked-up value.
func configInt(value any) int {
	switch v := value.(type) {
	case float64:
		return int(v)
	case bool:
		if v {
			return 1
		}
		return 0
	case string:
		n, err := strconv.Atoi(v)
		if err != nil {
			raise("ValueError", fmt.Sprintf("invalid literal for int(): %q", v))
		}
		return n
	}
	raise("TypeError", "int() argument must be a string or a number")
	return 0
}
