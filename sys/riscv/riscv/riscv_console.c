/*-
 * Copyright (c) 2015-2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/tty.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <dev/ofw/openfirm.h>
#include <ddb/ddb.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/asm.h>
#include <machine/trap.h>
#include <machine/vmparam.h>
#include <machine/sbi.h>

static struct resource_spec rcons_spec[] = {
	{ SYS_RES_IRQ,		0,	RF_ACTIVE | RF_SHAREABLE},
	{ -1, 0 }
};

/* bus softc */
struct rcons_softc {
	struct resource		*res[1];
	void			*ihl[1];
	device_t		dev;
};

/* CN Console interface */

static tsw_outwakeup_t riscvtty_outwakeup;

static struct ttydevsw riscv_ttydevsw = {
	.tsw_flags	= TF_NOPREFIX,
	.tsw_outwakeup	= riscvtty_outwakeup,
};

static int			polltime;
static struct callout		riscv_callout;
static struct tty 		*tp = NULL;

#if defined(KDB)
static int			alt_break_state;
#endif

static void	riscv_timeout(void *);

static cn_probe_t	riscv_cnprobe;
static cn_init_t	riscv_cninit;
static cn_term_t	riscv_cnterm;
static cn_getc_t	riscv_cngetc;
static cn_putc_t	riscv_cnputc;
static cn_grab_t	riscv_cngrab;
static cn_ungrab_t	riscv_cnungrab;

CONSOLE_DRIVER(riscv);

#define	MAX_BURST_LEN		1
#define	QUEUE_SIZE		256

struct queue_entry {
	uint64_t data;
	uint64_t used;
	struct queue_entry *next;
};

struct queue_entry cnqueue[QUEUE_SIZE];
struct queue_entry *entry_last;
struct queue_entry *entry_served;

static void
riscv_putc(int c)
{

	sbi_console_putchar(c);
}

#ifdef EARLY_PRINTF
early_putc_t *early_putc = riscv_putc;
#endif

static void
cn_drvinit(void *unused)
{

	if (riscv_consdev.cn_pri != CN_DEAD &&
	    riscv_consdev.cn_name[0] != '\0') {
		tp = tty_alloc(&riscv_ttydevsw, NULL);
		tty_init_console(tp, 0);
		tty_makedev(tp, NULL, "%s", "rcons");

		polltime = 1;

		callout_init(&riscv_callout, 1);
		callout_reset(&riscv_callout, polltime, riscv_timeout, NULL);
	}
}

SYSINIT(cndev, SI_SUB_CONFIGURE, SI_ORDER_MIDDLE, cn_drvinit, NULL);

static void
riscvtty_outwakeup(struct tty *tp)
{
	u_char buf[MAX_BURST_LEN];
	int len;
	int i;

	for (;;) {
		len = ttydisc_getc(tp, buf, sizeof(buf));
		if (len == 0)
			break;

		KASSERT(len == 1, ("tty error"));

		for (i = 0; i < len; i++)
			riscv_putc(buf[i]);
	}
}

static void
riscv_timeout(void *v)
{
	int c;

	tty_lock(tp);
	while ((c = riscv_cngetc(NULL)) != -1)
		ttydisc_rint(tp, c, 0);
	ttydisc_rint_done(tp);
	tty_unlock(tp);

	callout_reset(&riscv_callout, polltime, riscv_timeout, NULL);
}

static void
riscv_cnprobe(struct consdev *cp)
{

	cp->cn_pri = CN_NORMAL;
}

static void
riscv_cninit(struct consdev *cp)
{
	int i;

	strcpy(cp->cn_name, "rcons");

	for (i = 0; i < QUEUE_SIZE; i++) {
		if (i == (QUEUE_SIZE - 1))
			cnqueue[i].next = &cnqueue[0];
		else
			cnqueue[i].next = &cnqueue[i+1];
		cnqueue[i].data = 0;
		cnqueue[i].used = 0;
	}

	entry_last = &cnqueue[0];
	entry_served = &cnqueue[0];
}

static void
riscv_cnterm(struct consdev *cp)
{

}

static void
riscv_cngrab(struct consdev *cp)
{

}

static void
riscv_cnungrab(struct consdev *cp)
{

}

static int
riscv_cngetc(struct consdev *cp)
{
	uint8_t data;
	int ch;

#if defined(KDB)
	/*
	 * RISCVTODO: BBL polls for console data on timer interrupt,
	 * but interrupts are turned off in KDB.
	 * So we currently do not have console in KDB.
	 */
	if (kdb_active) {
		ch = sbi_console_getchar();
		while (ch) {
			entry_last->data = ch;
			entry_last->used = 1;
			entry_last = entry_last->next;

			ch = sbi_console_getchar();
		}
	}
#endif

	if (entry_served->used == 1) {
		data = entry_served->data;
		entry_served->used = 0;
		entry_served = entry_served->next;
		ch = (data & 0xff);
		if (ch > 0 && ch < 0xff) {
#if defined(KDB)
			kdb_alt_break(ch, &alt_break_state);
#endif
			return (ch);
		}
	}

	return (-1);
}

static void
riscv_cnputc(struct consdev *cp, int c)
{

	riscv_putc(c);
}

/* Bus interface */

static int
rcons_intr(void *arg)
{
	int c;

	c = sbi_console_getchar();
	if (c > 0 && c < 0xff) {
		entry_last->data = c;
		entry_last->used = 1;
		entry_last = entry_last->next;
	}

	csr_clear(sip, SIP_SSIP);

	return (FILTER_HANDLED);
}

static int
rcons_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "riscv,console"))
		return (ENXIO);

	device_set_desc(dev, "RISC-V console");
	return (BUS_PROBE_DEFAULT);
}

static int
rcons_attach(device_t dev)
{
	struct rcons_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, rcons_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Setup IRQs handler */
	error = bus_setup_intr(dev, sc->res[0], INTR_TYPE_CLK,
	    rcons_intr, NULL, sc, &sc->ihl[0]);
	if (error) {
		device_printf(dev, "Unable to alloc int resource.\n");
		return (ENXIO);
	}

	csr_set(sie, SIE_SSIE);

	bus_generic_attach(sc->dev);

	sbi_console_getchar();

	return (0);
}

static device_method_t rcons_methods[] = {
	DEVMETHOD(device_probe,		rcons_probe),
	DEVMETHOD(device_attach,	rcons_attach),

	DEVMETHOD_END
};

static driver_t rcons_driver = {
	"rcons",
	rcons_methods,
	sizeof(struct rcons_softc)
};

static devclass_t rcons_devclass;

DRIVER_MODULE(rcons, simplebus, rcons_driver, rcons_devclass, 0, 0);
