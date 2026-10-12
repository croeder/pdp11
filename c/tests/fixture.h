/* fixture.h - the class attributes most test files share:
 *
 *   reg = reg()
 *   ram = Ram(threading.Lock(), reg, 16)
 *   psw = PSW(ram)
 *   stack = Stack(reg, ram, psw)
 *   am = am(reg, ram, psw)
 *   sw = sw()
 *   br_ops, cc_ops, noopr_ops, other_ops, rss_ops, ss_ops, ssdd_ops
 */
#ifndef FIXTURE_H
#define FIXTURE_H

#include "pdp11_br_ops.h"
#include "pdp11_cc_ops.h"
#include "pdp11_hardware.h"
#include "pdp11_noopr_ops.h"
#include "pdp11_other_ops.h"
#include "pdp11_rss_ops.h"
#include "pdp11_ss_ops.h"
#include "pdp11_ssdd_ops.h"
#include "stopwatches.h"

static Lock fx_lock;
static Registers reg;
static Ram ram;
static PSW psw;
static Stack stack;
static AddressModes am;
static StopWatches sw;
static br_ops br;
static cc_ops cc;
static noopr_ops noopr;
static other_ops other;
static rss_ops rss;
static ss_ops ss;
static ssdd_ops ssdd;

static void fixture_init(void)
{
    Lock_init(&fx_lock);
    Registers_init(&reg);
    Ram_init(&ram, &fx_lock, &reg, 16);
    PSW_init(&psw, &ram);
    Stack_init(&stack, &reg, &ram, &psw);
    AddressModes_init(&am, &reg, &ram, &psw);
    StopWatches_init(&sw);
    br_ops_init(&br, &reg, &ram, &psw, &sw);
    cc_ops_init(&cc, &psw, &sw);
    noopr_ops_init(&noopr, &reg, &ram, &psw, &stack, &sw);
    other_ops_init(&other, &reg, &ram, &psw, &am, &sw);
    rss_ops_init(&rss, &reg, &ram, &psw, &am, &sw);
    ss_ops_init(&ss, &reg, &ram, &psw, &am, &sw);
    ssdd_ops_init(&ssdd, &reg, &ram, &psw, &am, &sw);
}

/* psw.get_nzvc() as a string, for assertions */
static inline const char *nzvc(void)
{
    static char buf[8];
    return PSW_get_nzvc(&psw, buf);
}

#endif
