/*-
 * Copyright (c) 2011 Marcel Moolenaar
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_comconsole.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/interrupt.h>
#include <sys/kdb.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/tty.h>
#include <machine/resource.h>
#include <machine/sal.h>
#include <machine/sgisn.h>

struct sncon_softc {
	device_t	sc_dev;
	struct tty	*sc_tp;
	struct resource *sc_ires;
	void		*sc_icookie;
	void		*sc_softih;
	int		sc_irid;
	int		sc_altbrk;
};

static char sncon_name[] = "sncon";
static int sncon_is_console = 0;

/*
 * Low-level console section.
 */

static int  sncon_cngetc(struct consdev *);
static void sncon_cninit(struct consdev *);
static void sncon_cnprobe(struct consdev *);
static void sncon_cnputc(struct consdev *, int);
static void sncon_cnterm(struct consdev *);

CONSOLE_DRIVER(sncon);

static int
sncon_cngetc(struct consdev *cp)
{
	struct ia64_sal_result r;

	r = ia64_sal_entry(SAL_SGISN_POLL, 0, 0, 0, 0, 0, 0, 0);
	if (r.sal_status || r.sal_result[0] == 0)
		return (-1);

	r = ia64_sal_entry(SAL_SGISN_GETC, 0, 0, 0, 0, 0, 0, 0);
	return ((!r.sal_status) ? r.sal_result[0] : -1);
}

static void
sncon_cninit(struct consdev *cp)
{

	sncon_is_console = 1;
}

static void
sncon_cnprobe(struct consdev *cp)
{
	struct ia64_sal_result r;

	r = ia64_sal_entry(SAL_SGISN_SN_INFO, 0, 0, 0, 0, 0, 0, 0);
	if (r.sal_status != 0)
		return;

	strcpy(cp->cn_name, "sncon0");
	cp->cn_pri = CN_INTERNAL;
}

static void
sncon_cnputc(struct consdev *cp, int c)
{
	struct ia64_sal_result r;
	char buf[1];

	buf[0] = c;
	r = ia64_sal_entry(SAL_SGISN_TXBUF, (uintptr_t)buf, 1, 0, 0, 0, 0, 0);
}

static void
sncon_cnterm(struct consdev *cp)
{

	sncon_is_console = 0;
}

/*
 * TTY section.
 */

static void sncon_tty_close(struct tty *);
static void sncon_tty_free(void *);
static void sncon_tty_inwakeup(struct tty *);
static int  sncon_tty_ioctl(struct tty *, u_long, caddr_t, struct thread *);
static int  sncon_tty_modem(struct tty *, int, int);
static int  sncon_tty_open(struct tty *);
static void sncon_tty_outwakeup(struct tty *);
static int  sncon_tty_param(struct tty *, struct termios *);

static struct ttydevsw sncon_tty_class = {
	.tsw_flags	= TF_INITLOCK|TF_CALLOUT,
	.tsw_open	= sncon_tty_open,
	.tsw_close	= sncon_tty_close,
	.tsw_outwakeup	= sncon_tty_outwakeup,
	.tsw_inwakeup	= sncon_tty_inwakeup,
	.tsw_ioctl	= sncon_tty_ioctl,
	.tsw_param	= sncon_tty_param,
	.tsw_modem	= sncon_tty_modem,
	.tsw_free	= sncon_tty_free,
};

static void
sncon_tty_close(struct tty *tp)
{
}

static void
sncon_tty_free(void *arg)
{
}

static void
sncon_tty_inwakeup(struct tty *tp)
{
	struct sncon_softc *sc;

	sc = tty_softc(tp);
	/*
	 * Re-start reception.
	 */
}

static int
sncon_tty_ioctl(struct tty *tp, u_long cmd, caddr_t data, struct thread *td)
{

	return (ENOTTY);
}

static int
sncon_tty_modem(struct tty *tp, int biton, int bitoff)
{

	return (0);
}

static int
sncon_tty_open(struct tty *tp)
{

	return (0);
}

static void
sncon_tty_outwakeup(struct tty *tp)
{
	struct sncon_softc *sc;

	sc = tty_softc(tp);
	/*
	 * Re-start transmission.
	 */
}

static int
sncon_tty_param(struct tty *tp, struct termios *t)
{

	t->c_ispeed = t->c_ospeed = 9600;
	t->c_cflag |= CLOCAL;
	t->c_cflag &= ~HUPCL;
	return (0);
}

/*
 * Device section.
 */

static int  sncon_attach(device_t);
static int  sncon_detach(device_t);
static int  sncon_probe(device_t);

static device_method_t sncon_methods[] = {
	DEVMETHOD(device_attach,	sncon_attach),
	DEVMETHOD(device_detach,	sncon_detach),
	DEVMETHOD(device_probe,		sncon_probe),
	{ 0, 0 }
};

static driver_t sncon_driver = {
	sncon_name,
	sncon_methods,
	sizeof(struct sncon_softc),
};

static devclass_t sncon_devclass;

DRIVER_MODULE(sncon, shub, sncon_driver, sncon_devclass, 0, 0);

static void
sncon_rx_intr(void *arg)
{
	struct sncon_softc *sc = arg;
	struct ia64_sal_result r;
	struct tty *tp;
	int ch, count;

	count = 0;
	tp = sc->sc_tp;
	tty_lock(tp);
	do {
		r = ia64_sal_entry(SAL_SGISN_POLL, 0, 0, 0, 0, 0, 0, 0);
		if (r.sal_status || r.sal_result[0] == 0)
			break;

		r = ia64_sal_entry(SAL_SGISN_GETC, 0, 0, 0, 0, 0, 0, 0);
		if (r.sal_status != 0)
			break;

		ch = r.sal_result[0];

#if defined(KDB) && defined(ALT_BREAK_TO_DEBUGGER)
		do {
			int kdb;
			kdb = kdb_alt_break(ch, &sc->sc_altbrk);
			if (kdb != 0) {
				switch (kdb) {
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
		} while (0);
#endif

		ttydisc_rint(tp, ch, 0);
		count++;
	} while (count < 128);
	if (count > 0)
		ttydisc_rint_done(tp);
	tty_unlock(tp);

	/* Acknowledge handling of Shub event. */
	BUS_WRITE_IVAR(device_get_parent(sc->sc_dev), sc->sc_dev,
	    SHUB_IVAR_EVENT, SHUB_EVENT_CONSOLE);
}

static void
sncon_tx_intr(void *arg)
{
}

static int
sncon_attach(device_t dev)
{
	struct ia64_sal_result r;
	struct sncon_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	do {
		sc->sc_irid = 0;
		sc->sc_ires = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &sc->sc_irid, RF_ACTIVE | RF_SHAREABLE);
		if (sc->sc_ires == NULL)
			break;

		error = bus_setup_intr(dev, sc->sc_ires,
		    INTR_TYPE_TTY | INTR_MPSAFE, NULL, sncon_rx_intr, sc,
		    &sc->sc_icookie);
		if (error) {
			device_printf(dev, "could not activate interrupt\n");
			bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irid,
			    sc->sc_ires);
			sc->sc_ires = NULL;
		}
	} while (0);

	/* Enable or disable RX interrupts appropriately. */
	r = ia64_sal_entry(SAL_SGISN_CON_INTR, 2,
	    (sc->sc_ires != NULL) ? 1 : 0, 0, 0, 0, 0, 0);

	swi_add(&tty_intr_event, sncon_name, sncon_tx_intr, sc, SWI_TTY,
	    INTR_TYPE_TTY, &sc->sc_softih);

	sc->sc_tp = tty_alloc(&sncon_tty_class, sc);
	if (sncon_is_console)
		tty_init_console(sc->sc_tp, 0);
	tty_makedev(sc->sc_tp, NULL, "s0");
	return (0);
}

static int
sncon_detach(device_t dev)
{
	struct ia64_sal_result r;
	struct sncon_softc *sc;
	struct tty *tp;

	sc = device_get_softc(dev);
	tp = sc->sc_tp;

	tty_lock(tp);
	swi_remove(sc->sc_softih);
	tty_rel_gone(tp);

	if (sc->sc_ires != NULL) {
		/* Disable RX interrupts. */
		r = ia64_sal_entry(SAL_SGISN_CON_INTR, 2, 0, 0, 0, 0, 0, 0);

		bus_teardown_intr(dev, sc->sc_ires, sc->sc_icookie);
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irid,
		    sc->sc_ires);
	}
	return (0);
}

static int
sncon_probe(device_t dev)
{
	struct ia64_sal_result r;
	int error;
 
	r = ia64_sal_entry(SAL_SGISN_SN_INFO, 0, 0, 0, 0, 0, 0, 0);
	if (r.sal_status != 0)
		return (ENXIO);

	error = bus_set_resource(dev, SYS_RES_IRQ, 0, 0xe9, 1);
	if (error) {
		device_printf(dev, "Can't set IRQ (error=%d)\n", error);
		return (error);
	}
	device_set_desc_copy(dev, "SGI L1 console");
	return (0);
}
