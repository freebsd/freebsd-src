/*-
 * Copyright (c) 2008 Ed Schouten <ed@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed under sponsorship from Snow
 * B.V., the Netherlands.
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
 *
 *	@(#)tty.h	8.6 (Berkeley) 1/21/94
 * $FreeBSD$
 */

#ifndef _SYS_TTY_H_
#define	_SYS_TTY_H_

#include <sys/clist.h>
#include <sys/termios.h>
#include <sys/queue.h>
#include <sys/selinfo.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>

struct tty;
struct pps_state;
struct cdev;
struct cdevsw;
struct thread;

typedef int t_open_t(struct tty *, struct cdev *);
typedef void t_close_t(struct tty *);
typedef void t_oproc_t(struct tty *);
typedef void t_purge_t(struct tty *);
typedef void t_stop_t(struct tty *, int);
typedef int t_param_t(struct tty *, struct termios *);
typedef int t_modem_t(struct tty *, int, int);
typedef void t_break_t(struct tty *, int);
typedef int t_ioctl_t(struct tty *, u_long cmd, void * data,
		      int fflag, struct thread *td);
/* XXX: same as d_ioctl_t in sys/conf.h to avoid #include polution */
typedef int __d_ioctl_t(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td);



/*
 * Per-tty structure.
 *
 * Should be split in two, into device and tty drivers.
 * Glue could be masks of what to echo and circular buffer
 * (low, high, timeout).
 */
struct tty {
	struct	clist t_rawq;		/* Device raw input queue. */
	long	t_rawcc;		/* Raw input queue statistics. */
	struct	clist t_canq;		/* Device canonical queue. */
	long	t_cancc;		/* Canonical queue statistics. */
	struct	clist t_outq;		/* Device output queue. */
	long	t_outcc;		/* Output queue statistics. */
	int	t_line;			/* Interface to device drivers. */
	struct cdev *t_dev;		/* Device. */
	struct cdev *t_mdev;		/* Device. */
	u_int	t_devunit;		/* Cdev unit number */
	int	t_state;		/* Device and driver (TS*) state. */
	int	t_flags;		/* Tty flags. */
	int     t_timeout;              /* Timeout for ttywait() */
	struct	pgrp *t_pgrp;		/* Foreground process group. */
	struct	session *t_session;	/* Enclosing session. */
	struct  sigio *t_sigio;		/* Information for async I/O. */
	struct	selinfo t_rsel;		/* Tty read/oob select. */
	struct	selinfo t_wsel;		/* Tty write select. */
	struct	termios t_termios;	/* Termios state. */
	struct	termios t_init_in;	/* ... init ingoing */
	struct	termios t_init_out;	/* ... outgoing */
	struct	termios t_lock_in;	/* ... lock ingoing */
	struct	termios t_lock_out;	/* ... outgoing */
	struct	winsize t_winsize;	/* Window size. */
	void	*t_sc;			/* driver private softc pointer. */
	void	*t_lsc;			/* linedisc private softc pointer. */
	int	t_column;		/* Tty output column. */
	int	t_rocount, t_rocol;	/* Tty. */
	int	t_ififosize;		/* Total size of upstream fifos. */
	int	t_ihiwat;		/* High water mark for input. */
	int	t_ilowat;		/* Low water mark for input. */
	speed_t	t_ispeedwat;		/* t_ispeed override for watermarks. */
	int	t_ohiwat;		/* High water mark for output. */
	int	t_olowat;		/* Low water mark for output. */
	speed_t	t_ospeedwat;		/* t_ospeed override for watermarks. */
	int	t_gen;			/* Generation number. */
	TAILQ_ENTRY(tty) t_list;	/* Global chain of ttys for pstat(8) */
	int	t_actout;		/* Outbound device open */
	int	t_wopeners;		/* #threads waiting for DCD in open */

	struct mtx t_mtx;
	int	t_refcnt;
	int	t_hotchar;		/* linedisc preferred hot char */
	int	t_dtr_wait;		/* Inter-session DTR holddown [hz] */
	int	t_do_timestamp;		/* flag instead ? */
	struct	timeval t_timestamp;	/* char timestamp */
	struct	pps_state *t_pps;	/* PPS-API stuff */

	/* Driver supplied methods */
	t_oproc_t *t_oproc;		/* Start output. */
	t_stop_t *t_stop;		/* Stop output. */
	t_param_t *t_param;		/* Set parameters. */
	t_modem_t *t_modem;		/* Set modem state (optional). */
	t_break_t *t_break;		/* Set break state (optional). */
	t_ioctl_t *t_ioctl;		/* Set ioctl handling (optional). */
	t_open_t *t_open;		/* First open */
	t_purge_t *t_purge;		/* Purge threads */
	t_close_t *t_close;		/* Last close */
	__d_ioctl_t *t_cioctl;		/* Ioctl on control devices */
};

#define	t_cc		t_termios.c_cc
#define	t_cflag		t_termios.c_cflag
#define	t_iflag		t_termios.c_iflag
#define	t_ispeed	t_termios.c_ispeed
#define	t_lflag		t_termios.c_lflag
#define	t_min		t_termios.c_min
#define	t_oflag		t_termios.c_oflag
#define	t_ospeed	t_termios.c_ospeed
#define	t_time		t_termios.c_time

#define	TTIPRI		(PSOCK + 1)	/* Sleep priority for tty reads. */
#define	TTOPRI		(PSOCK + 2)	/* Sleep priority for tty writes. */

/*
 * Userland version of struct tty, for sysctl.
 */
struct xtty {
	size_t	xt_size;		/* Structure size. */
	long	xt_rawcc;		/* Raw input queue statistics. */
	long	xt_cancc;		/* Canonical queue statistics. */
	long	xt_outcc;		/* Output queue statistics. */
	int	xt_line;		/* Interface to device drivers. */
	dev_t	xt_dev;			/* Userland (sysctl) instance. */
	int	xt_state;		/* Device and driver (TS*) state. */
	int	xt_flags;		/* Tty flags. */
	int     xt_timeout;		/* Timeout for ttywait(). */
	pid_t	xt_pgid;		/* Process group ID. */
	pid_t	xt_sid;			/* Session ID. */
	struct	termios xt_termios;	/* Termios state. */
	struct	winsize xt_winsize;	/* Window size. */
	int	xt_column;		/* Tty output column. */
	int	xt_rocount, xt_rocol;	/* Tty. */
	int	xt_ififosize;		/* Total size of upstream fifos. */
	int	xt_ihiwat;		/* High water mark for input. */
	int	xt_ilowat;		/* Low water mark for input. */
	speed_t	xt_ispeedwat;		/* t_ispeed override for watermarks. */
	int	xt_ohiwat;		/* High water mark for output. */
	int	xt_olowat;		/* Low water mark for output. */
	speed_t	xt_ospeedwat;		/* t_ospeed override for watermarks. */
};

/*
 * User data unfortunately has to be copied through buffers on the way to
 * and from clists.  The buffers are on the stack so their sizes must be
 * fairly small.
 */
#define	IBUFSIZ	384			/* Should be >= max value of MIN. */
#define	OBUFSIZ	100

#ifndef TTYHOG
#define	TTYHOG	8192
#endif

#ifdef _KERNEL
#define	TTMAXHIWAT	roundup(2048, CBSIZE)
#define	TTMINHIWAT	roundup(100, CBSIZE)
#define	TTMAXLOWAT	256
#define	TTMINLOWAT	32
#endif

/* These flags are kept in t_state. */
#define	TS_SO_OLOWAT	0x00001		/* Wake up when output <= low water. */
#define	TS_ASYNC	0x00002		/* Tty in async I/O mode. */
#define	TS_BUSY		0x00004		/* Draining output. */
#define	TS_CARR_ON	0x00008		/* Carrier is present. */
#define	TS_FLUSH	0x00010		/* Outq has been flushed during DMA. */
#define	TS_ISOPEN	0x00020		/* Open has completed. */
#define	TS_TBLOCK	0x00040		/* Further input blocked. */
#define	TS_TIMEOUT	0x00080		/* Wait for output char processing. */
#define	TS_TTSTOP	0x00100		/* Output paused. */
#ifdef notyet
#define	TS_WOPEN	0x00200		/* Open in progress. */
#endif
#define	TS_XCLUDE	0x00400		/* Tty requires exclusivity. */

/* State for intra-line fancy editing work. */
#define	TS_BKSL		0x00800		/* State for lowercase \ work. */
#define	TS_CNTTB	0x01000		/* Counting tab width, ignore FLUSHO. */
#define	TS_ERASE	0x02000		/* Within a \.../ for PRTRUB. */
#define	TS_LNCH		0x04000		/* Next character is literal. */
#define	TS_TYPEN	0x08000		/* Retyping suspended input (PENDIN). */
#define	TS_LOCAL	(TS_BKSL | TS_CNTTB | TS_ERASE | TS_LNCH | TS_TYPEN)

/* Extras. */
#define	TS_CAN_BYPASS_L_RINT 0x010000	/* Device in "raw" mode. */
#define	TS_CONNECTED	0x020000	/* Connection open. */
#define	TS_SNOOP	0x040000	/* Device is being snooped on. */
#define	TS_SO_OCOMPLETE	0x080000	/* Wake up when output completes. */
#define	TS_ZOMBIE	0x100000	/* Connection lost. */

/* Hardware flow-control-invoked bits. */
#define	TS_CAR_OFLOW	0x200000	/* For MDMBUF (XXX handle in driver). */
#ifdef notyet
#define	TS_CTS_OFLOW	0x400000	/* For CCTS_OFLOW. */
#define	TS_DSR_OFLOW	0x800000	/* For CDSR_OFLOW. */
#endif

#define TS_DTR_WAIT	0x1000000	/* DTR hold-down between sessions */
#define TS_GONE		0x2000000	/* Hardware detached */
#define TS_CALLOUT	0x4000000	/* Callout devices */

/* Character type information. */
#define	ORDINARY	0
#define	CONTROL		1
#define	BACKSPACE	2
#define	NEWLINE		3
#define	TAB		4
#define	VTAB		5
#define	RETURN		6

struct speedtab {
	int sp_speed;			/* Speed. */
	int sp_code;			/* Code. */
};

/* Modem control commands (driver). */
#define	DMSET		0
#define	DMBIS		1
#define	DMBIC		2
#define	DMGET		3

/* Flags on a character passed to ttyinput. */
#define	TTY_CHARMASK	0x000000ff	/* Character mask */
#define	TTY_QUOTE	0x00000100	/* Character quoted */
#define	TTY_ERRORMASK	0xff000000	/* Error mask */
#define	TTY_FE		0x01000000	/* Framing error */
#define	TTY_PE		0x02000000	/* Parity error */
#define	TTY_OE		0x04000000	/* Overrun error */
#define	TTY_BI		0x08000000	/* Break condition */

/* Is tp controlling terminal for p? */
#define	isctty(p, tp)							\
	((p)->p_session == (tp)->t_session && (p)->p_flag & P_CONTROLT)

/* Is p in background of tp? */
#define	isbackground(p, tp)						\
	(isctty((p), (tp)) && (p)->p_pgrp != (tp)->t_pgrp)

/* Unique sleep addresses. */
#define	TSA_CARR_ON(tp)		((void *)&(tp)->t_rawq)
#define	TSA_HUP_OR_INPUT(tp)	((void *)&(tp)->t_rawq.c_cf)
#define	TSA_OCOMPLETE(tp)	((void *)&(tp)->t_outq.c_cl)
#define	TSA_OLOWAT(tp)		((void *)&(tp)->t_outq)

#ifdef _KERNEL
#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_TTYS);
#endif

/* Minor number flag bits */
#define	MINOR_CALLOUT	0x80000000
#define	MINOR_INIT	0x40000000
#define	MINOR_LOCK	0x20000000

#define	ISCALLOUT(dev)	(minor(dev) & MINOR_CALLOUT)
#define	ISINIT(dev)	(minor(dev) & MINOR_INIT)
#define	ISLOCK(dev)	(minor(dev) & MINOR_LOCK)

extern long tk_cancc;
extern long tk_nin;
extern long tk_nout;
extern long tk_rawcc;

void	 nottystop(struct tty *tp, int rw);
void	 termioschars(struct termios *t);
int	 tputchar(int c, struct tty *tp);
int	 ttcompat(struct tty *tp, u_long com, caddr_t data, int flag);
int	 ttioctl(struct tty *tp, u_long com, void *data, int flag);
int	 ttread(struct tty *tp, struct uio *uio, int flag);
void	 ttrstrt(void *tp);
void	 ttsetwater(struct tty *tp);
int	 ttspeedtab(int speed, struct speedtab *table);
int	 ttstart(struct tty *tp);
void	 ttwakeup(struct tty *tp);
int	 ttwrite(struct tty *tp, struct uio *uio, int flag);
void	 ttwwakeup(struct tty *tp);
struct tty *ttyalloc(void);
void	 ttyblock(struct tty *tp);
void	 ttychars(struct tty *tp);
int	 ttycheckoutq(struct tty *tp, int wait);
void	 ttyconsolemode(struct tty *tp, int speed);
int	 tty_close(struct tty *tp);
int	 ttycreate(struct tty *tp, int flags, const char *fmt, ...) __printflike(3, 4);
int	 ttydtrwaitsleep(struct tty *tp);
void	 ttydtrwaitstart(struct tty *tp);
void	 ttyflush(struct tty *tp, int rw);
void	 ttyfree(struct tty *tp);
void	 ttygone(struct tty *tp);
void	 ttyinfo(struct tty *tp);
void	 ttyinitmode(struct tty *tp, int echo, int speed);
int	 ttyinput(int c, struct tty *tp);
int	 ttylclose(struct tty *tp, int flag);
void	 ttyldoptim(struct tty *tp);
int	 ttymodem(struct tty *tp, int flag);
int	 tty_open(struct cdev *device, struct tty *tp);
int	 ttyref(struct tty *tp);
int	 ttyrel(struct tty *tp);
int	 ttysleep(struct tty *tp, void *chan, int pri, char *wmesg, int timo);
int	 ttywait(struct tty *tp);

static __inline int
tt_open(struct tty *t, struct cdev *c)
{

	if (t->t_open == NULL)
		return (0);
	return (t->t_open(t, c));
}

static __inline void
tt_close(struct tty *t)
{

	if (t->t_close != NULL)
		return (t->t_close(t));
}

static __inline void
tt_oproc(struct tty *t)
{

	if (t->t_oproc != NULL)			/* XXX: Kludge for pty. */
		t->t_oproc(t);
}

static __inline void
tt_purge(struct tty *t)
{

	if (t->t_purge != NULL)
		t->t_purge(t);
}

static __inline void
tt_stop(struct tty *t, int i)
{

	t->t_stop(t, i);
}

static __inline int
tt_param(struct tty *t, struct termios *s)
{

	if (t->t_param == NULL)
		return (0);
	return (t->t_param(t, s));
}

static __inline int
tt_modem(struct tty *t, int i, int j)
{

	if (t->t_modem == NULL)
		return (0);
	return (t->t_modem(t, i, j));
}

static __inline int
tt_break(struct tty *t, int i)
{

	if (t->t_break == NULL)
		return (ENOIOCTL);
	t->t_break(t, i);
	return (0);
}

static __inline int
tt_ioctl(struct tty *t, u_long cmd, void *data,
		      int fflag, struct thread *td)
{

	return (t->t_ioctl(t, cmd, data, fflag, td));
}

/*
 * XXX: temporary
 */
#include <sys/linedisc.h>

#endif /* _KERNEL */

#endif /* !_SYS_TTY_H_ */
