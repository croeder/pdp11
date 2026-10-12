/* pdp11_config.h - translation of pdp11_config.py
 *
 * Reads a JSON configuration file (default config_default.json) whose top
 * level maps device names to objects of parameters.  lookup() returns the
 * parameter's value with its JSON type, or the string "ERROR" if missing. */
#ifndef PDP11_CONFIG_H
#define PDP11_CONFIG_H

#include "pyint.h"

typedef enum { CONFIG_ERROR, CONFIG_STRING, CONFIG_NUMBER, CONFIG_BOOL, CONFIG_NULL, CONFIG_OTHER } ConfigKind;

typedef struct {
    ConfigKind kind;
    char string[128];   /* for CONFIG_STRING; "ERROR" for CONFIG_ERROR */
    double number;      /* for CONFIG_NUMBER and CONFIG_BOOL */
} ConfigValue;

typedef struct {
    char device[64];
    char parameter[64];
    ConfigValue value;
} ConfigEntry;

typedef struct {
    ConfigEntry entries[128];
    int count;
} Config;

void Config_init(Config *self, const char *filename);   /* NULL -> config_default.json */
ConfigValue Config_lookup(const Config *self, const char *device, const char *parameter);

/* int(value), as Python's int() would apply to the looked-up value */
pyint ConfigValue_int(const ConfigValue *value);

#endif
