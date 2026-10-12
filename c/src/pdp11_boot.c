/* pdp11_boot.c - translation of pdp11_boot.py (PDP11 bootstrap utilities) */
#include "pdp11_boot.h"
#include "pdp11_logger.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* from pdp-11/40 book */
const pyint pdp11Boot_bootstrap_loader[] = {
    0016701,  /* MOV 0o67:0o0 0o1:0o0 */
    0000240,  /* 0o000026 */
    0012702,  /* MOV 0o27:0o0 0o2:0o0 */
    0000240,  /* 0o000352 */
    0005211,  /* INC 0o0 0o0 incomplete */
    0105711,  /* CLR 0o0 0o1 */
    0100376,  /* BPL 0o376 */
    0116162,  /* MOVB 0o61:0o377 0o62:0o377 */
    0000240,  /* 0o000002 RTI */
    0000240,  /* BR 0o0 */
    0005267,  /* INC 0o770 0o5267 */
    0177756,
    0000765,  /* BR 0o365 */
    0177560,  /* 0o177560 */
    0000000};
const int pdp11Boot_bootstrap_loader_len = sizeof pdp11Boot_bootstrap_loader / sizeof pdp11Boot_bootstrap_loader[0];
/* NOP 0o000240;  0o000400 BR 00 - what's at 00 now? */
const pyint pdp11Boot_bootaddress = 0000744;

const pyint pdp11Boot_echo[] = {
    0012700, 0177560,           /* start: mov #kbs, r0 */
    0105710,                    /* wait: tstb (r0)       ; character received? */
    0100376,                    /* bpl wait        ; no, loop */
    0016060, 0000002, 0000006,  /* mov 2(r0),6(r0) ; transmit data */
    0000772};                   /* br wait         ; get next character */
const int pdp11Boot_echo_len = sizeof pdp11Boot_echo / sizeof pdp11Boot_echo[0];
const pyint pdp11Boot_echo_address = 0001000;

void pdp11Boot_init(pdp11Boot *self, Registers *reg, Ram *ram)
{
    logging_info("initializing pdp11Boot");
    self->reg = reg;
    self->ram = ram;
}

void pdp11Boot_load_machine_code(pdp11Boot *self, const pyint *code, int count, pyint base)
{
    /* Copy the pdp-11 machine code found at code into the address in base */
    pyint address = base;
    for (int i = 0; i < count; i++) {
        Ram_write_word(self->ram, address, code[i]);
        address = address + 2;
    }
    Registers_set_pc(self->reg, base, "load_machine_code");
}

pyint pdp11Boot_octal_to_decimal(pdp11Boot *self, pyint octal_value)
{
    /* convert an octal masquerading as an integer to decimal */
    (void)self;
    pyint decimal_value = 0;
    pyint base = 1;
    while (octal_value) {
        pyint last_digit = py_mod(octal_value, 10);
        octal_value = octal_value / 10;   /* int(octal_value / 10): truncates toward zero */
        decimal_value += last_digit * base;
        base = base * 8;
    }
    return decimal_value;
}

/* str.isnumeric() for the ASCII text in these files */
static int isnumeric_str(const char *s)
{
    if (!*s) return 0;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

pyint pdp11Boot_read_pdp11_assembly_file(pdp11Boot *self, const char *file)
{
    /* read a DEC-formatted assembly file with machine code
     * returns the address specified in the file */
    pyint base = 0;
    FILE *text = fopen(file, "r");
    if (!text) PY_RAISE("FileNotFoundError", file);
    char line[1024];
    while (fgets(line, sizeof line, text)) {
        /* parts = line.split() */
        char *parts[4];
        int nparts = 0;
        char *save = NULL;
        for (char *tok = strtok_r(line, " \t\r\n\v\f", &save); tok && nparts < 4; tok = strtok_r(NULL, " \t\r\n\v\f", &save))
            parts[nparts++] = tok;

        /* if the line is empty, skip it */
        if (nparts == 0) continue;
        char *part0 = parts[0];

        /* if the line starts with ;, skip it */
        if (!strcmp(part0, ";")) continue;

        /* first item is the address */
        pyint address;
        if (isnumeric_str(part0)) {
            address = pdp11Boot_octal_to_decimal(self, strtoll(part0, NULL, 10));
            /* the first address is the base address */
            if (base == 0) base = address;
        } else {
            /* if it doesn't make sense, skip it */
            continue;
        }

        /* get the next value */
        if (nparts < 2) PY_RAISE("IndexError", "list index out of range");
        char *part1 = parts[1];
        if (isnumeric_str(part1)) {
            pyint value1 = pdp11Boot_octal_to_decimal(self, strtoll(part1, NULL, 10));
            Ram_write_word(self->ram, address, value1);
        }
    }
    fclose(text);

    char a[PYSTR];
    logging_info("read_pdp11_assembly_file \"%s\" returns base address:%s", file, py_oct(a, base));
    return base;
}
