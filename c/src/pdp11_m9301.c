/* pdp11_m9301.c - translation of pdp11_m9301.py
 * Emulates PDP11 M9301 boot ROM card.
 * 7.3 Microswitch Settings. See p. 7-2 of
 * PDP11 M9301 bootstrap/terminator module maintenance and operator's manual
 * https://gunkies.org/wiki/M9301_ROM
 *
 * M9301-YA - PDP-11/04 and PDP-11/34 OEM version; basic diagnostics,
 *   console emulator, boots from various devices.
 * M9301-YB - /04 and /34 end user version.
 * M9301-YF - All models.
 * M9301-YH - /60 and /70 version. */
#include "pdp11_m9301.h"
#include "pdp11_logger.h"

static pyint io_read_dip_switches(void *device) { return M9301_read_dip_switches(device); }

void M9301_init(M9301 *self, Registers *reg, Ram *ram, pdp11Boot *boot)
{
    logging_info("initializing M9301");
    self->ram = ram;

    self->base_address = 0173000;
    self->switch_address = 0173024;
    self->switch_settings = 0;

    /* switch81_1 = True: low rom enable ON for normal operation. */

    int switch81_2 = 1;  /* power up reboot enable ON = True
     * If this is on then processor will trap to a selectable address.
     * Switches 3-10 set the address.  If off, processor traps to location 24.
     * The new PC will be the logical OR of the contents of ROM location 773024
     * and the eight microswitches on the M9301-YA.
     *
     * Entry point -- Console emulator w/ CPU Diagnostics 1-5
     * DIP switches xx11111111  (173000)
     * Entry point -- Console Emulator
     * DIP switches xx11110011  (173030)
     * Entry point -- RK11 Boot w/ Diagnostic 1-5
     * DIP switches xx01101111  (173440)
     * Entry point -- DL11 Boot w/ Diagnostic 1-5
     * DIP switches xx00101011  (173650) */

    /* Address switches at 0o173024.  Configure by setting each to on or off. */
    pyint on = 0000000;   /* 0 bits in switches */
    /* pyint off = 0000777;   1 bits in switches */
    pyint switch81_3 = 00400 & on;   /* bus address bit 8 */
    pyint switch81_4 = 00200 & on;   /* bus address bit 7 */
    pyint switch81_5 = 00100 & on;   /* bus address bit 6 */
    pyint switch81_6 = 00040 & on;   /* bus address bit 5 */
    pyint switch81_7 = 00020 & on;   /* bus address bit 4 */
    pyint switch81_8 = 00010 & on;   /* bus address bit 3 */
    pyint switch81_9 = 00004 & on;   /* bus address bit 2 */
    pyint switch81_10 = 00002 & on;  /* bus address bit 1 */
    /* None of the switches on results in CPU Diagnostics with Console Emulator. */
    if (switch81_2) {
        /* Load boot rom image into RAM (165000 - 173776) */
        pdp11Boot_read_pdp11_assembly_file(boot, "source/M9301-YA.txt");
        self->switch_settings = switch81_3 + switch81_4 + switch81_5 + switch81_6 +
                                switch81_7 + switch81_8 + switch81_9 + switch81_10 + self->base_address;
        char a[PYSTR], b[PYSTR];
        logging_info("switch_address:%s switch_settings:%s", py_oct(a, self->switch_address), py_oct(b, self->switch_settings));
        Registers_set_pc(reg, self->switch_settings, "M9301 switch81_2 True");
    } else {
        Registers_set_pc(reg, 024, "M9301 else switch81_2 False");
    }

    /* Register address 173024 like an io device */
    Ram_register_io_reader(self->ram, self->switch_address, self, io_read_dip_switches, "read_dip_switches");

    char a[PYSTR];
    logging_info("initializing M9301 done. PC:%s", py_oct(a, Registers_get_pc(reg)));
}

pyint M9301_read_dip_switches(M9301 *self)
{
    /* io device method returns the dip switch positions */
    char a[PYSTR];
    logging_info("                                  ; read_dip_switches returns %s", py_oct(a, self->switch_settings));
    return self->switch_settings;
}
