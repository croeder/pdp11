/* pdp11_hardware.c - translation of pdp11_hardware.py:
 * PDP11 Registers, RAM, PSW, Stack, AddressModes */
#include "pdp11_hardware.h"
#include "pdp11_logger.h"

#include <stdlib.h>
#include <string.h>

/* Python's assert; kept active regardless of NDEBUG */
#define py_assert(cond) do { if (!(cond)) PY_RAISE("AssertionError", #cond); } while (0)

#define SYS_MAXSIZE INT64_MAX

pyint fix_sign(pyint word)
{
    /* fix a negative word so it will work with python math */
    pyint result;
    if (word & MASK_WORD_MSB) {
        pyint more_bits = SYS_MAXSIZE & ~MASK_WORD;
        pyint field = SYS_MAXSIZE - ((word & MASK_WORD) | more_bits) + 1;
        result = -field;
    } else {
        result = word;
    }
    return result;
}

pyint address_offset(pyint address, pyint offset)
{
    /* address plus sign-fixed offset */
    return address + fix_sign(offset);
}

char *formatted_offset(char *buf, pyint offset)
{
    /* Format offset for disassembly */
    pyint formatted = offset;
    /* if the high bit is set, it's negative: mask it off and subtract what's left */
    if ((offset & MASK_WORD_MSB) == MASK_WORD_MSB)
        formatted = -(077777 - (offset & 077777)) * 2;
    return py_oct(buf, formatted);
}

/* ================================================================ Lock */

void Lock_init(Lock *self)
{
    pthread_mutex_init(&self->mutex, NULL);
    self->is_locked = 0;
}

int Lock_locked(Lock *self) { return self->is_locked; }

void Lock_acquire(Lock *self)
{
    pthread_mutex_lock(&self->mutex);
    self->is_locked = 1;
}

void Lock_release(Lock *self)
{
    if (!self->is_locked) PY_RAISE("RuntimeError", "release unlocked lock");
    self->is_locked = 0;
    pthread_mutex_unlock(&self->mutex);
}

/* =========================================================== Registers */

void Registers_init(Registers *self)
{
    logging_info("initializing pdp11_hardware registers");
    self->registermask = 07;
    self->sp = 06;   /* R6 is the stack pointer */
    self->pc = 07;   /* R7 is the program counter */
    memset(self->registers, 0, sizeof self->registers);   /* R0 R1 R2 R3 R4 R5 R6 R7 */
}

static void check_register(int reg)
{
    /* Python list indexing: -8..7 are valid */
    if (reg < -8 || reg > 7) PY_RAISE("IndexError", "list index out of range");
}

pyint Registers_get(Registers *self, int reg)
{
    /* read a single register; register is integer 0-7 */
    check_register(reg);
    return self->registers[reg < 0 ? reg + 8 : reg];
}

void Registers_set(Registers *self, int reg, pyint value)
{
    /* set a single register */
    py_assert(value <= MASK_WORD);
    check_register(reg);
    self->registers[reg < 0 ? reg + 8 : reg] = value;
}

pyint Registers_get_pc(Registers *self)
{
    /* get program counter without incrementing */
    return self->registers[self->pc];
}

pyint Registers_inc_pc(Registers *self, const char *whocalled)
{
    /* increment program counter by 2.
     * The Program Counter points to the word after the instruction being interpreted. */
    (void)whocalled;
    self->registers[self->pc] = self->registers[self->pc] + 2;
    return self->registers[self->pc];
}

void Registers_set_pc(Registers *self, pyint newpc, const char *whocalled)
{
    /* set program counter to arbitrary value */
    (void)whocalled;
    py_assert(newpc <= MASK_WORD);
    self->registers[self->pc] = newpc;
}

void Registers_set_pc_2x_offset(Registers *self, pyint offset, const char *whocalled)
{
    /* set program counter to 2x offset byte; adjust for sign bit and PC.
     * Bit 7 of the offset is the sign.  If it is set then the offset is negative. */
    (void)whocalled;
    offset = 2 * offset;
    if (offset > MASK_LOW_BYTE)  /* if we overflowed with the sign bit */
        offset = fix_sign(offset | MASK_HIGH_BYTE);
    self->registers[self->pc] = self->registers[self->pc] + offset;
}

pyint Registers_get_sp(Registers *self)
{
    return self->registers[self->sp];
}

void Registers_set_sp(Registers *self, pyint newsp, const char *whocalled)
{
    (void)whocalled;
    py_assert(newsp <= MASK_WORD);
    self->registers[self->sp] = newsp;
}

char *Registers_registers_to_string(Registers *self, char *buf, size_t cap)
{
    /* report = f'{report} {u.oct6(register)}' for each register */
    size_t n = 0;
    buf[0] = '\0';
    for (int i = 0; i < 8; i++) {
        char o[PYSTR];
        n += (size_t)snprintf(buf + n, n < cap ? cap - n : 0, " %s", oct6(o, self->registers[i]));
    }
    return buf;
}

/* ================================================================= Ram */
/* Sixteen-bit words are stored little-endian.
 * Bytes can be on even or odd addresses.  Words must be on even addresses.
 * Odd-numbered bytes are high.  Even-numbered bytes are low.
 * for example 0o271 - high byte, 0o270 - low byte */

uint8_t *Ram_memory(Ram *self, pyint address)
{
    /* bytearray indexing: negative addresses count back from the end */
    pyint index = address < 0 ? address + self->memory_size : address;
    if (index < 0 || index >= self->memory_size) PY_RAISE("IndexError", "bytearray index out of range");
    return &self->memory[index];
}

static IOWriter *find_writer(Ram *self, pyint address)
{
    if (address < self->io_space || address > self->top_of_memory) return NULL;
    IOWriter *w = &self->iomap_writers[address - self->io_space];
    return w->write ? w : NULL;
}

static IOReader *find_reader(Ram *self, pyint address)
{
    if (address < self->io_space || address > self->top_of_memory) return NULL;
    IOReader *r = &self->iomap_readers[address - self->io_space];
    return r->read ? r : NULL;
}

void Ram_init(Ram *self, Lock *lock, Registers *reg, pyint bits)
{
    logging_info("initializing pdp11_hardware.Ram(%lld)", (long long)bits);
    self->reg = reg;

    /* set up basic memory boundaries
     * 16 bits   0o177777 = 0x0000FFFF 64kB 32kw
     * 18 bits   0o777777 = 0x0003FFFF 256kB 128kw
     * 24 bits 0o17777777 = 0x003FFFFF 4MB 4096kB 2048kw */
    self->top_of_memory = py_shl(1, bits) - 1;

    /* the io page is the top 4k words (8kB) of memory */
    self->io_space = self->top_of_memory - 017777;
    char a[PYSTR], b[PYSTR];
    logging_info("%lld bits -> top_of_memory: %s = %lld; io_space:%s io_space=%lld",
                 (long long)bits, py_oct(a, self->top_of_memory), (long long)self->top_of_memory,
                 py_oct(b, self->io_space), (long long)self->io_space);

    /* instantiate the byte array */
    self->memory_size = self->top_of_memory + 1;
    self->memory = calloc((size_t)self->memory_size, 1);

    /* io map is dictionaries of addresses and methods */
    pyint io_entries = self->top_of_memory - self->io_space + 1;
    self->iomap_readers = calloc((size_t)io_entries, sizeof *self->iomap_readers);
    self->iomap_writers = calloc((size_t)io_entries, sizeof *self->iomap_writers);

    /* set up the vector space: the bottom area is io device handler vectors */
    self->top_of_vector_space = 0274;
    for (pyint address = 0; address < self->top_of_vector_space; address++)
        Ram_write_byte(self, address, 0277);

    /* Initialize the shadow RAM for io page. */
    for (pyint address = self->io_space; address < self->top_of_memory; address++)
        Ram_write_byte(self, address, 0);

    self->lock = lock;
    self->i_set_lock = 0;
    logging_info("Ram init done. Lock object:%p", (void *)self->lock);
}

void Ram_free(Ram *self)
{
    free(self->memory);
    free(self->iomap_readers);
    free(self->iomap_writers);
}

/* emulator interface to locking
 * The idea is that any time there's unlocked access to IO page RAM,
 * It's the CPU doing it and that section needs to be locked. */
void Ram_get_lock(Ram *self, const char *whocalled)
{
    if (!Lock_locked(self->lock)) {
        Lock_acquire(self->lock);
        logging_info("%s got lock", whocalled);
        self->i_set_lock = 1;
    }
}

void Ram_release_lock(Ram *self, const char *whocalled)
{
    if (self->i_set_lock) {
        Lock_release(self->lock);
        logging_debug("%s released lock", whocalled);
        self->i_set_lock = 0;
    }
}

void Ram_register_io_writer(Ram *self, pyint device_address, void *device, io_writer_fn method, const char *name)
{
    /* map i/o write handler into memory */
    py_assert(device_address < self->top_of_memory);
    py_assert(device_address >= self->io_space);
    char a[PYSTR];
    logging_info("register_io_writer(%s, %s)", py_oct(a, device_address), name);
    IOWriter *w = &self->iomap_writers[device_address - self->io_space];
    w->device = device;
    w->write = method;
    w->name = name;
}

void Ram_register_io_reader(Ram *self, pyint device_address, void *device, io_reader_fn method, const char *name)
{
    /* map i/o read handler into memory */
    py_assert(device_address < self->top_of_memory);
    py_assert(device_address >= self->io_space);
    char a[PYSTR];
    logging_info("register_io_reader(%s, %s)", py_oct(a, device_address), name);
    IOReader *r = &self->iomap_readers[device_address - self->io_space];
    r->device = device;
    r->read = method;
    r->name = name;
}

void Ram_write_byte(Ram *self, pyint address, pyint data)
{
    /* write a byte to memory.  Address can be even or odd */
    py_assert(address <= self->top_of_memory);   /* *** should be a trap */
    data = data & MASK_LOW_BYTE;
    IOWriter *w = find_writer(self, address);
    if (w) {
        if (data != 0) {
            char a[PYSTR], b[PYSTR], c[PYSTR];
            logging_debug("write_byte io self_lock:%d @%s, %s %s", Lock_locked(self->lock),
                          py_oct(a, address), py_oct(b, data), safe_character(c, data));
        }
        Ram_get_lock(self, "");
        w->write(w->device, data);
        Ram_release_lock(self, "");
    } else {
        *Ram_memory(self, address) = (uint8_t)data;
    }
}

pyint Ram_read_byte(Ram *self, pyint address)
{
    /* Read one byte of memory.  Address can be even or odd. */
    py_assert(address <= self->top_of_memory);
    pyint result;
    IOReader *r = find_reader(self, address);
    if (r) {
        Ram_get_lock(self, "");
        result = r->read(r->device);
        Ram_release_lock(self, "");
        if (result != 0) {
            char a[PYSTR], b[PYSTR], c[PYSTR];
            logging_debug("read_byte io self_lock:%d @%s returns %s %s", Lock_locked(self->lock),
                          py_oct(a, address), py_oct(b, result), safe_character(c, result));
        }
    } else {
        result = *Ram_memory(self, address);
    }
    return result;
}

void Ram_write_word(Ram *self, pyint address, pyint data)
{
    /* write a two-byte data chunk to memory.  Address must be even. */
    py_assert(address < self->top_of_memory);   /* *** should be a trap */
    py_assert(py_mod(address, 2) == 0);           /* *** should be a trap */
    py_assert(data <= MASK_WORD);
    IOWriter *w = find_writer(self, address);
    if (w) {
        char a[PYSTR], b[PYSTR];
        logging_debug("write_word io @%s, %s", py_oct(a, address), py_oct(b, data));
        Ram_get_lock(self, "");
        w->write(w->device, data);
        Ram_release_lock(self, "");
    } else {
        *Ram_memory(self, address + 1) = (uint8_t)py_shr(data & MASK_HIGH_BYTE, 8);
        *Ram_memory(self, address) = (uint8_t)(data & MASK_LOW_BYTE);
    }
}

pyint Ram_read_word(Ram *self, pyint address)
{
    /* Read a word of memory.  Address must be even.  Returns a two-byte value.
     * Low bytes are stored at even-numbered memory locations.
     * High bytes at are stored at odd-numbered memory locations. */
    py_assert(address < self->top_of_memory);   /* *** should be a trap */
    py_assert(py_mod(address, 2) == 0);           /* *** should be a trap */
    pyint result;
    IOReader *r = find_reader(self, address);
    if (r) {
        Ram_get_lock(self, "");
        result = r->read(r->device);
        Ram_release_lock(self, "");
        char a[PYSTR], b[PYSTR];
        logging_debug("read word IO(%s) returns %s", oct6(a, address), oct6(b, result));
    } else {
        result = ((pyint)*Ram_memory(self, address + 1) << 8) + *Ram_memory(self, address);
    }
    return result;
}

pyint Ram_read_word_from_pc(Ram *self)
{
    /* Read word from PC and increment PC */
    pyint pc = Registers_get_pc(self->reg);
    pyint result = Ram_read_word(self, pc);
    Registers_inc_pc(self->reg, "read_word_from_pc");
    return result;
}

void Ram_dump(Ram *self, pyint start, pyint stop)
{
    /* log memory from a nice address before start to a nice address after stop.
     * A row is octal Words followed by octal Bytes followed by ascii interpretation */
    const int display_bytes = 8;  /* display this many bytes across the line */
    start = start & 0177770;
    stop = (stop & 0177770) + 0010;
    char a[PYSTR], b[PYSTR];
    logging_info("dump(%s, %s)", py_oct(a, start), py_oct(b, stop));
    logging_debug("@@@@@@ octal                        bytes                            ascii   ");

    for (pyint row_address = start; row_address < stop; row_address += display_bytes) {
        char line[256];
        char o[PYSTR];
        size_t n = (size_t)snprintf(line, sizeof line, "%s  ", oct6(o, row_address));
        for (pyint word_address = row_address; word_address < row_address + display_bytes; word_address += 2)
            n += (size_t)snprintf(line + n, sizeof line - n, "%s ", oct6(o, Ram_read_word(self, word_address)));
        n += (size_t)snprintf(line + n, sizeof line - n, " ");
        for (pyint byte_address = row_address; byte_address < row_address + display_bytes; byte_address++)
            n += (size_t)snprintf(line + n, sizeof line - n, "%s ", oct3(o, Ram_read_byte(self, byte_address)));
        n += (size_t)snprintf(line + n, sizeof line - n, " ");
        for (pyint byte_address = row_address; byte_address < row_address + display_bytes; byte_address++) {
            pyint byte = Ram_read_byte(self, byte_address);
            char ch = (byte >= 16 && byte < 127) ? (char)byte : '.';
            if (n + 1 < sizeof line) { line[n++] = ch; line[n] = '\0'; }
        }
        logging_info("%s", line);
    }
}

/* ================================================================= PSW */
/* PSW bit meanings and masks
 * 15 14 current mode        0o140000
 * 13 12 previous mode       0o030000
 * 7 6 5 priority            0o000340
 * 4 T trap                  0o000020
 * 3 get_n result was negative   0o000010
 * 2 get_z result was zero       0o000004
 * 1 get_v overflow              0o000002
 * 0 get_c result had carry      0o000001 */

static pyint psw_io_read(void *device) { return PSW_get_psw((PSW *)device); }

void PSW_init(PSW *self, Ram *ram)
{
    logging_info("initializing pdp11_hardware psw");
    self->ram = ram;
    self->psw_address = ram->top_of_memory - 1;  /* 0o177776 */

    /* This class keeps the definitive version of the PSW. */
    self->psw = 0;

    self->c_mode_mask = 0140000;
    self->p_mode_mask = 0030000;
    self->mode_mask = 0170000;
    self->priority_mask = 0000340;
    self->trap_mask = 0000020;

    /* NZVC condition codes */
    self->n_mask = 0000010;  /* Negative */
    self->z_mask = 0000004;  /* Zero */
    self->v_mask = 0000002;  /* Overflow */
    self->c_mask = 0000001;  /* Carry */

    /* set up psw as an io device so we're not constantly writing to ram */
    Ram_register_io_reader(self->ram, self->psw_address, self, psw_io_read, "get_psw");
    char a[PYSTR];
    logging_info("psw initilialized @%s", py_oct(a, self->psw_address));
}

pyint PSW_get_psw(PSW *self) { return self->psw; }

void PSW_set_psw_args(PSW *self, PSWArgs args)
{
    /* set processor status word by optional parameter */
    if (args.psw > -1)
        self->psw = args.psw;
    if (args.mode > -1) {
        pyint oldmode = self->psw & self->mode_mask;
        self->psw = (self->psw & ~self->c_mode_mask) | py_shl(args.mode, 14) | py_shr(oldmode, 2);
    }
    if (args.priority > -1)
        self->psw = (self->psw & ~self->priority_mask) | py_shl(args.priority, 5);
    if (args.trap > -1)
        self->psw = (self->psw & ~self->trap_mask) | py_shl(args.trap, 4);
    if (args.n > -1)
        self->psw = (self->psw & ~self->n_mask) | py_shl(args.n, 3);
    if (args.z > -1)
        self->psw = (self->psw & ~self->z_mask) | py_shl(args.z, 2);
    if (args.v > -1)
        self->psw = (self->psw & ~self->v_mask) | py_shl(args.v, 1);
    if (args.c > -1)
        self->psw = (self->psw & ~self->c_mask) | args.c;
}

void PSW_set_nzvc(PSW *self, pyint value)
{
    /* set condition codes (last four bits of psw) */
    pyint non_psw_bits = 0177760;
    pyint psw_bits = 0000017;
    self->psw = (self->psw & non_psw_bits) | (value & psw_bits);
}

pyint PSW_set_n(PSW *self, char b, pyint value)
{
    /* set condition code n based on the msb negative bit */
    pyint n_mask = (b == 'B') ? MASK_BYTE_MSB : MASK_WORD_MSB;
    pyint n = ((value & n_mask) == n_mask) ? 1 : 0;
    PSW_set_psw(self, .n = n);
    return n;
}

pyint PSW_set_z(PSW *self, char b, pyint value)
{
    /* set condition code z based on the value */
    pyint z_mask = (b == 'B') ? MASK_LOW_BYTE : MASK_WORD;
    pyint z = ((value & z_mask) == 0) ? 1 : 0;
    PSW_set_psw(self, .z = z);
    return z;
}

pyint PSW_set_v(PSW *self, char b, pyint value)
{
    /* set condition code v based on the value */
    pyint v = 0;
    if (b == 'B') {
        if (value > MASK_LOW_BYTE) v = 1;
    } else if (b == 'W') {
        if (value > MASK_WORD) v = 1;
    }
    /* TRANSLATION NOTE: the Python calls self.set_psw(v), passing v
     * positionally, so it lands in the first parameter, mode - not v.
     * Reproduced as written. */
    PSW_set_psw(self, .mode = v);
    return v;
}

pyint PSW_get_n(PSW *self) { return py_shr(self->psw & self->n_mask, 3); }
pyint PSW_get_z(PSW *self) { return py_shr(self->psw & self->z_mask, 2); }
pyint PSW_get_v(PSW *self) { return py_shr(self->psw & self->v_mask, 1); }
pyint PSW_get_c(PSW *self) { return self->psw & self->c_mask; }

char *PSW_get_nzvc(PSW *self, char *buf)
{
    /* return string containing NZVC bits */
    sprintf(buf, "%lld%lld%lld%lld", (long long)PSW_get_n(self), (long long)PSW_get_z(self),
            (long long)PSW_get_v(self), (long long)PSW_get_c(self));
    return buf;
}

/* =============================================================== Stack */

void Stack_init(Stack *self, Registers *reg, Ram *ram, PSW *psw)
{
    logging_info("initializing pdp11_hardware stack");
    self->reg = reg;
    self->ram = ram;
    self->psw = psw;
}

void Stack_push(Stack *self, pyint value)
{
    /* decrement stack pointer, write value to that address */
    pyint stack = Registers_get_sp(self->reg) - 2;
    Registers_set_sp(self->reg, stack, "");
    Ram_write_word(self->ram, stack, value);
}

pyint Stack_pop(Stack *self)
{
    /* get value from stack pointer, increment stack pointer */
    pyint stack = Registers_get_sp(self->reg);
    pyint result = Ram_read_word(self->ram, stack);
    Registers_set_sp(self->reg, stack + 2, "");
    return result;
}

/* ======================================================== AddressModes */
/* Implements the 8 standard address modes of the PDP11 instruction set.
 * Autoincrement and autodecrement operations on a register are
 * by 1 in byte instructions, by 2 in word instructions,
 * and by 2 whenever a deferred mode is used,
 * since the quantity the register addresses is a (word) pointer. */

void AddressModes_init(AddressModes *self, Registers *reg, Ram *ram, PSW *psw)
{
    logging_info("initializing pdp11_hardware addressModes");
    self->reg = reg;
    self->ram = ram;
    self->psw = psw;
    memset(self->address_modes_used, 0, sizeof self->address_modes_used);
}

AMGet AddressModes_addressing_mode_get(AddressModes *self, char b, pyint mode_register)
{
    /* copy the value from the location indicated by mode_register
     * b: 'B' for byte, anything else for word
     * mode_register: six bits that specify the mode and the register */
    AMGet r;
    memset(&r, 0, sizeof r);
    int addressmode = (int)py_shr(mode_register & 070, 3);
    self->address_modes_used[addressmode] = self->address_modes_used[addressmode] + 1;

    int register_ = (int)(mode_register & 007);
    pyint address = 0;
    pyint operand = 0;
    if (addressmode != 0 && register_ == 7)
        oct6(r.operand_word, Ram_read_word(self->ram, Registers_get(self->reg, 7)));

    pyint (*ram_read)(Ram *, pyint);
    pyint increment;
    if (b == 'B') {
        ram_read = Ram_read_byte;
        increment = 1;
    } else {
        ram_read = Ram_read_word;
        increment = 2;
    }
    if (register_ == 6 || register_ == 7)
        increment = 2;

    char x_text[PYSTR];
    switch (addressmode) {
    case 0:  /* Register: register contains operand */
        operand = Registers_get(self->reg, register_);
        sprintf(r.assembly, "R%d", register_);
        break;
    case 1:  /* Register Deferred: register contains address of operand */
        address = Registers_get(self->reg, register_);
        operand = ram_read(self->ram, address);
        sprintf(r.assembly, "@R%d", register_);
        break;
    case 2:  /* Autoincrement: register contains address of operand then incremented */
        address = Registers_get(self->reg, register_);
        operand = ram_read(self->ram, address);
        Registers_set(self->reg, register_, Registers_get(self->reg, register_) + increment);
        sprintf(r.assembly, "(R%d)+", register_);
        break;
    case 3: { /* autoincrement deferred */
        pyint pointer = Registers_get(self->reg, register_);
        address = Ram_read_word(self->ram, pointer);
        operand = ram_read(self->ram, address);
        Registers_set(self->reg, register_, Registers_get(self->reg, register_) + 2);
        sprintf(r.assembly, "@(R%d)+", register_);
        break;
    }
    case 4:  /* autodecrement direct */
        Registers_set(self->reg, register_, Registers_get(self->reg, register_) - increment);
        address = Registers_get(self->reg, register_);
        operand = ram_read(self->ram, address);
        sprintf(r.assembly, "-(R%d)", register_);
        break;
    case 5: { /* autodecrement deferred */
        Registers_set(self->reg, register_, Registers_get(self->reg, register_) - 2);
        pyint pointer = Registers_get(self->reg, register_);
        address = Ram_read_word(self->ram, pointer);
        operand = ram_read(self->ram, address);
        sprintf(r.assembly, "@-(R%d)", register_);
        break;
    }
    case 6: { /* index: X, stored in the word after the instruction, is added to the register */
        pyint x = Ram_read_word_from_pc(self->ram);
        address = address_offset(Registers_get(self->reg, register_), x);
        operand = ram_read(self->ram, address);
        oct6(r.operand_word, x);
        sprintf(r.assembly, "%s(R%d)", formatted_offset(x_text, x), register_);
        break;
    }
    case 7: { /* index deferred */
        pyint x = Ram_read_word_from_pc(self->ram);
        pyint pointer = address_offset(Registers_get(self->reg, register_), x);
        address = Ram_read_word(self->ram, pointer);
        operand = ram_read(self->ram, address);
        oct6(r.operand_word, x);
        sprintf(r.assembly, "@%s(R%d)", formatted_offset(x_text, x), register_);
        break;
    }
    }

    r.operand = operand;
    r.register_ = register_;
    r.address = address;
    r.addressmode = addressmode;
    return r;
}

/* https://retrocomputing.stackexchange.com/questions/9248/pdp-11-jmp-and-jsr-how-was-the-target-specified
 * Mode 0 - illegal
 * Mode 1, Register Indirect, jumps to wherever the register points - JMP (R1)
 * Mode 2 (Autoincrement) becomes immediate (Not useful for JMP/JSR) -  ADC #label
 * Mode 3 (Autoincrement Indirect) becomes Absolute - JMP @#label
 * Mode 6 (Indexed) is Relative -  JMP label;  JMP 20(PC)
 * Mode 7, Index Indirect - JMP @20(PC) */
AMJmp AddressModes_addressing_mode_jmp(AddressModes *self, pyint mode_register)
{
    /* See page A-11 of pdp11/40.pdf */
    AMJmp r;
    memset(&r, 0, sizeof r);
    int addressmode = (int)py_shr(mode_register & 070, 3);
    int register_ = (int)(mode_register & 007);
    pyint address = 0;
    pyint jump_address = 0;
    char x_text[PYSTR];

    r.run = 1;
    switch (addressmode) {
    case 0:
        /* JMP direct illegal register address; halt.  *** this should call a trap */
        jump_address = 0;
        r.run = 0;
        sprintf(r.assembly, "R%d", register_);
        break;
    case 1:
        jump_address = Registers_get(self->reg, register_);
        sprintf(r.assembly, "@R%d", register_);
        break;
    case 2:
        jump_address = Registers_get(self->reg, register_);
        Registers_set(self->reg, register_, Registers_get(self->reg, register_) + 2);
        sprintf(r.assembly, "(R%d)+", register_);
        break;
    case 3:
        /* jumps to address contained in a word addressed by the register
         * and increments the register by two - JMP @(R1)+ */
        address = Registers_get(self->reg, register_);
        jump_address = Ram_read_word(self->ram, address);
        Registers_set(self->reg, register_, Registers_get(self->reg, register_) + 2);
        sprintf(r.assembly, "@(R%d)+", register_);
        break;
    case 4:
        /* register is decremented before being used as the address of the operand */
        Registers_set(self->reg, register_, Registers_get(self->reg, register_) - 2);
        address = Registers_get(self->reg, register_);
        jump_address = Ram_read_word(self->ram, address);
        sprintf(r.assembly, "-(R%d)", register_);
        break;
    case 5: {
        /* register is decremented before being used as the pointer to the address */
        Registers_set(self->reg, register_, Registers_get(self->reg, register_) - 2);
        pyint pointer = Registers_get(self->reg, register_);
        address = Ram_read_word(self->ram, pointer);
        jump_address = Ram_read_word(self->ram, address);
        sprintf(r.assembly, "@-(R%d)", register_);
        break;
    }
    case 6: {
        /* The expression E, plus the contents of the PC, yield the effective jump address. */
        pyint x = Ram_read_word_from_pc(self->ram);
        address = Registers_get(self->reg, register_);
        jump_address = address_offset(address, x);
        oct6(r.operand_word, x);
        sprintf(r.assembly, "%s(R%d)", formatted_offset(x_text, x), register_);
        break;
    }
    case 7: {
        /* The expression E, plus the contents of the PC,
         * yield a pointer to the effective address of the operand. */
        pyint x = Ram_read_word_from_pc(self->ram);
        pyint pointer = address_offset(Registers_get(self->reg, register_), x);
        address = Ram_read_word(self->ram, pointer);
        pyint word = Ram_read_word(self->ram, address);
        jump_address = Registers_get_pc(self->reg) + word;
        oct6(r.operand_word, x);
        sprintf(r.assembly, "@%s(R%d)", formatted_offset(x_text, x), register_);
        break;
    }
    }
    r.jump_address = jump_address;
    return r;
}

void AddressModes_addressing_mode_set(AddressModes *self, char b, int addressmode, pyint result, int register_, pyint address)
{
    /* copy the result into the register or address specified */
    if (addressmode == 0) {
        Registers_set(self->reg, register_, result);
    } else {
        if (b == 'B')
            Ram_write_byte(self->ram, address, result);
        else
            Ram_write_word(self->ram, address, result);
    }
}

void AddressModes_address_mode_report(AddressModes *self)
{
    logging_info("address modes used:");
    for (int addressmode = 0; addressmode < 8; addressmode++)
        logging_info("%d:%lld", addressmode, (long long)self->address_modes_used[addressmode]);
}
