/*-
 * Copyright (C) 2006 Kip Macy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Kip Macy ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL Kip Macy BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/priv.h>
#include <sys/rman.h>
#include <sys/tty.h>

#include <machine/mdesc_bus.h>
#include <machine/cddl/mdesc.h>

#include "mdesc_bus_if.h"

#include "opt_simulator.h"

#include <machine/resource.h>
#include <machine/hypervisorvar.h>
#include <machine/hv_api.h>

#define HVCN_POLL_FREQ 10

static tsw_open_t	hvcn_open;
static tsw_outwakeup_t	hvcn_outwakeup;
static tsw_close_t	hvcn_close;

static struct ttydevsw hvcn_class = {
	.tsw_open	= hvcn_open,
	.tsw_outwakeup	= hvcn_outwakeup,
	.tsw_close	= hvcn_close,
};

#define PCBURST 16
static struct tty		*hvcn_tp = NULL;
static struct resource          *hvcn_irq;
static void                     *hvcn_intrhand;

static int bufindex;
static int buflen;
static u_char buf[PCBURST];
static int			polltime;
static struct callout_handle	hvcn_timeouthandle
    = CALLOUT_HANDLE_INITIALIZER(&hvcn_timeouthandle);

#if defined(KDB)
static int			alt_break_state;
#endif

static void     hvcn_timeout(void *);

static cn_probe_t	hvcn_cnprobe;
static cn_init_t	hvcn_cninit;
static cn_getc_t	hvcn_cngetc;
static cn_putc_t	hvcn_cnputc;
static cn_term_t	hvcn_cnterm;


CONSOLE_DRIVER(hvcn);

void
hv_cnputs(char *p)
{
	int c, error;

	while ((c = *p++) != '\0') {
		if (c == '\n') {
			do {
				error = hv_cons_putchar('\r');
			} while (error == H_EWOULDBLOCK);
		}
		do {
			error = hv_cons_putchar(c);
		} while (error == H_EWOULDBLOCK);
	}
}

static int
hvcn_open(struct tty *tp)
{

	/*
	 * Set up timeout to trigger fake interrupts to transmit
	 * trailing data.
	 */
	polltime = hz / HVCN_POLL_FREQ;
	if (polltime < 1)
		polltime = 1;
	hvcn_timeouthandle = timeout(hvcn_timeout, tp, polltime);

	buflen = 0;

	return (0);
}
 
static void
hvcn_close(struct tty *tp)
{
	untimeout(hvcn_timeout, tp, hvcn_timeouthandle);
}

static void
hvcn_cnprobe(struct consdev *cp)
{

#if 0
	char name[64];

	node = OF_peer(0);
	if (node == -1)
		panic("%s: OF_peer failed.", __func__);
	
	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		OF_getprop(node, "name", name, sizeof(name));
		if (!strcmp(name, "virtual-devices"))
			break;
	}
	
	if (node == 0)
		goto done;

	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		OF_getprop(node, "name", name, sizeof(name));
		if (!strcmp(name, "console"))
			break;
	}
done:
#endif	
	cp->cn_pri = CN_NORMAL;

}

static void
hvcn_cninit(struct consdev *cp)
{

	strcpy(cp->cn_name, "hvcn");
}

static int
hvcn_cngetc(struct consdev *cp)
{
	unsigned char ch;
	int l;

	ch = '\0';

	while ((l = hv_cons_getchar(&ch)) != H_EOK) {
#if defined(KDB)
		int kdb_brk;

		if (l == H_BREAK || l ==  H_HUP)
			kdb_enter(KDB_WHY_BREAK, "Break sequence on console");

		if ((kdb_brk = kdb_alt_break(ch, &alt_break_state)) != 0) {
			switch (kdb_brk) {
			case KDB_REQ_DEBUGGER:
				kdb_enter(KDB_WHY_BREAK,
				    "Break sequence on console");
				break;
			case KDB_REQ_PANIC:
				kdb_panic("Panic sequence on console");
				break;
			case KDB_REQ_REBOOT:
				kdb_reboot();
				break;
			}
		}
#endif
		if (l != -2 && l != 0) {
			return (-1);
		}
	}



	return (ch);
}

static int
hvcn_cncheckc(struct consdev *cp)
{
	unsigned char ch;
	int l;

	if ((l = hv_cons_getchar(&ch)) == H_EOK) {
#if defined(KDB)
		if (l == H_BREAK || l ==  H_HUP)
			kdb_enter(KDB_WHY_BREAK, "Break sequence on console");
		if (kdb_alt_break(ch, &alt_break_state))
			kdb_enter(KDB_WHY_BREAK, "Break sequence on console");
#endif
		return (ch);
	}

	return (-1);
}


static void
hvcn_cnterm(struct consdev *cp)
{
	;
}

static void
hvcn_cnputc(struct consdev *cp, int c)
{

	int error;

	error = 0;
	do {
		if (c == '\n') 
			error = hv_cons_putchar('\r');
	} while (error == H_EWOULDBLOCK);
	do {
		error = hv_cons_putchar(c);
	} while (error == H_EWOULDBLOCK);
}

static void
hvcn_outwakeup(struct tty *tp)
{

	for (;;) {
		/* Refill the input buffer. */
		if (buflen == 0) {
			buflen = ttydisc_getc(tp, buf, PCBURST);
			bufindex = 0;
		}

		/* Transmit the input buffer. */
		while (buflen) {
			if (hv_cons_putchar(buf[bufindex]) == H_EWOULDBLOCK)
				return;
			bufindex++;
			buflen--;
		}
	}
}

static void
hvcn_intr(void *v)
{
	struct tty *tp = v;
	int c;

	tty_lock(tp);
	
	/* Receive data. */
	while ((c = hvcn_cncheckc(NULL)) != -1) 
		ttydisc_rint(tp, c, 0);
	ttydisc_rint_done(tp);

	/* Transmit trailing data. */
	hvcn_outwakeup(tp);
	tty_unlock(tp);
}

static void
hvcn_timeout(void *v)
{
	hvcn_intr(v);

	hvcn_timeouthandle = timeout(hvcn_timeout, v, polltime);
}


static int
hvcn_dev_probe(device_t dev)
{

	if (strcmp(mdesc_bus_get_name(dev), "console"))
		return (ENXIO);
	
	device_set_desc(dev, "sun4v virtual console");	

	return (0);
}


static int
hvcn_dev_attach(device_t dev)
{
      
	struct tty *tp;
	int error, rid;

	/* belongs in attach - but attach is getting called multiple times
	 * for reasons I have not delved into
	 */

	if (hvcn_consdev.cn_pri == CN_DEAD || 
	    hvcn_consdev.cn_name[0] == '\0') 
		return (ENXIO);

	tp = tty_alloc(&hvcn_class, NULL, NULL);
	tty_makedev(tp, NULL, "v%r", 1);
	tty_makealias(tp, "hvcn");
	
	rid = 0;

	if ((hvcn_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
					RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
		
	}
	error = bus_setup_intr(dev, hvcn_irq, INTR_TYPE_TTY, NULL, hvcn_intr, hvcn_tp, 
			       hvcn_intrhand);
	
	if (error)
		device_printf(dev, "couldn't set up irq\n");
	
		
fail:
	return (error);
      
}

static device_method_t hvcn_methods[] = {
        DEVMETHOD(device_probe, hvcn_dev_probe),
        DEVMETHOD(device_attach, hvcn_dev_attach),
        {0, 0}
};


static driver_t hvcn_driver = {
        "hvcn",
        hvcn_methods,
	0,
};


static devclass_t hvcn_devclass;

DRIVER_MODULE(hvcn, vnex, hvcn_driver, hvcn_devclass, 0, 0);
