/* $FreeBSD$ */
/* $NetBSD: prom.c,v 1.3 1997/09/06 14:03:58 drochner Exp $ */

/*  
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <sys/types.h>

#include <machine/prom.h>
#include <machine/rpb.h>

#include "common.h"
#include "bootstrap.h"

int console;

static void prom_probe(struct console *cp);
static int prom_init(int);
void prom_putchar(int);
int prom_getchar(void);
int prom_poll(void);

struct console promconsole = {
    "prom",
    "SRM firmware console",
    0,
    prom_probe,
    prom_init,
    prom_putchar,
    prom_getchar,
    prom_poll,
};

void
init_prom_calls()
{
    extern struct prom_vec prom_dispatch_v;
    struct rpb *r;
    struct crb *c;
    char buf[4];

    r = (struct rpb *)HWRPB_ADDR;
    c = (struct crb *)((u_int8_t *)r + r->rpb_crb_off);

    prom_dispatch_v.routine_arg = c->crb_v_dispatch;
    prom_dispatch_v.routine = c->crb_v_dispatch->entry_va;

    /* Look for console tty. */
    prom_getenv(PROM_E_TTY_DEV, buf, 4);
    console = buf[0] - '0';
}

static void
prom_probe(struct console *cp)
{
    init_prom_calls();
    cp->c_flags |= C_PRESENTIN|C_PRESENTOUT;
}

static int
prom_init(int arg)
{
    return 0;
}

void
prom_putchar(int c)
{
    prom_return_t ret;
    char cbuf;

    cbuf = c;
    do {
	ret.bits = prom_dispatch(PROM_R_PUTS, console, &cbuf, 1);
    } while ((ret.u.retval & 1) == 0);
}

static int saved_char = -1;

int
prom_getchar()
{
    prom_return_t ret;

    if (saved_char != -1) {
	int c = saved_char;
	saved_char = -1;
	return c;
    }

    for (;;) {
	ret.bits = prom_dispatch(PROM_R_GETC, console);
	if (ret.u.status == 0 || ret.u.status == 1)
	    return (ret.u.retval);
    }
}

int
prom_poll()
{
    prom_return_t ret;

    if (saved_char != -1)
	return 1;

    ret.bits = prom_dispatch(PROM_R_GETC, console);
    if (ret.u.status == 0 || ret.u.status == 1) {
	saved_char = ret.u.retval;
	return 1;
    }

    return 0;
}

int
prom_getenv(id, buf, len)
    int id, len;
    char *buf;
{
    prom_return_t ret;

    ret.bits = prom_dispatch(PROM_R_GETENV, id, buf, len-1);
    if (ret.u.status & 0x4)
	ret.u.retval = 0;
    buf[ret.u.retval] = '\0';

    return (ret.u.retval);
}

int
prom_open(dev, len)
    char *dev;
    int len;
{
    prom_return_t ret;

    ret.bits = prom_dispatch(PROM_R_OPEN, dev, len);
    if (ret.u.status & 0x4)
	return (-1);
    else
	return (ret.u.retval);
}
