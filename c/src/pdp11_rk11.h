/* pdp11_rk11.h - translation of pdp11_rk11.py (DEC RK11 disk interface, stubbed) */
#ifndef PDP11_RK11_H
#define PDP11_RK11_H

#include "pdp11_hardware.h"

typedef struct {
    Ram *ram;
    pyint RKDS_address, RKER_address, RKCS_address, RKWC_address;
    pyint RKBA_address, RKDA_address, RKMR_address, RKDB_address;
    pyint RKCS, RKWC, RKBA, RKDA, RKMR, RKDB;
} RK11;

void RK11_init(RK11 *self, Ram *ram);
void RK11_control_reset(RK11 *self);
void RK11_drive_reset(RK11 *self);
void RK11_write_lock(RK11 *self);
void RK11_seek(RK11 *self);
void RK11_write_check(RK11 *self);
void RK11_write(RK11 *self);
void RK11_read_check(RK11 *self);
void RK11_read(RK11 *self);

#endif
