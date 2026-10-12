/* pdp11_util.c - translation of pdp11_util.py */
#include "pdp11_util.h"
#include "pdp11_logger.h"

#include <string.h>

static char *digits_in_base(char *buf, pyint value, int bits_per_digit, const char *prefix)
{
    char tmp[80];
    int n = 0;
    uint64_t magnitude = value < 0 ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value;
    unsigned mask = (1u << bits_per_digit) - 1;
    do {
        tmp[n++] = (char)('0' + (magnitude & mask));
        magnitude >>= bits_per_digit;
    } while (magnitude);
    char *p = buf;
    if (value < 0) *p++ = '-';
    strcpy(p, prefix);
    p += strlen(prefix);
    while (n) *p++ = tmp[--n];
    *p = '\0';
    return buf;
}

char *py_oct(char *buf, pyint value) { return digits_in_base(buf, value, 3, "0o"); }
char *py_bin(char *buf, pyint value) { return digits_in_base(buf, value, 1, "0b"); }

/* Python s[a:b] with Python's handling of negative and out-of-range bounds */
static void py_slice(char *out, const char *s, pyint start, pyint stop)
{
    pyint len = (pyint)strlen(s);
    if (start < 0) start += len;
    if (stop < 0) stop += len;
    if (start < 0) start = 0;
    if (stop < 0) stop = 0;
    if (start > len) start = len;
    if (stop > len) stop = len;
    pyint n = stop > start ? stop - start : 0;
    memcpy(out, s + start, (size_t)n);
    out[n] = '\0';
}

char *oct3(char *buf, pyint word)
{
    /* octal = oct(word)[2:]
     * padding_zeroes = '0000'[0:3-len(octal)] */
    char full[PYSTR], octal[PYSTR], padding[8];
    py_oct(full, word);
    py_slice(octal, full, 2, (pyint)strlen(full));
    py_slice(padding, "0000", 0, 3 - (pyint)strlen(octal));
    strcpy(buf, padding);
    strcat(buf, octal);
    return buf;
}

char *oct6(char *buf, pyint word)
{
    /* octal = oct(word)
     * padding_zeroes = '000000'[0:8 - len(octal)]
     * result = f'{octal[2:2]}{padding_zeroes}{octal[2:]}' */
    char octal[PYSTR], padding[8], tail[PYSTR];
    py_oct(octal, word);
    py_slice(padding, "000000", 0, 8 - (pyint)strlen(octal));
    py_slice(tail, octal, 2, (pyint)strlen(octal));
    strcpy(buf, padding);
    strcat(buf, tail);
    return buf;
}

char *pad(char *buf, const char *string, pyint width)
{
    /* padding = '                    '[0:width - len(string)] */
    char padding[24];
    py_slice(padding, "                    ", 0, width - (pyint)strlen(string));
    strcpy(buf, string);
    strcat(buf, padding);
    return buf;
}

pyint twosComplementNegative(pyint source)
{
    /* invert bits and add 1 (then mask_word for PDP11ness) */
    pyint result = (~source + 1) & MASK_WORD;
    char a[PYSTR], b[PYSTR];
    logging_debug("twosComplementNegative(%s) returns %s", py_oct(a, source), py_oct(b, result));
    return result;
}

pyint twosComplementNegativeLong(pyint source)
{
    pyint result = (~source + 1) & MASK_LONG;
    char a[PYSTR], b[PYSTR];
    logging_debug("twosComplementNegativeLong(%s) returns %s", py_oct(a, source), py_oct(b, result));
    return result;
}

pyint pythonifyPDP11Byte(pyint source)
{
    /* If it's a PDP11 negative, extend the bits, subtract 1, invert */
    pyint result = source & MASK_BYTE;
    if ((MASK_BYTE_MSB & source) == MASK_BYTE_MSB) {
        char a[PYSTR];
        logging_debug("MASK_BYTE_MSB & source: %s", py_oct(a, MASK_BYTE_MSB & source));
        result = -~((source | ~MASK_BYTE) - 1);
    }
    char a[PYSTR];
    logging_debug("pythonifyPDP11Byte(%s) returns %lld", py_oct(a, source), (long long)result);
    return result;
}

pyint pythonifyPDP11Word(pyint source)
{
    pyint result = source;
    if ((MASK_WORD_MSB & source) == MASK_WORD_MSB)
        result = -~((source | ~MASK_WORD) - 1);
    char a[PYSTR];
    logging_debug("pythonifyPDP11Word(%s) returns %lld", py_oct(a, source), (long long)result);
    return result;
}

pyint pythonifyPDP11Long(pyint source)
{
    pyint result = source;
    if ((MASK_LONG_MSB & source) == MASK_LONG_MSB)
        result = -~((source | ~MASK_LONG) - 1);
    char a[PYSTR], b[PYSTR];
    logging_debug("pythonifyPDP11Long(%s = %lld) returns %s = %lld",
                  py_oct(a, source), (long long)source, py_oct(b, result), (long long)result);
    return result;
}

pyint PDP11WordifyPythonInteger(pyint source)
{
    pyint result;
    if (source >= 0)
        result = source & MASK_WORD;
    else
        result = twosComplementNegative(-source);
    char a[PYSTR];
    logging_debug("PDP11WordifyPythonInteger(%lld) returns %s", (long long)source, py_oct(a, result));
    return result;
}

pyint PDP11LongifyPythonInteger(pyint source)
{
    pyint result;
    if (source >= 0)
        result = source & MASK_LONG;
    else
        result = twosComplementNegativeLong(-source);
    char a[PYSTR];
    logging_debug("PDP11LongifyPythonInteger(%lld) returns %s", (long long)source, py_oct(a, result));
    return result;
}

pyint extendSign(pyint source)
{
    /* As written in Python: source & ~0 is just source. */
    pyint result = source;
    if ((MASK_WORD_MSB & source) == MASK_WORD_MSB)
        result = source & ~0LL;
    return result;
}

static const char *low_ascii[32] = {
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
    "BS", "HT", "LF", "VF", "FF", "CR", "SO", "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US"};

char *safe_character(char *buf, pyint byte)
{
    if (byte > 31) {
        if (byte < 127) { buf[0] = (char)byte; buf[1] = '\0'; }
        else snprintf(buf, PYSTR, "\\x%02llx", (long long)byte);  /* Python: chr(byte) */
    } else if (byte >= 0) {
        strcpy(buf, low_ascii[byte]);
    } else if (byte >= -32) {
        strcpy(buf, low_ascii[32 + byte]);  /* Python negative list index */
    } else {
        PY_RAISE("IndexError", "list index out of range");  /* low_ascii[negative] past -32 */
    }
    return buf;
}
