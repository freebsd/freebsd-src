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
 * $Id: dcons.c,v 1.65 2003/10/24 03:24:55 simokawa Exp $
 * $FreeBSD$
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
#include <sys/proc.h>
#include <sys/ucred.h>

#include <machine/bus.h>

#include <dev/dcons/dcons.h>

#include <ddb/ddb.h>
#include <sys/reboot.h>

#include <sys/sysctl.h>

#include "opt_ddb.h"
#include "opt_comconsole.h"
#include "opt_dcons.h"

#ifndef DCONS_POLL_HZ
#define DCONS_POLL_HZ	100
#endif

#ifndef DCONS_BUF_SIZE
#define DCONS_BUF_SIZE (16*1024)
#endif

#ifndef DCONS_FORCE_CONSOLE
#define DCONS_FORCE_CONSOLE	0	/* mostly for FreeBSD-4 */
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

static struct cdevsw dcons_cdevsw = {
#if __FreeBSD_version >= 500104
	.d_version =	D_VERSION,
	.d_open =	dcons_open,
	.d_close =	dcons_close,
	.d_name =	"dcons",
	.d_flags =	D_TTY | D_NEEDGIANT,
#else
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
	/* flags */	0,
#endif
};

#ifndef KLD_MODULE
static char bssbuf[DCONS_BUF_SIZE];	/* buf in bss */
#endif

/* global data */
static struct dcons_global dg;
struct dcons_global *dcons_conf;
static int poll_hz = DCONS_POLL_HZ;

SYSCTL_NODE(_kern, OID_AUTO, dcons, CTLFLAG_RD, 0, "Dumb Console");
SYSCTL_INT(_kern_dcons, OID_AUTO, poll_hz, CTLFLAG_RW, &poll_hz, 0,
				"dcons polling rate");

static int drv_init = 0;
static struct callout dcons_callout;
struct dcons_buf *dcons_buf;		/* for local dconschat */

/* per device data */
static struct dcons_softc {
	struct cdev *dev;
	struct dcons_ch	o, i;
	int brk_state;
#define DC_GDB	1
	int flags;
} sc[DCONS_NPORT];
static void	dcons_tty_start(struct tty *);
static int	dcons_tty_param(struct tty *, struct termios *);
static void	dcons_timeout(void *);
static int	dcons_drv_init(int);
static int	dcons_getc(struct dcons_softc *);
static int	dcons_checkc(struct dcons_softc *);
static void	dcons_putc(struct dcons_softc *, int);

static cn_probe_t	dcons_cnprobe;
static cn_init_t	dcons_cninit;
static cn_getc_t	dcons_cngetc;
static cn_checkc_t 	dcons_cncheckc;
static cn_putc_t	dcons_cnputc;

CONS_DRIVER(dcons, dcons_cnprobe, dcons_cninit, NULL, dcons_cngetc,
    dcons_cncheckc, dcons_cnputc, NULL);

#if __FreeBSD_version >= 502122
static gdb_probe_f dcons_dbg_probe;
static gdb_init_f dcons_dbg_init;
static gdb_term_f dcons_dbg_term;
static gdb_getc_f dcons_dbg_getc;
static gdb_checkc_f dcons_dbg_checkc;
static gdb_putc_f dcons_dbg_putc;

GDB_DBGPORT(dcons, dcons_dbg_probe, dcons_dbg_init, dcons_dbg_term,
    dcons_dbg_checkc, dcons_dbg_getc, dcons_dbg_putc);

extern struct gdb_dbgport *gdb_cur;
#endif

#if __FreeBSD_version < 500000
#define THREAD	proc
#else
#define THREAD	thread
#endif

static int
dcons_open(struct cdev *dev, int flag, int mode, struct THREAD *td)
{
	struct tty *tp;
	int unit, error, s;

	unit = minor(dev);
	if (unit != 0)
		return (ENXIO);

	tp = dev->si_tty = ttymalloc(dev->si_tty);
	tp->t_oproc = dcons_tty_start;
	tp->t_param = dcons_tty_param;
	tp->t_stop = nottystop;
	tp->t_dev = dev;

	error = 0;

	s = spltty();
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG|CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ttsetwater(tp);
	} else if ((tp->t_state & TS_XCLUDE) && suser(td)) {
		splx(s);
		return (EBUSY);
	}
	splx(s);

	error = ttyld_open(tp, dev);

	return (error);
}

static int
dcons_close(struct cdev *dev, int flag, int mode, struct THREAD *td)
{
	int	unit;
	struct	tty *tp;

	unit = minor(dev);
	if (unit != 0)
		return (ENXIO);

	tp = dev->si_tty;
	if (tp->t_state & TS_ISOPEN) {
		ttyld_close(tp, flag);
		tty_close(tp);
	}

	return (0);
}

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
		dcons_putc(dc, getc(&tp->t_outq));
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
		tp = dc->dev->si_tty;
		while ((c = dcons_checkc(dc)) != -1)
			if (tp->t_state & TS_ISOPEN)
				ttyld_rint(tp, c);
	}
	polltime = hz / poll_hz;
	if (polltime < 1)
		polltime = 1;
	callout_reset(&dcons_callout, polltime, dcons_timeout, tp);
}

static void
dcons_cnprobe(struct consdev *cp)
{
#if __FreeBSD_version >= 501109
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

#if CONS_NODEV
static int
dcons_cngetc(struct consdev *cp)
{
	return(dcons_getc((struct dcons_softc *)cp->cn_arg));
}
static int
dcons_cncheckc(struct consdev *cp)
{
	return(dcons_checkc((struct dcons_softc *)cp->cn_arg)); 
}
static void
dcons_cnputc(struct consdev *cp, int c)
{
	dcons_putc((struct dcons_softc *)cp->cn_arg, c);
}
#else
static int
dcons_cngetc(struct cdev *dev)
{
	return(dcons_getc((struct dcons_softc *)dev->si_drv1));
}
static int
dcons_cncheckc(struct cdev *dev)
{
	return(dcons_checkc((struct dcons_softc *)dev->si_drv1));
}
static void
dcons_cnputc(struct cdev *dev, int c)
{
	dcons_putc((struct dcons_softc *)dev->si_drv1, c);
}
#endif

static int
dcons_getc(struct dcons_softc *dc)
{
	int c;

	while ((c = dcons_checkc(dc)) == -1);

	return (c & 0xff);
}

static int
dcons_checkc(struct dcons_softc *dc)
{
	unsigned char c;
	u_int32_t ptr, pos, gen, next_gen;
	struct dcons_ch *ch;

	ch = &dc->i;

	if (dg.dma_tag != NULL)
		bus_dmamap_sync(dg.dma_tag, dg.dma_map, BUS_DMASYNC_POSTREAD);
	ptr = ntohl(*ch->ptr);
	gen = ptr >> DCONS_GEN_SHIFT;
	pos = ptr & DCONS_POS_MASK;
	if (gen == ch->gen && pos == ch->pos)
		return (-1);

	next_gen = DCONS_NEXT_GEN(ch->gen);
	/* XXX sanity check */
	if ((gen != ch->gen && gen != next_gen)
			|| (gen == ch->gen && pos < ch->pos)) {
		/* generation skipped !! */
		/* XXX discard */
		ch->gen = gen;
		ch->pos = pos;
		return (-1);
	}

	c = ch->buf[ch->pos];
	ch->pos ++;
	if (ch->pos >= ch->size) {
		ch->gen = next_gen;
		ch->pos = 0;
	}

#if __FreeBSD_version >= 502122
#if KDB && ALT_BREAK_TO_DEBUGGER
	if (kdb_alt_break(c, &dc->brk_state)) {
		if ((dc->flags & DC_GDB) != 0) {
			if (gdb_cur == &dcons_gdb_dbgport) {
				kdb_dbbe_select("gdb");
				breakpoint();
			}
		} else
			breakpoint();
	}
#endif
#else
#if DDB && ALT_BREAK_TO_DEBUGGER
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
#endif
	return (c);
}

static void
dcons_putc(struct dcons_softc *dc, int c)
{
	struct dcons_ch *ch;

	ch = &dc->o;

	ch->buf[ch->pos] = c;
	ch->pos ++;
	if (ch->pos >= ch->size) {
		ch->gen = DCONS_NEXT_GEN(ch->gen);
		ch->pos = 0;
	}
	*ch->ptr = DCONS_MAKE_PTR(ch);
	if (dg.dma_tag != NULL)
		bus_dmamap_sync(dg.dma_tag, dg.dma_map, BUS_DMASYNC_PREWRITE);
}

static int
dcons_init_port(int port, int offset, int size)
{
	int osize;
	struct dcons_softc *dc;

	dc = &sc[port];

	osize = size * 3 / 4;

	dc->o.size = osize;
	dc->i.size = size - osize;
	dc->o.buf = (char *)dg.buf + offset;
	dc->i.buf = dc->o.buf + osize;
	dc->o.gen = dc->i.gen = 0;
	dc->o.pos = dc->i.pos = 0;
	dc->o.ptr = &dg.buf->optr[port];
	dc->i.ptr = &dg.buf->iptr[port];
	dc->brk_state = STATE0;
	dg.buf->osize[port] = htonl(osize);
	dg.buf->isize[port] = htonl(size - osize);
	dg.buf->ooffset[port] = htonl(offset);
	dg.buf->ioffset[port] = htonl(offset + osize);
	dg.buf->optr[port] = DCONS_MAKE_PTR(&dc->o);
	dg.buf->iptr[port] = DCONS_MAKE_PTR(&dc->i);

	return(0);
}

static int
dcons_drv_init(int stage)
{
	int size, size0, offset;

	if (drv_init)
		return(drv_init);

	drv_init = -1;

	bzero(&dg, sizeof(dg));
	dcons_conf = &dg;
	dg.cdev = &dcons_consdev;
	dg.size = DCONS_BUF_SIZE;

#ifndef KLD_MODULE
	if (stage == 0) /* XXX or cold */
		/*
		 * DCONS_FORCE_CONSOLE == 1 and statically linked.
		 * called from cninit(). can't use contigmalloc yet .
		 */
		dg.buf = (struct dcons_buf *) bssbuf;
	else
#endif
		/*
		 * DCONS_FORCE_CONSOLE == 0 or kernel module case.
		 * if the module is loaded after boot,
		 * bssbuf could be non-continuous.
		 */ 
		dg.buf = (struct dcons_buf *) contigmalloc(dg.size,
			M_DEVBUF, 0, 0x10000, 0xffffffff, PAGE_SIZE, 0ul);

	dcons_buf = dg.buf;
	offset = DCONS_HEADER_SIZE;
	size = (dg.size - offset);
	size0 = size * 3 / 4;

	dcons_init_port(0, offset, size0);
	offset += size0;
	dcons_init_port(1, offset, size - size0);
	dg.buf->version = htonl(DCONS_VERSION);
	dg.buf->magic = ntohl(DCONS_MAGIC);

#if __FreeBSD_version < 502122
#if DDB && DCONS_FORCE_GDB
#if CONS_NODEV
	gdbconsdev.cn_arg = (void *)&sc[DCONS_GDB];
#if __FreeBSD_version >= 501109
	sprintf(gdbconsdev.cn_name, "dgdb");
#endif
	gdb_arg = &gdbconsdev;
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

	dc = &sc[port];
	dc->flags = flags;
	dc->dev = make_dev(&dcons_cdevsw, port,
			UID_ROOT, GID_WHEEL, 0600, name);
	tp = ttymalloc(NULL);

	dc->dev->si_drv1 = (void *)dc;
	dc->dev->si_tty = tp;

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

	tp = dc->dev->si_tty;

	if (tp->t_state & TS_ISOPEN) {
		printf("dcons: still opened\n");
		ttyld_close(tp, 0);
		tty_close(tp);
	}
	/* XXX
	 * must wait until all device are closed.
	 */
	tsleep((void *)dc, PWAIT, "dcodtc", hz/4);
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
#if DDB && DCONS_FORCE_GDB
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
		break;
	default:
		err = EOPNOTSUPP;
		break;
	}
	return(err);
}

#if __FreeBSD_version >= 502122
/* Debugger interface */

static int
dcons_dbg_probe(void)
{
	return(DCONS_FORCE_GDB);
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
	dcons_putc(&sc[DCONS_GDB], c);
}

static int
dcons_dbg_checkc(void)
{
	return (dcons_checkc(&sc[DCONS_GDB]));
}

static int
dcons_dbg_getc(void)
{
	return (dcons_getc(&sc[DCONS_GDB]));
}
#endif

DEV_MODULE(dcons, dcons_modevent, NULL);
MODULE_VERSION(dcons, DCONS_VERSION);
