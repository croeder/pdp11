/* pdp11_dl11.h - translation of pdp11_dl11.py (DEC DL11 serial interface) */
#ifndef PDP11_DL11_H
#define PDP11_DL11_H

#include "pdp11_hardware.h"

typedef struct {
    Ram *ram;
    Lock *lock;
    pyint RCSR_address, RBUF_address, XCSR_address, XBUF_address;

    /* RCSR Receiver Status Register bits (DL11-B) */
    pyint RCSR_RCVR_ACT;   /* 11 RO - active (not implemented) */
    pyint RCSR_RCVR_DONE;  /* 7 RO - set when character received, cleared when RBUF is read */
    pyint RCSR_INT_ENB;    /* 6 - enables interrupt (not implemented) */
    pyint RCSR;            /* receive status register */
    pyint RBUF;            /* receive buffer */

    /* XCSR Transmitter Status Register bits (DL11-B) */
    pyint XCSR_XMIT_RDY;     /* 7 RO - set when transmitter can accept another character */
    pyint XCSR_XMIT_INT_ENB; /* 6 RW - enables interrupt (not implemented) */
    pyint XCSR_MAINT;        /* 2 RW - maintenance loopback XBUF to RBUF */
    pyint XCSR;              /* transmit status register */
    pyint XBUF;              /* transmit buffer */

    int i_set_lock;
} DL11;

void  DL11_init(DL11 *self, Ram *ram, pyint base_address);
void  DL11_get_lock(DL11 *self, const char *whocalled);
void  DL11_release_lock(DL11 *self, const char *whocalled);

void  DL11_write_RCSR(DL11 *self, pyint byte);
pyint DL11_read_RCSR(DL11 *self);
void  DL11_write_RBUF(DL11 *self, pyint byte);
pyint DL11_read_RBUF(DL11 *self);
void  DL11_write_XCSR(DL11 *self, pyint byte);
pyint DL11_read_XCSR(DL11 *self);
void  DL11_write_XBUF(DL11 *self, pyint byte);
pyint DL11_read_XBUF(DL11 *self);

#endif
