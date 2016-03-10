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

#include <machine/bus.h>
#include <machine/trap.h>

#include "htif.h"

#include <dev/ofw/openfirm.h>

#include <ddb/ddb.h>

extern uint64_t console_intr;

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
#define	CONSOLE_DEFAULT_ID	1ul
#define	SPIN_IN_MACHINE_MODE	1

struct queue_entry {
	uint64_t data;
	uint64_t used;
	struct queue_entry *next;
};

struct queue_entry cnqueue[QUEUE_SIZE];
struct queue_entry *entry_last;
struct queue_entry *entry_served;

static void
htif_putc(int c)
{
	uint64_t cmd;

	cmd = (HTIF_CMD_WRITE << HTIF_CMD_SHIFT);
	cmd |= (CONSOLE_DEFAULT_ID << HTIF_DEV_ID_SHIFT);
	cmd |= c;

#ifdef SPIN_IN_MACHINE_MODE
	machine_command(ECALL_HTIF_LOWPUTC, cmd);
#else
	htif_command(cmd);
#endif

}

static uint8_t
htif_getc(void)
{
	uint64_t cmd;
	uint8_t res;

	cmd = (HTIF_CMD_READ << HTIF_CMD_SHIFT);
	cmd |= (CONSOLE_DEFAULT_ID << HTIF_DEV_ID_SHIFT);

	res = htif_command(cmd);

	return (res);
}

static void
riscv_putc(int c)
{
	uint64_t counter;
	uint64_t *cc;
	uint64_t val;

	val = 0;
	counter = 0;

	cc = (uint64_t*)&console_intr;
	*cc = 0;

	htif_putc(c);

#ifndef SPIN_IN_MACHINE_MODE
	/* Wait for an interrupt */
	__asm __volatile(
		"li	%0, 1\n"	/* counter = 1 */
		"slli	%0, %0, 12\n"	/* counter <<= 12 */
	"1:"
		"addi	%0, %0, -1\n"	/* counter -= 1 */
		"beqz	%0, 2f\n"	/* counter == 0 ? finish */
		"ld	%1, 0(%2)\n"	/* val = *cc */
		"beqz	%1, 1b\n"	/* val == 0 ? repeat */
	"2:"
		: "=&r"(counter), "=&r"(val) : "r"(cc)
	);
#endif
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
#if defined(KDB)
	uint64_t devcmd;
	uint64_t entry;
	uint64_t devid;
#endif
	uint8_t data;
	int ch;

	htif_getc();

#if defined(KDB)
	if (kdb_active) {
		entry = machine_command(ECALL_HTIF_GET_ENTRY, 0);
		while (entry) {
			devid = HTIF_DEV_ID(entry);
			devcmd = HTIF_DEV_CMD(entry);
			data = HTIF_DEV_DATA(entry);

			if (devid == CONSOLE_DEFAULT_ID && devcmd == 0) {
				entry_last->data = data;
				entry_last->used = 1;
				entry_last = entry_last->next;
			} else {
				printf("Lost interrupt: devid %d\n",
				    devid);
			}

			entry = machine_command(ECALL_HTIF_GET_ENTRY, 0);
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

/*
 * Bus interface.
 */

struct htif_console_softc {
	device_t	dev;
	int		running;
	int		intr_chan;
	int		cmd_done;
	int		curtag;
	int		index;
};

static void
htif_console_intr(void *arg, uint64_t entry)
{
	struct htif_console_softc *sc;
	uint8_t devcmd;
	uint64_t data;

	sc = arg;

	devcmd = HTIF_DEV_CMD(entry);
	data = HTIF_DEV_DATA(entry);

	if (devcmd == 0) {
		entry_last->data = data;
		entry_last->used = 1;
		entry_last = entry_last->next;
	}
}

static int
htif_console_probe(device_t dev)
{

	return (0);
}

static int
htif_console_attach(device_t dev)
{
	struct htif_console_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->index = htif_get_index(dev);
	if (sc->index < 0)
		return (EINVAL);

	htif_setup_intr(sc->index, htif_console_intr, sc);

	return (0);
}

static device_method_t htif_console_methods[] = {
	DEVMETHOD(device_probe,		htif_console_probe),
	DEVMETHOD(device_attach,	htif_console_attach),
	DEVMETHOD_END
};

static driver_t htif_console_driver = {
	"htif_console",
	htif_console_methods,
	sizeof(struct htif_console_softc)
};

static devclass_t htif_console_devclass;

DRIVER_MODULE(htif_console, htif, htif_console_driver,
    htif_console_devclass, 0, 0);
