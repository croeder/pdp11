/* pdp11_boot.h - translation of pdp11_boot.py
 * load machine code, or an assembly listing with machine code, into pdp11 ram */
#ifndef PDP11_BOOT_H
#define PDP11_BOOT_H

#include "pdp11_hardware.h"

typedef struct {
    Registers *reg;
    Ram *ram;
} pdp11Boot;

/* class attributes */
extern const pyint pdp11Boot_bootstrap_loader[];
extern const int pdp11Boot_bootstrap_loader_len;
extern const pyint pdp11Boot_bootaddress;
extern const pyint pdp11Boot_echo[];
extern const int pdp11Boot_echo_len;
extern const pyint pdp11Boot_echo_address;

void  pdp11Boot_init(pdp11Boot *self, Registers *reg, Ram *ram);
void  pdp11Boot_load_machine_code(pdp11Boot *self, const pyint *code, int count, pyint base);
pyint pdp11Boot_octal_to_decimal(pdp11Boot *self, pyint octal_value);
pyint pdp11Boot_read_pdp11_assembly_file(pdp11Boot *self, const char *file);

#endif
