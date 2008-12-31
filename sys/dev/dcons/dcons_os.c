/*-
 * Copyright (C) 2003,2004
 * 	Hidetoshi Shimokawa. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $FreeBSD: src/sys/dev/dcons/dcons_os.c,v 1.19.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <sys/param.h>
#if __FreeBSD_version >= 502122
#include <sys/kdb.h>
#include <gdb/gdb.h>
#endif
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/tty.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/ucred.h>

#include <machine/bus.h>

#ifdef __DragonFly__
#include "dcons.h"
#include "dcons_os.h"
#else
#include <dev/dcons/dcons.h>
#include <dev/dcons/dcons_os.h>
#endif

#include <ddb/ddb.h>
#include <sys/reboot.h>

#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include "opt_comconsole.h"
#include "opt_dcons.h"
#include "opt_kdb.h"
#include "opt_gdb.h"
#include "opt_ddb.h"


#ifndef DCONS_POLL_HZ
#define DCONS_POLL_HZ	100
#endif

#ifndef DCONS_BUF_SIZE
#define DCONS_BUF_SIZE (16*1024)
#endif

#ifndef DCONS_FORCE_CONSOLE
#define DCONS_FORCE_CONSOLE	0	/* Mostly for FreeBSD-4/DragonFly */
#endif

#ifndef DCONS_FORCE_GDB
#define DCONS_FORCE_GDB	1
#endif

#if __FreeBSD_version >= 500101
#define CONS_NODEV	1
#if __FreeBSD_version < 502122
static struct consdev gdbconsdev;
#endif
#endif

static d_open_t		dcons_open;
static d_close_t	dcons_close;
#if defined(__DragonFly__) || __FreeBSD_version < 500104
static d_ioctl_t	dcons_ioctl;
#endif

static struct cdevsw dcons_cdevsw = {
#ifdef __DragonFly__
#define CDEV_MAJOR      184
	"dcons", CDEV_MAJOR, D_TTY, NULL, 0,
	dcons_open, dcons_close, ttyread, ttywrite, dcons_ioctl,
	ttypoll, nommap, nostrategy, nodump, nopsize,
#elif __FreeBSD_version >= 500104
	.d_version =	D_VERSION,
	.d_open =	dcons_open,
	.d_close =	dcons_close,
	.d_name =	"dcons",
	.d_flags =	D_TTY | D_NEEDGIANT,
#else
#define CDEV_MAJOR      184
	/* open */	dcons_open,
	/* close */	dcons_close,
	/* read */	ttyread,
	/* write */	ttywrite,
	/* ioctl */	dcons_ioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"dcons",
	/* major */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY,
#endif
};

#ifndef KLD_MODULE
static char bssbuf[DCONS_BUF_SIZE];	/* buf in bss */
#endif

/* global data */
static struct dcons_global dg;
struct dcons_global *dcons_conf;
static int poll_hz = DCONS_POLL_HZ;

static struct dcons_softc sc[DCONS_NPORT];

SYSCTL_NODE(_kern, OID_AUTO, dcons, CTLFLAG_RD, 0, "Dumb Console");
SYSCTL_INT(_kern_dcons, OID_AUTO, poll_hz, CTLFLAG_RW, &poll_hz, 0,
				"dcons polling rate");

static int drv_init = 0;
static struct callout dcons_callout;
struct dcons_buf *dcons_buf;		/* for local dconschat */

#ifdef __DragonFly__
#define DEV	dev_t
#define THREAD	d_thread_t
#elif __FreeBSD_version < 500000
#define DEV	dev_t
#define THREAD	struct proc
#else
#define DEV	struct cdev *
#define THREAD	struct thread
#endif


static void	dcons_tty_start(struct tty *);
static int	dcons_tty_param(struct tty *, struct termios *);
static void	dcons_timeout(void *);
static int	dcons_drv_init(int);

static cn_probe_t	dcons_cnprobe;
static cn_init_t	dcons_cninit;
static cn_term_t	dcons_cnterm;
static cn_getc_t	dcons_cngetc;
static cn_putc_t	dcons_cnputc;

CONSOLE_DRIVER(dcons);

#if defined(GDB) && (__FreeBSD_version >= 502122)
static gdb_probe_f dcons_dbg_probe;
static gdb_init_f dcons_dbg_init;
static gdb_term_f dcons_dbg_term;
static gdb_getc_f dcons_dbg_getc;
static gdb_putc_f dcons_dbg_putc;

GDB_DBGPORT(dcons, dcons_dbg_probe, dcons_dbg_init, dcons_dbg_term,
    dcons_dbg_getc, dcons_dbg_putc);

extern struct gdb_dbgport *gdb_cur;
#endif

#if (defined(GDB) || defined(DDB)) && defined(ALT_BREAK_TO_DEBUGGER)
static int
dcons_check_break(struct dcons_softc *dc, int c)
{
	if (c < 0)
		return (c);

#if __FreeBSD_version >= 502122
	if (kdb_alt_break(c, &dc->brk_state)) {
		if ((dc->flags & DC_GDB) != 0) {
#ifdef GDB
			if (gdb_cur == &dcons_gdb_dbgport) {
				kdb_dbbe_select("gdb");
				kdb_enter_why(KDB_WHY_BREAK,
				    "Break sequence on dcons gdb port");
			}
#endif
		} else
			kdb_enter_why(KDB_WHY_BREAK,
			    "Break sequence on dcons console port");
	}
#else
	switch (dc->brk_state) {
	case STATE1:
		if (c == KEY_TILDE)
			dc->brk_state = STATE2;
		else
			dc->brk_state = STATE0;
		break;
	case STATE2:
		dc->brk_state = STATE0;
		if (c == KEY_CTRLB) {
#if DCONS_FORCE_GDB
			if (dc->flags & DC_GDB)
				boothowto |= RB_GDB;
#endif
			breakpoint();
		}
	}
	if (c == KEY_CR)
		dc->brk_state = STATE1;
#endif
	return (c);
}
#else
#define	dcons_check_break(dc, c)	(c)
#endif

static int
dcons_os_checkc_nopoll(struct dcons_softc *dc)
{
	int c;

	if (dg.dma_tag != NULL)
		bus_dmamap_sync(dg.dma_tag, dg.dma_map, BUS_DMASYNC_POSTREAD);
  
	c = dcons_check_break(dc, dcons_checkc(dc));

	if (dg.dma_tag != NULL)
		bus_dmamap_sync(dg.dma_tag, dg.dma_map, BUS_DMASYNC_PREREAD);

	return (c);
}

static int
dcons_os_checkc(struct dcons_softc *dc)
{
	EVENTHANDLER_INVOKE(dcons_poll, 0);
	return (dcons_os_checkc_nopoll(dc));
}

#if defined(GDB) || !defined(CONS_NODEV)
static int
dcons_os_getc(struct dcons_softc *dc)
{
	int c;

	while ((c = dcons_os_checkc(dc)) == -1);

	return (c & 0xff);
} 
#endif

static void
dcons_os_putc(struct dcons_softc *dc, int c)
{
	if (dg.dma_tag != NULL)
		bus_dmamap_sync(dg.dma_tag, dg.dma_map, BUS_DMASYNC_POSTWRITE);

	dcons_putc(dc, c);

	if (dg.dma_tag != NULL)
		bus_dmamap_sync(dg.dma_tag, dg.dma_map, BUS_DMASYNC_PREWRITE);
}
static int
dcons_open(DEV dev, int flag, int mode, THREAD *td)
{
	struct tty *tp;
	int unit, error, s;

	unit = minor(dev);
	if (unit != 0)
		return (ENXIO);

	tp = dev->si_tty;
	tp->t_oproc = dcons_tty_start;
	tp->t_param = dcons_tty_param;
	tp->t_stop = nottystop;
	tp->t_dev = dev;

	error = 0;

	s = spltty();
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttyconsolemode(tp, 0);
	} else if ((tp->t_state & TS_XCLUDE) &&
	    priv_check(td, PRIV_TTY_EXCLUSIVE)) {
		splx(s);
		return (EBUSY);
	}
	splx(s);

#if __FreeBSD_version < 502113
	error = (*linesw[tp->t_line].l_open)(dev, tp);
#else
	error = ttyld_open(tp, dev);
#endif

	return (error);
}

static int
dcons_close(DEV dev, int flag, int mode, THREAD *td)
{
	int	unit;
	struct	tty *tp;

	unit = minor(dev);
	if (unit != 0)
		return (ENXIO);

	tp = dev->si_tty;
	if (tp->t_state & TS_ISOPEN) {
#if __FreeBSD_version < 502113
		(*linesw[tp->t_line].l_close)(tp, flag);
		ttyclose(tp);
#else
		ttyld_close(tp, flag);
		tty_close(tp);
#endif
	}

	return (0);
}

#if defined(__DragonFly__) || __FreeBSD_version < 500104
static int
dcons_ioctl(DEV dev, u_long cmd, caddr_t data, int flag, THREAD *td)
{
	int	unit;
	struct	tty *tp;
	int	error;

	unit = minor(dev);
	if (unit != 0)
		return (ENXIO);

	tp = dev->si_tty;
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, td);
	if (error != ENOIOCTL)
		return (error);

	error = ttioctl(tp, cmd, data, flag);
	if (error != ENOIOCTL)
		return (error);

	return (ENOTTY);
}
#endif

static int
dcons_tty_param(struct tty *tp, struct termios *t)
{
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return 0;
}

static void
dcons_tty_start(struct tty *tp)
{
	struct dcons_softc *dc;
	int s;

	dc = (struct dcons_softc *)tp->t_dev->si_drv1;
	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
		return;
	}

	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0)
		dcons_os_putc(dc, getc(&tp->t_outq));
	tp->t_state &= ~TS_BUSY;

	ttwwakeup(tp);
	splx(s);
}

static void
dcons_timeout(void *v)
{
	struct	tty *tp;
	struct dcons_softc *dc;
	int i, c, polltime;

	for (i = 0; i < DCONS_NPORT; i ++) {
		dc = &sc[i];
		tp = ((DEV)dc->dev)->si_tty;
		while ((c = dcons_os_checkc_nopoll(dc)) != -1)
			if (tp->t_state & TS_ISOPEN)
#if __FreeBSD_version < 502113
				(*linesw[tp->t_line].l_rint)(c, tp);
#else
				ttyld_rint(tp, c);
#endif
	}
	polltime = hz / poll_hz;
	if (polltime < 1)
		polltime = 1;
	callout_reset(&dcons_callout, polltime, dcons_timeout, tp);
}

static void
dcons_cnprobe(struct consdev *cp)
{
#ifdef __DragonFly__
	cp->cn_dev = make_dev(&dcons_cdevsw, DCONS_CON,
	    UID_ROOT, GID_WHEEL, 0600, "dcons");
#elif __FreeBSD_version >= 501109
	sprintf(cp->cn_name, "dcons");
#else
	cp->cn_dev = makedev(CDEV_MAJOR, DCONS_CON);
#endif
#if DCONS_FORCE_CONSOLE
	cp->cn_pri = CN_REMOTE;
#else
	cp->cn_pri = CN_NORMAL;
#endif
}

static void
dcons_cninit(struct consdev *cp)
{
	dcons_drv_init(0);
#if CONS_NODEV
	cp->cn_arg
#else
	cp->cn_dev->si_drv1
#endif
		= (void *)&sc[DCONS_CON]; /* share port0 with unit0 */
}

static void
dcons_cnterm(struct consdev *cp)
{
}

#if CONS_NODEV
static int
dcons_cngetc(struct consdev *cp)
{
	struct dcons_softc *dc = (struct dcons_softc *)cp->cn_arg;
	return (dcons_os_checkc(dc));
}
static void
dcons_cnputc(struct consdev *cp, int c)
{
	struct dcons_softc *dc = (struct dcons_softc *)cp->cn_arg;
	dcons_os_putc(dc, c);
}
#else
static int
dcons_cngetc(DEV dev)
{
	struct dcons_softc *dc = (struct dcons_softc *)dev->si_drv1;
	return (dcons_os_getc(dc));
}
static int
dcons_cncheckc(DEV dev)
{
	struct dcons_softc *dc = (struct dcons_softc *)dev->si_drv1;
	return (dcons_os_checkc(dc));
}
static void
dcons_cnputc(DEV dev, int c)
{
	struct dcons_softc *dc = (struct dcons_softc *)dev->si_drv1;
	dcons_os_putc(dc, c);
}
#endif

static int
dcons_drv_init(int stage)
{
#if defined(__i386__) || defined(__amd64__)
	quad_t addr, size;
#endif

	if (drv_init)
		return(drv_init);

	drv_init = -1;

	bzero(&dg, sizeof(dg));
	dcons_conf = &dg;
	dg.cdev = &dcons_consdev;
	dg.buf = NULL;
	dg.size = DCONS_BUF_SIZE;

#if defined(__i386__) || defined(__amd64__)
	if (getenv_quad("dcons.addr", &addr) > 0 &&
	    getenv_quad("dcons.size", &size) > 0) {
#ifdef __i386__
		vm_paddr_t pa;
		/*
		 * Allow read/write access to dcons buffer.
		 */
		for (pa = trunc_page(addr); pa < addr + size; pa += PAGE_SIZE)
			*vtopte(KERNBASE + pa) |= PG_RW;
		invltlb();
#endif
		/* XXX P to V */
		dg.buf = (struct dcons_buf *)(vm_offset_t)(KERNBASE + addr);
		dg.size = size;
		if (dcons_load_buffer(dg.buf, dg.size, sc) < 0)
			dg.buf = NULL;
	}
#endif
	if (dg.buf != NULL)
		goto ok;

#ifndef KLD_MODULE
	if (stage == 0) { /* XXX or cold */
		/*
		 * DCONS_FORCE_CONSOLE == 1 and statically linked.
		 * called from cninit(). can't use contigmalloc yet .
		 */
		dg.buf = (struct dcons_buf *) bssbuf;
		dcons_init(dg.buf, dg.size, sc);
	} else
#endif
	{
		/*
		 * DCONS_FORCE_CONSOLE == 0 or kernel module case.
		 * if the module is loaded after boot,
		 * bssbuf could be non-continuous.
		 */ 
		dg.buf = (struct dcons_buf *) contigmalloc(dg.size,
			M_DEVBUF, 0, 0x10000, 0xffffffff, PAGE_SIZE, 0ul);
		dcons_init(dg.buf, dg.size, sc);
	}

ok:
	dcons_buf = dg.buf;

#if __FreeBSD_version < 502122
#if defined(DDB) && DCONS_FORCE_GDB
#if CONS_NODEV
	gdbconsdev.cn_arg = (void *)&sc[DCONS_GDB];
#if __FreeBSD_version >= 501109
	sprintf(gdbconsdev.cn_name, "dgdb");
#endif
	gdb_arg = &gdbconsdev;
#elif defined(__DragonFly__)
	gdbdev = make_dev(&dcons_cdevsw, DCONS_GDB,
	    UID_ROOT, GID_WHEEL, 0600, "dgdb");
#else
	gdbdev = makedev(CDEV_MAJOR, DCONS_GDB);
#endif
	gdb_getc = dcons_cngetc;
	gdb_putc = dcons_cnputc;
#endif
#endif
	drv_init = 1;

	return 0;
}


static int
dcons_attach_port(int port, char *name, int flags)
{
	struct dcons_softc *dc;
	struct tty *tp;
	DEV dev;

	dc = &sc[port];
	dc->flags = flags;
	dev = make_dev(&dcons_cdevsw, port,
			UID_ROOT, GID_WHEEL, 0600, name);
	dc->dev = (void *)dev;
	tp = ttyalloc();

	dev->si_drv1 = (void *)dc;
	dev->si_tty = tp;

	tp->t_oproc = dcons_tty_start;
	tp->t_param = dcons_tty_param;
	tp->t_stop = nottystop;
	tp->t_dev = dc->dev;

	return(0);
}

static int
dcons_attach(void)
{
	int polltime;

#ifdef __DragonFly__
	cdevsw_add(&dcons_cdevsw, -1, 0);
#endif
	dcons_attach_port(DCONS_CON, "dcons", 0);
	dcons_attach_port(DCONS_GDB, "dgdb", DC_GDB);
#if __FreeBSD_version < 500000
	callout_init(&dcons_callout);
#else
	callout_init(&dcons_callout, 0);
#endif
	polltime = hz / poll_hz;
	if (polltime < 1)
		polltime = 1;
	callout_reset(&dcons_callout, polltime, dcons_timeout, NULL);
	return(0);
}

static int
dcons_detach(int port)
{
	struct	tty *tp;
	struct dcons_softc *dc;

	dc = &sc[port];

	tp = ((DEV)dc->dev)->si_tty;

	if (tp->t_state & TS_ISOPEN) {
		printf("dcons: still opened\n");
#if __FreeBSD_version < 502113
		(*linesw[tp->t_line].l_close)(tp, 0);
		tp->t_gen++;
		ttyclose(tp);
		ttwakeup(tp);
		ttwwakeup(tp);
#else
		ttyld_close(tp, 0);
		tty_close(tp);
#endif
	}
	/* XXX
	 * must wait until all device are closed.
	 */
#ifdef __DragonFly__
	tsleep((void *)dc, 0, "dcodtc", hz/4);
#else
	tsleep((void *)dc, PWAIT, "dcodtc", hz/4);
#endif
	destroy_dev(dc->dev);

	return(0);
}


/* cnXXX works only for FreeBSD-5 */
static int
dcons_modevent(module_t mode, int type, void *data)
{
	int err = 0, ret;

	switch (type) {
	case MOD_LOAD:
		ret = dcons_drv_init(1);
		dcons_attach();
#if __FreeBSD_version >= 500000
		if (ret == 0) {
			dcons_cnprobe(&dcons_consdev);
			dcons_cninit(&dcons_consdev);
			cnadd(&dcons_consdev);
		}
#endif
		break;
	case MOD_UNLOAD:
		printf("dcons: unload\n");
		callout_stop(&dcons_callout);
#if __FreeBSD_version < 502122
#if defined(DDB) && DCONS_FORCE_GDB
#if CONS_NODEV
		gdb_arg = NULL;
#else
		gdbdev = NULL;
#endif
#endif
#endif
#if __FreeBSD_version >= 500000
		cnremove(&dcons_consdev);
#endif
		dcons_detach(DCONS_CON);
		dcons_detach(DCONS_GDB);
		dg.buf->magic = 0;

		contigfree(dg.buf, DCONS_BUF_SIZE, M_DEVBUF);

		break;
	case MOD_SHUTDOWN:
#if 0		/* Keep connection after halt */
		dg.buf->magic = 0;
#endif
		break;
	default:
		err = EOPNOTSUPP;
		break;
	}
	return(err);
}

#if defined(GDB) && (__FreeBSD_version >= 502122)
/* Debugger interface */

static int
dcons_dbg_probe(void)
{
	int dcons_gdb;

	if (getenv_int("dcons_gdb", &dcons_gdb) == 0)
		return (-1);
	return (dcons_gdb);
}

static void
dcons_dbg_init(void)
{
}

static void
dcons_dbg_term(void)
{
}

static void
dcons_dbg_putc(int c)
{
	struct dcons_softc *dc = &sc[DCONS_GDB];
	dcons_os_putc(dc, c);
}

static int
dcons_dbg_getc(void)
{
	struct dcons_softc *dc = &sc[DCONS_GDB];
	return (dcons_os_getc(dc));
}
#endif

DEV_MODULE(dcons, dcons_modevent, NULL);
MODULE_VERSION(dcons, DCONS_VERSION);
