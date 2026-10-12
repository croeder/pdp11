/* pdp11_dl11.c - translation of pdp11_dl11.py (DEC DL11 serial interface emulator)
 *
 * All reads and writes to IO buffers and CSRs must be protected.
 * CPU access to these routines is supposed to happen through hardware
 * ram.read and ram.write methods.  DL11 access happens directly through these calls. */
#include "pdp11_dl11.h"
#include "pdp11_logger.h"

#include <stdio.h>

/* io-page adapters: Ram calls these with the DL11 as the device */
static void io_write_RCSR(void *d, pyint v) { DL11_write_RCSR(d, v); }
static void io_write_RBUF(void *d, pyint v) { DL11_write_RBUF(d, v); }
static void io_write_XCSR(void *d, pyint v) { DL11_write_XCSR(d, v); }
static void io_write_XBUF(void *d, pyint v) { DL11_write_XBUF(d, v); }
static pyint io_read_RCSR(void *d) { return DL11_read_RCSR(d); }
static pyint io_read_RBUF(void *d) { return DL11_read_RBUF(d); }
static pyint io_read_XCSR(void *d) { return DL11_read_XCSR(d); }
static pyint io_read_XBUF(void *d) { return DL11_read_XBUF(d); }

void DL11_init(DL11 *self, Ram *ram, pyint base_address)
{
    char a[PYSTR];
    logging_info("initializing dl11(%s)", py_oct(a, base_address));
    self->ram = ram;
    logging_info("dl11 ram:%p", (void *)self->ram);
    self->lock = ram->lock;
    logging_info("dl11 lock:%p", (void *)self->lock);
    self->RCSR_address = base_address;
    self->RBUF_address = base_address + 2;
    self->XCSR_address = base_address + 4;
    self->XBUF_address = base_address + 6;

    Ram_register_io_writer(ram, self->RCSR_address, self, io_write_RCSR, "write_RCSR");
    Ram_register_io_writer(ram, self->RBUF_address, self, io_write_RBUF, "write_RBUF");
    Ram_register_io_writer(ram, self->XCSR_address, self, io_write_XCSR, "write_XCSR");
    Ram_register_io_writer(ram, self->XBUF_address, self, io_write_XBUF, "write_XBUF");

    Ram_register_io_reader(ram, self->RCSR_address, self, io_read_RCSR, "read_RCSR");
    Ram_register_io_reader(ram, self->RBUF_address, self, io_read_RBUF, "read_RBUF");
    Ram_register_io_reader(ram, self->XCSR_address, self, io_read_XCSR, "read_XCSR");
    Ram_register_io_reader(ram, self->XBUF_address, self, io_read_XBUF, "read_XBUF");

    /* RCSR Receiver Status Register bits.  This is a DL11-B */
    self->RCSR_RCVR_ACT = 0004000;
    self->RCSR_RCVR_DONE = 0000200;
    self->RCSR_INT_ENB = 0000100;
    self->RCSR = 0;  /* Not active; no character yet */
    self->RBUF = 0;

    /* XCSR Transmitter Status Register bits.  This is a DL11-B */
    self->XCSR_XMIT_RDY = 0000200;
    self->XCSR_XMIT_INT_ENB = 0000100;
    self->XCSR_MAINT = 0000004;
    self->XCSR = self->XCSR_XMIT_RDY;  /* transmit status register ready on init */
    self->XBUF = 0;

    self->i_set_lock = 0;
    logging_info("ram.lock: %p", (void *)self->lock);
    logging_info("initializing dl11 done");
}

void DL11_get_lock(DL11 *self, const char *whocalled)
{
    logging_info("get_lock called by %s", whocalled);
    if (!Lock_locked(self->lock)) {
        Lock_acquire(self->lock);
        logging_info("dl11 got lock");
        self->i_set_lock = 1;
    }
}

void DL11_release_lock(DL11 *self, const char *whocalled)
{
    logging_info("release_lock called by %s", whocalled);
    if (self->i_set_lock) {
        Lock_release(self->lock);
        logging_info("dl11 released lock");
        self->i_set_lock = 0;
    }
}

/* DL11 Internal Interface to PDP-11 Emulator
 *
 * Receiver receives characters from a terminal.
 * RCSR Receiver Status Register
 * 7: set when character has been received (ro)
 * 6: receiver interrupt enable (rw)
 * 0: reader enable (wo) clears 7 */
void DL11_write_RCSR(DL11 *self, pyint byte)
{
    /* write to receiver status register */
    DL11_get_lock(self, "write_RCSR");
    self->RCSR = byte;
    DL11_release_lock(self, "write_RCSR");
}

pyint DL11_read_RCSR(DL11 *self)
{
    /* read from receiver status register */
    DL11_get_lock(self, "read_RCSR");
    pyint result = self->RCSR;
    DL11_release_lock(self, "read_RCSR");
    return result;
}

/* RBUF receiver data buffer (ro)
 * 15: error  14: overrun  13: framing error  12: receive parity error
 * 7-0 received data */
void DL11_write_RBUF(DL11 *self, pyint byte)
{
    /* DL11 calls this to write to receiver buffer and set ready bit */
    char a[PYSTR], b[PYSTR], who[PYSTR + 16];
    logging_info("dl11.write_RBUF %s %s", py_oct(a, byte), safe_character(b, byte));
    snprintf(who, sizeof who, "write_RBUF(%s)", a);
    DL11_get_lock(self, who);
    self->RBUF = byte;
    self->RCSR = self->RCSR | self->RCSR_RCVR_DONE;
    if ((self->RCSR & self->RCSR_RCVR_DONE) != self->RCSR_RCVR_DONE)
        logging_error("XCSR %s was not set with %s", py_oct(a, self->RCSR), py_oct(b, self->RCSR_RCVR_DONE));
    DL11_release_lock(self, "write_RBUF");
    logging_info("dl11.write_RBUF exit");
}

pyint DL11_read_RBUF(DL11 *self)
{
    /* PDP11 calls this to read from receiver buffer. Read buffer and reset ready bit */
    DL11_get_lock(self, "read_RBUF");
    pyint result = self->RBUF;
    pyint RCSR = self->RCSR & ~self->RCSR_RCVR_DONE;
    self->RCSR = RCSR;
    if ((self->RCSR & self->RCSR_RCVR_DONE) == self->RCSR_RCVR_DONE) {
        char a[PYSTR], b[PYSTR];
        logging_error("XCSR %s was not cleared with %s", py_oct(a, self->RCSR), py_oct(b, self->RCSR_RCVR_DONE));
    }
    /* cleared when RBUF is read */
    DL11_release_lock(self, "read_RBUF");
    return result;
}

/* Transmitter enables CPU to send characters to a terminal.
 * XCSR transmit status register (rw)
 * 7: transmitter ready (ro). Cleared when XBUF is loaded. Set when XBUF can accept another character.
 * 6: transmit interrupt enable (rw)
 * 2: maintenance. when set sends serial output to serial input (rw)
 * 0: break. when set sends continuous space (rw) */
void DL11_write_XCSR(DL11 *self, pyint byte)
{
    /* write to transmitter status register */
    DL11_get_lock(self, "write_XCSR");
    self->XCSR = byte;
    DL11_release_lock(self, "write_XCSR");
}

pyint DL11_read_XCSR(DL11 *self)
{
    /* read from transmitter status register */
    DL11_get_lock(self, "read_XCSR");
    pyint result = self->XCSR;
    DL11_release_lock(self, "read_XCSR");
    return result;
}

/* XBUF transmit data buffer (wo)
 * 7-0 transmitted data buffer */
void DL11_write_XBUF(DL11 *self, pyint byte)
{
    /* PDP11 calls this to write to transmitter buffer register. */
    DL11_get_lock(self, "write_XBUF({oct(byte)})");  /* sic: not an f-string in the Python */
    self->XBUF = byte;
    /* XCSR_XMIT_RDY is cleared when XBUF is loaded */
    pyint XCSR = self->XCSR & ~self->XCSR_XMIT_RDY;
    self->XCSR = XCSR;
    if ((self->XCSR & self->XCSR_XMIT_RDY) == self->XCSR_XMIT_RDY) {
        char a[PYSTR], b[PYSTR];
        logging_error("XCSR %s was not cleared with %s", py_oct(a, self->XCSR), py_oct(b, self->XCSR_XMIT_RDY));
    }
    /* check for Maintenance mode */
    if ((self->XCSR & self->XCSR_MAINT) == self->XCSR_MAINT)
        DL11_write_RBUF(self, byte);
    DL11_release_lock(self, "write_XBUF");
}

pyint DL11_read_XBUF(DL11 *self)
{
    /* DL11 calls this to read from transmitter buffer register. */
    DL11_get_lock(self, "read_XBUF");
    pyint result = self->XBUF;
    /* XCSR_XMIT_RDY is set when XBUF can accept another character */
    pyint XCSR = self->XCSR | self->XCSR_XMIT_RDY;
    self->XCSR = XCSR;
    if ((self->XCSR & self->XCSR_XMIT_RDY) != self->XCSR_XMIT_RDY) {
        char a[PYSTR], b[PYSTR];
        logging_error("XCSR %s was not set with %s", py_oct(a, self->XCSR), py_oct(b, self->XCSR_XMIT_RDY));
    }
    DL11_release_lock(self, "read_XBUF");
    return result;
}
