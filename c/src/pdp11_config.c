/* pdp11_config.c - translation of pdp11_config.py
 * Includes a small JSON reader, since C has no json module. */
#include "pdp11_config.h"
#include "pdp11_logger.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *p;
    int ok;
} Parser;

static void skip_ws(Parser *ps)
{
    while (*ps->p && isspace((unsigned char)*ps->p)) ps->p++;
}

static int parse_string(Parser *ps, char *out, size_t cap)
{
    skip_ws(ps);
    if (*ps->p != '"') return ps->ok = 0;
    ps->p++;
    size_t n = 0;
    while (*ps->p && *ps->p != '"') {
        char c = *ps->p++;
        if (c == '\\') {
            char e = *ps->p++;
            switch (e) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'u': c = '?'; ps->p += 4; break;  /* non-ASCII not needed here */
            default:  c = e; break;
            }
        }
        if (n + 1 < cap) out[n++] = c;
    }
    if (*ps->p != '"') return ps->ok = 0;
    ps->p++;
    out[n] = '\0';
    return 1;
}

static int parse_value(Parser *ps, ConfigValue *v, Config *cfg, const char *device, int depth);

static int parse_object(Parser *ps, Config *cfg, const char *device, int depth)
{
    skip_ws(ps);
    if (*ps->p != '{') return ps->ok = 0;
    ps->p++;
    skip_ws(ps);
    if (*ps->p == '}') { ps->p++; return 1; }
    for (;;) {
        char key[64];
        if (!parse_string(ps, key, sizeof key)) return 0;
        skip_ws(ps);
        if (*ps->p != ':') return ps->ok = 0;
        ps->p++;
        ConfigValue v;
        if (depth == 0) {
            /* top level: device name -> object of parameters */
            if (!parse_value(ps, &v, cfg, key, depth + 1)) return 0;
        } else {
            if (!parse_value(ps, &v, cfg, NULL, depth + 1)) return 0;
            if (depth == 1 && cfg->count < (int)(sizeof cfg->entries / sizeof cfg->entries[0])) {
                ConfigEntry *e = &cfg->entries[cfg->count++];
                snprintf(e->device, sizeof e->device, "%s", device);
                snprintf(e->parameter, sizeof e->parameter, "%s", key);
                e->value = v;
            }
        }
        skip_ws(ps);
        if (*ps->p == ',') { ps->p++; continue; }
        if (*ps->p == '}') { ps->p++; return 1; }
        return ps->ok = 0;
    }
}

static int parse_array(Parser *ps, int depth)
{
    ps->p++;  /* '[' */
    skip_ws(ps);
    if (*ps->p == ']') { ps->p++; return 1; }
    for (;;) {
        ConfigValue ignored;
        if (!parse_value(ps, &ignored, NULL, NULL, depth + 1)) return 0;
        skip_ws(ps);
        if (*ps->p == ',') { ps->p++; continue; }
        if (*ps->p == ']') { ps->p++; return 1; }
        return ps->ok = 0;
    }
}

static int parse_value(Parser *ps, ConfigValue *v, Config *cfg, const char *device, int depth)
{
    memset(v, 0, sizeof *v);
    skip_ws(ps);
    char c = *ps->p;
    if (c == '{') {
        v->kind = CONFIG_OTHER;
        if (cfg && device) return parse_object(ps, cfg, device, depth);
        Config scratch;
        scratch.count = 0;
        return parse_object(ps, &scratch, "", depth + 2);  /* deeper objects are not stored */
    }
    if (c == '[') { v->kind = CONFIG_OTHER; return parse_array(ps, depth); }
    if (c == '"') { v->kind = CONFIG_STRING; return parse_string(ps, v->string, sizeof v->string); }
    if (!strncmp(ps->p, "true", 4))  { ps->p += 4; v->kind = CONFIG_BOOL; v->number = 1; return 1; }
    if (!strncmp(ps->p, "false", 5)) { ps->p += 5; v->kind = CONFIG_BOOL; v->number = 0; return 1; }
    if (!strncmp(ps->p, "null", 4))  { ps->p += 4; v->kind = CONFIG_NULL; return 1; }
    char *end;
    errno = 0;
    double d = strtod(ps->p, &end);
    if (end == ps->p) return ps->ok = 0;
    ps->p = end;
    v->kind = CONFIG_NUMBER;
    v->number = d;
    return 1;
}

void Config_init(Config *self, const char *filename)
{
    if (!filename) filename = "config_default.json";
    Logger();
    logging_info("configuration file: %s", filename);
    self->count = 0;

    FILE *f = fopen(filename, "r");
    if (!f) PY_RAISE("FileNotFoundError", filename);  /* open() raises; Python does not catch it */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    size_t got = fread(text, 1, (size_t)size, f);
    text[got] = '\0';
    fclose(f);

    Parser ps = {text, 1};
    if (!parse_object(&ps, self, NULL, 0) || !ps.ok) {
        /* Python logs with an undefined name here (configFilePath) and so
         * raises NameError; the C version logs the file name instead. */
        logging_error("There was an error while reading %s", filename);
        self->count = 0;
    }
    free(text);
}

ConfigValue Config_lookup(const Config *self, const char *device, const char *parameter)
{
    for (int i = 0; i < self->count; i++) {
        if (!strcmp(self->entries[i].device, device) && !strcmp(self->entries[i].parameter, parameter))
            return self->entries[i].value;
    }
    ConfigValue v;
    memset(&v, 0, sizeof v);
    v.kind = CONFIG_ERROR;
    strcpy(v.string, "ERROR");
    logging_error("could not find value for %s:%s", device, parameter);
    return v;
}

pyint ConfigValue_int(const ConfigValue *value)
{
    switch (value->kind) {
    case CONFIG_NUMBER:
    case CONFIG_BOOL:
        return (pyint)value->number;
    case CONFIG_STRING:
    case CONFIG_ERROR: {
        char *end;
        long long n = strtoll(value->string, &end, 10);
        if (end == value->string || *end != '\0') PY_RAISE("ValueError", "invalid literal for int()");
        return n;
    }
    default:
        PY_RAISE("TypeError", "int() argument must be a string or a number");
    }
}
