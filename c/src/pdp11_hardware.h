/* pdp11_hardware.h - translation of pdp11_hardware.py:
 * PDP11 Registers, RAM, PSW, Stack, AddressModes */
#ifndef PDP11_HARDWARE_H
#define PDP11_HARDWARE_H

#include <pthread.h>
#include <stdint.h>
#include <stddef.h>

#include "pyint.h"
#include "pdp11_util.h"

pyint fix_sign(pyint word);
pyint address_offset(pyint address, pyint offset);
char *formatted_offset(char *buf, pyint offset);

/* ---------------------------------------------------------------- Lock */
/* Stand-in for threading.Lock, including its locked() query. */
typedef struct {
    pthread_mutex_t mutex;
    int is_locked;
} Lock;

void Lock_init(Lock *self);
int  Lock_locked(Lock *self);
void Lock_acquire(Lock *self);
void Lock_release(Lock *self);

/* ----------------------------------------------------------- Registers */
typedef struct {
    pyint registermask;
    int sp;   /* R6 is the stack pointer */
    int pc;   /* R7 is the program counter */
    pyint registers[8];
} Registers;

void  Registers_init(Registers *self);
pyint Registers_get(Registers *self, int reg);
void  Registers_set(Registers *self, int reg, pyint value);
pyint Registers_get_pc(Registers *self);
pyint Registers_inc_pc(Registers *self, const char *whocalled);
void  Registers_set_pc(Registers *self, pyint newpc, const char *whocalled);
void  Registers_set_pc_2x_offset(Registers *self, pyint offset, const char *whocalled);
pyint Registers_get_sp(Registers *self);
void  Registers_set_sp(Registers *self, pyint newsp, const char *whocalled);
char *Registers_registers_to_string(Registers *self, char *buf, size_t cap);

/* ----------------------------------------------------------------- Ram */
/* An I/O page handler is a bound method in Python: an object plus a function. */
typedef pyint (*io_reader_fn)(void *device);
typedef void  (*io_writer_fn)(void *device, pyint data);

typedef struct {
    void *device;
    io_reader_fn read;
    const char *name;
} IOReader;

typedef struct {
    void *device;
    io_writer_fn write;
    const char *name;
} IOWriter;

typedef struct {
    Registers *reg;
    pyint top_of_memory;
    pyint io_space;
    uint8_t *memory;        /* bytearray(top_of_memory+1) */
    pyint memory_size;
    /* iomap_readers / iomap_writers: dictionaries keyed by address.  Handlers
     * may only be registered in the I/O page, so they are stored in arrays
     * indexed by (address - io_space). */
    IOReader *iomap_readers;
    IOWriter *iomap_writers;
    pyint top_of_vector_space;
    Lock *lock;
    int i_set_lock;
} Ram;

void  Ram_init(Ram *self, Lock *lock, Registers *reg, pyint bits);
void  Ram_free(Ram *self);
void  Ram_get_lock(Ram *self, const char *whocalled);
void  Ram_release_lock(Ram *self, const char *whocalled);
void  Ram_register_io_writer(Ram *self, pyint device_address, void *device, io_writer_fn method, const char *name);
void  Ram_register_io_reader(Ram *self, pyint device_address, void *device, io_reader_fn method, const char *name);
void  Ram_write_byte(Ram *self, pyint address, pyint data);
pyint Ram_read_byte(Ram *self, pyint address);
void  Ram_write_word(Ram *self, pyint address, pyint data);
pyint Ram_read_word(Ram *self, pyint address);
pyint Ram_read_word_from_pc(Ram *self);
void  Ram_dump(Ram *self, pyint start, pyint stop);
/* ram.memory[address], with Python's negative-index behaviour */
uint8_t *Ram_memory(Ram *self, pyint address);

/* ----------------------------------------------------------------- PSW */
typedef struct {
    Ram *ram;
    pyint psw_address;
    pyint psw;
    pyint c_mode_mask, p_mode_mask, mode_mask, priority_mask, trap_mask;
    pyint n_mask, z_mask, v_mask, c_mask;
} PSW;

/* Python: set_psw(mode=-1, priority=-1, trap=-1, n=-1, z=-1, v=-1, c=-1, psw=-1)
 * In C: PSW_set_psw(self, .n=1, .z=0) - unnamed fields default to -1. */
typedef struct {
    pyint mode, priority, trap, n, z, v, c, psw;
} PSWArgs;

#define PSW_set_psw(self, ...) \
    PSW_set_psw_args((self), (PSWArgs){ .mode = -1, .priority = -1, .trap = -1, \
                                        .n = -1, .z = -1, .v = -1, .c = -1, .psw = -1, __VA_ARGS__ })

void  PSW_init(PSW *self, Ram *ram);
pyint PSW_get_psw(PSW *self);
void  PSW_set_psw_args(PSW *self, PSWArgs args);
void  PSW_set_nzvc(PSW *self, pyint value);
/* b is 'B' for byte, 'W' or 0 ("") for word, as in the Python */
pyint PSW_set_n(PSW *self, char b, pyint value);
pyint PSW_set_z(PSW *self, char b, pyint value);
pyint PSW_set_v(PSW *self, char b, pyint value);
pyint PSW_get_n(PSW *self);
pyint PSW_get_z(PSW *self);
pyint PSW_get_v(PSW *self);
pyint PSW_get_c(PSW *self);
char *PSW_get_nzvc(PSW *self, char *buf);   /* buf of at least 8 */

/* --------------------------------------------------------------- Stack */
typedef struct {
    Registers *reg;
    Ram *ram;
    PSW *psw;
} Stack;

void  Stack_init(Stack *self, Registers *reg, Ram *ram, PSW *psw);
void  Stack_push(Stack *self, pyint value);
pyint Stack_pop(Stack *self);
/* trap sequence: push PS and PC, load PC and PS from vector and vector+2 */
void  Stack_trap(Stack *self, pyint vector);

/* -------------------------------------------------------- AddressModes */
typedef struct {
    Registers *reg;
    Ram *ram;
    PSW *psw;
    pyint address_modes_used[8];
} AddressModes;

/* return operand, register, address, operand_word, assembly, addressmode */
typedef struct {
    pyint operand;
    int register_;
    pyint address;
    char operand_word[PYSTR];
    char assembly[PYSTR];
    int addressmode;
} AMGet;

/* return run, jump_address, operand_word, assembly */
typedef struct {
    int run;
    pyint jump_address;
    char operand_word[PYSTR];
    char assembly[PYSTR];
} AMJmp;

void   AddressModes_init(AddressModes *self, Registers *reg, Ram *ram, PSW *psw);
AMGet  AddressModes_addressing_mode_get(AddressModes *self, char b, pyint mode_register);
/* effective address only; the operand is not read */
AMGet  AddressModes_addressing_mode_address(AddressModes *self, char b, pyint mode_register);
AMJmp  AddressModes_addressing_mode_jmp(AddressModes *self, pyint mode_register);
void   AddressModes_addressing_mode_set(AddressModes *self, char b, int addressmode, pyint result, int register_, pyint address);
void   AddressModes_address_mode_report(AddressModes *self);

#endif
