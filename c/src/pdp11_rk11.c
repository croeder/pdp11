/* pdp11_rk11.c - translation of pdp11_rk11.py
 * DEC RK11 disk drive interface.  *** stubbed out. Inoperative. */
#include "pdp11_rk11.h"
#include "pdp11_logger.h"

#define RK_WRITER(reg)                                         \
    static void write_##reg(void *d, pyint data)               \
    {                                                          \
        logging_info("RK11 write_" #reg);                      \
        ((RK11 *)d)->reg = data;                               \
    }
#define RK_READER(reg)                                         \
    static pyint read_##reg(void *d)                           \
    {                                                          \
        logging_info("RK11 read_" #reg);                       \
        return ((RK11 *)d)->reg;                               \
    }

RK_WRITER(RKCS) RK_WRITER(RKWC) RK_WRITER(RKBA) RK_WRITER(RKDA) RK_WRITER(RKMR) RK_WRITER(RKDB)
RK_READER(RKCS) RK_READER(RKWC) RK_READER(RKBA) RK_READER(RKDA) RK_READER(RKMR) RK_READER(RKDB)

static pyint read_RKDS(void *d) { (void)d; logging_info("RK11 read_RKDS"); return 0; }
static pyint read_RKER(void *d) { (void)d; logging_info("RK11 read_RKER"); return 0; }

void RK11_init(RK11 *self, Ram *ram)
{
    logging_info("initializing RK11");
    self->ram = ram;
    pyint base_address = 0177400;
    self->RKDS_address = base_address;
    self->RKER_address = base_address + 002;
    self->RKCS_address = base_address + 004;
    self->RKWC_address = base_address + 006;
    self->RKBA_address = base_address + 010;
    self->RKDA_address = base_address + 012;
    self->RKMR_address = base_address + 014;
    self->RKDB_address = base_address + 016;

    logging_info("RK11 register_with_ram");
    Ram_register_io_writer(ram, self->RKCS_address, self, write_RKCS, "write_RKCS");  /* rw */
    Ram_register_io_writer(ram, self->RKWC_address, self, write_RKWC, "write_RKWC");  /* rw */
    Ram_register_io_writer(ram, self->RKBA_address, self, write_RKBA, "write_RKBA");  /* rw */
    Ram_register_io_writer(ram, self->RKDA_address, self, write_RKDA, "write_RKDA");  /* rw */
    Ram_register_io_writer(ram, self->RKMR_address, self, write_RKMR, "write_RKMR");  /* rw */
    Ram_register_io_writer(ram, self->RKDB_address, self, write_RKDB, "write_RKDB");  /* rw */

    Ram_register_io_reader(ram, self->RKDS_address, self, read_RKDS, "read_RKDS");  /* ro */
    Ram_register_io_reader(ram, self->RKER_address, self, read_RKER, "read_RKER");  /* ro */
    Ram_register_io_reader(ram, self->RKCS_address, self, read_RKCS, "read_RKCS");
    Ram_register_io_reader(ram, self->RKWC_address, self, read_RKWC, "read_RKWC");
    Ram_register_io_reader(ram, self->RKBA_address, self, read_RKBA, "read_RKBA");
    Ram_register_io_reader(ram, self->RKDA_address, self, read_RKDA, "read_RKDA");
    Ram_register_io_reader(ram, self->RKMR_address, self, read_RKMR, "read_RKMR");
    Ram_register_io_reader(ram, self->RKDB_address, self, read_RKDB, "read_RKDB");

    self->RKCS = 0;
    self->RKWC = 0;
    self->RKBA = 0;
    self->RKDA = 0;
    self->RKMR = 0;
    self->RKDB = 0;
    logging_info("initializing RK11 done");
}

void RK11_control_reset(RK11 *self) { (void)self; logging_info("RK11 control reset"); }
void RK11_drive_reset(RK11 *self)   { (void)self; logging_info("RK11 drive reset"); }
void RK11_write_lock(RK11 *self)    { (void)self; logging_info("RK11 write lock"); }
void RK11_seek(RK11 *self)          { (void)self; logging_info("RK11 seek"); }
void RK11_write_check(RK11 *self)   { (void)self; logging_info("RK11 write check"); }
void RK11_write(RK11 *self)         { (void)self; logging_info("RK11 write"); }
void RK11_read_check(RK11 *self)    { (void)self; logging_info("RK11 read check"); }
void RK11_read(RK11 *self)          { (void)self; logging_info("RK11 read"); }
