/* pdp11_util.h - translation of pdp11_util.py */
#ifndef PDP11_UTIL_H
#define PDP11_UTIL_H

#include "pyint.h"

#define MASK_LONG     037777777777LL /* 32 bits */
#define MASK_LONG_MSB 020000000000LL
#define MASK_WORD     0177777LL      /* 16 bits */
#define MASK_WORD_MSB 0100000LL
#define MASK_BYTE     0000377LL      /* 8 bits */
#define MASK_BYTE_MSB 0000200LL
#define MASK_LOW_BYTE  0000377LL
#define MASK_HIGH_BYTE 0177400LL

/* Formatting helpers.  Each writes into a caller-supplied buffer of at least
 * PYSTR bytes and returns it, so they can be used inline in printf calls. */
#define PYSTR 96

/* Python's oct(): 0o27, -0o5, 0o0 */
char *py_oct(char *buf, pyint value);
/* Python's bin(): 0b101, -0b1 */
char *py_bin(char *buf, pyint value);

char *oct3(char *buf, pyint word);
char *oct6(char *buf, pyint word);
char *pad(char *buf, const char *string, pyint width);

pyint twosComplementNegative(pyint source);
pyint twosComplementNegativeLong(pyint source);
pyint pythonifyPDP11Byte(pyint source);
pyint pythonifyPDP11Word(pyint source);
pyint pythonifyPDP11Long(pyint source);
pyint PDP11WordifyPythonInteger(pyint source);
pyint PDP11LongifyPythonInteger(pyint source);
pyint extendSign(pyint source);

/* safe_character: control-character name, or the character itself. */
char *safe_character(char *buf, pyint byte);

#endif
