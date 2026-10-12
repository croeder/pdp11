/* pdp11_m9301.h - translation of pdp11_m9301.py (M9301 boot ROM card) */
#ifndef PDP11_M9301_H
#define PDP11_M9301_H

#include "pdp11_boot.h"
#include "pdp11_hardware.h"

typedef struct {
    Ram *ram;
    pyint base_address;
    pyint switch_address;
    pyint switch_settings;
} M9301;

void  M9301_init(M9301 *self, Registers *reg, Ram *ram, pdp11Boot *boot);
pyint M9301_read_dip_switches(M9301 *self);

#endif
