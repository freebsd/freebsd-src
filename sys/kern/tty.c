/*-
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Copyright (c) 2002 Networks Associates Technologies, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)tty.c	8.8 (Berkeley) 1/21/94
 */

/*-
 * TODO:
 *	o Fix races for sending the start char in ttyflush().
 *	o Handle inter-byte timeout for "MIN > 0, TIME > 0" in ttyselect().
 *	  With luck, there will be MIN chars before select() returns().
 *	o Handle CLOCAL consistently for ptys.  Perhaps disallow setting it.
 *	o Don't allow input in TS_ZOMBIE case.  It would be visible through
 *	  FIONREAD.
 *	o Do the new sio locking stuff here and use it to avoid special
 *	  case for EXTPROC?
 *	o Lock PENDIN too?
 *	o Move EXTPROC and/or PENDIN to t_state?
 *	o Wrap most of ttioctl in spltty/splx.
 *	o Implement TIOCNOTTY or remove it from <sys/ioctl.h>.
 *	o Send STOP if IXOFF is toggled off while TS_TBLOCK is set.
 *	o Don't allow certain termios flags to affect disciplines other
 *	  than TTYDISC.  Cancel their effects before switch disciplines
 *	  and ignore them if they are set while we are in another
 *	  discipline.
 *	o Now that historical speed conversions are handled here, don't
 *	  do them in drivers.
 *	o Check for TS_CARR_ON being set while everything is closed and not
 *	  waiting for carrier.  TS_CARR_ON isn't cleared if nothing is open,
 *	  so it would live until the next open even if carrier drops.
 *	o Restore TS_WOPEN since it is useful in pstat.  It must be cleared
 *	  only when _all_ openers leave open().
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_tty.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/sx.h>
#if defined(COMPAT_43TTY)
#include <sys/ioctl_compat.h>
#endif
#include <sys/priv.h>
#include <sys/proc.h>
#define	TTYDEFCHARS
#include <sys/tty.h>
#undef	TTYDEFCHARS
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/serial.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/malloc.h>
#include <sys/filedesc.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/timepps.h>

#include <machine/stdarg.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

MALLOC_DEFINE(M_TTYS, "ttys", "tty data structures");

long tk_cancc;
long tk_nin;
long tk_nout;
long tk_rawcc;

static	d_open_t	ttysopen;
static	d_close_t	ttysclose;
static	d_read_t	ttysrdwr;
static	d_ioctl_t	ttysioctl;
static	d_purge_t	ttypurge;

/* Default cdevsw for common tty devices */
static struct cdevsw tty_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	ttyopen,
	.d_close =	ttyclose,
	.d_ioctl =	ttyioctl,
	.d_purge =	ttypurge,
	.d_name =	"ttydrv",
	.d_flags =	D_TTY | D_NEEDGIANT,
};

/* Cdevsw for slave tty devices */
static struct cdevsw ttys_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	ttysopen,
	.d_close =	ttysclose,
	.d_read =	ttysrdwr,
	.d_write =	ttysrdwr,
	.d_ioctl =	ttysioctl,
	.d_name =	"TTYS",
	.d_flags =	D_TTY | D_NEEDGIANT,
};

static int	proc_sum(struct proc *, int *);
static int	proc_compare(struct proc *, struct proc *);
static int	thread_compare(struct thread *, struct thread *);
static int	ttnread(struct tty *tp);
static void	ttyecho(int c, struct tty *tp);
static int	ttyoutput(int c, struct tty *tp);
static void	ttypend(struct tty *tp);
static void	ttyretype(struct tty *tp);
static void	ttyrub(int c, struct tty *tp);
static void	ttyrubo(struct tty *tp, int cnt);
static void	ttyunblock(struct tty *tp);
static int	ttywflush(struct tty *tp);
static int	filt_ttyread(struct knote *kn, long hint);
static void	filt_ttyrdetach(struct knote *kn);
static int	filt_ttywrite(struct knote *kn, long hint);
static void	filt_ttywdetach(struct knote *kn);

/*
 * Table with character classes and parity. The 8th bit indicates parity,
 * the 7th bit indicates the character is an alphameric or underscore (for
 * ALTWERASE), and the low 6 bits indicate delay type.  If the low 6 bits
 * are 0 then the character needs no special processing on output; classes
 * other than 0 might be translated or (not currently) require delays.
 */
#define	E	0x00	/* Even parity. */
#define	O	0x80	/* Odd parity. */
#define	PARITY(c)	(char_type[c] & O)

#define	ALPHA	0x40	/* Alpha or underscore. */
#define	ISALPHA(c)	(char_type[(c) & TTY_CHARMASK] & ALPHA)

#define	CCLASSMASK	0x3f
#define	CCLASS(c)	(char_type[c] & CCLASSMASK)

#define	BS	BACKSPACE
#define	CC	CONTROL
#define	CR	RETURN
#define	NA	ORDINARY | ALPHA
#define	NL	NEWLINE
#define	NO	ORDINARY
#define	TB	TAB
#define	VT	VTAB

static u_char const char_type[] = {
	E|CC, O|CC, O|CC, E|CC, O|CC, E|CC, E|CC, O|CC,	/* nul - bel */
	O|BS, E|TB, E|NL, O|CC, E|VT, O|CR, O|CC, E|CC, /* bs - si */
	O|CC, E|CC, E|CC, O|CC, E|CC, O|CC, O|CC, E|CC, /* dle - etb */
	E|CC, O|CC, O|CC, E|CC, O|CC, E|CC, E|CC, O|CC, /* can - us */
	O|NO, E|NO, E|NO, O|NO, E|NO, O|NO, O|NO, E|NO, /* sp - ' */
	E|NO, O|NO, O|NO, E|NO, O|NO, E|NO, E|NO, O|NO, /* ( - / */
	E|NA, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* 0 - 7 */
	O|NA, E|NA, E|NO, O|NO, E|NO, O|NO, O|NO, E|NO, /* 8 - ? */
	O|NO, E|NA, E|NA, O|NA, E|NA, O|NA, O|NA, E|NA, /* @ - G */
	E|NA, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* H - O */
	E|NA, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* P - W */
	O|NA, E|NA, E|NA, O|NO, E|NO, O|NO, O|NO, O|NA, /* X - _ */
	E|NO, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* ` - g */
	O|NA, E|NA, E|NA, O|NA, E|NA, O|NA, O|NA, E|NA, /* h - o */
	O|NA, E|NA, E|NA, O|NA, E|NA, O|NA, O|NA, E|NA, /* p - w */
	E|NA, O|NA, O|NA, E|NO, O|NO, E|NO, E|NO, O|CC, /* x - del */
	/*
	 * Meta chars; should be settable per character set;
	 * for now, treat them all as normal characters.
	 */
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
};
#undef	BS
#undef	CC
#undef	CR
#undef	NA
#undef	NL
#undef	NO
#undef	TB
#undef	VT

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

#undef MAX_INPUT		/* XXX wrong in <sys/syslimits.h> */
#define	MAX_INPUT	TTYHOG	/* XXX limit is usually larger for !ICANON */

/*
 * list of struct tty where pstat(8) can pick it up with sysctl
 *
 * The lock order is to grab the list mutex before the tty mutex.
 * Together with additions going on the tail of the list, this allows
 * the sysctl to avoid doing retries.
 */
static	TAILQ_HEAD(, tty) tty_list = TAILQ_HEAD_INITIALIZER(tty_list);
static struct mtx tty_list_mutex;
MTX_SYSINIT(tty_list, &tty_list_mutex, "ttylist", MTX_DEF);

static struct unrhdr *tty_unit;

static int  drainwait = 5*60;
SYSCTL_INT(_kern, OID_AUTO, drainwait, CTLFLAG_RW, &drainwait,
	0, "Output drain timeout in seconds");

static struct tty *
tty_gettp(struct cdev *dev)
{
	struct tty *tp;
	struct cdevsw *csw;

	csw = dev_refthread(dev);
	KASSERT(csw != NULL, ("No cdevsw in ttycode (%s)", devtoname(dev)));
	KASSERT(csw->d_flags & D_TTY,
	    ("non D_TTY (%s) in tty code", devtoname(dev)));
	dev_relthread(dev);
	tp = dev->si_tty;
	KASSERT(tp != NULL,
	    ("no tty pointer on (%s) in tty code", devtoname(dev)));
	return (tp);
}

/*
 * Initial open of tty, or (re)entry to standard tty line discipline.
 */
int
tty_open(struct cdev *device, struct tty *tp)
{
	int s;

	s = spltty();
	tp->t_dev = device;
	tp->t_hotchar = 0;
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		ttyref(tp);
		SET(tp->t_state, TS_ISOPEN);
		if (ISSET(tp->t_cflag, CLOCAL))
			SET(tp->t_state, TS_CONNECTED);
		bzero(&tp->t_winsize, sizeof(tp->t_winsize));
	}
	/* XXX don't hang forever on output */
	if (tp->t_timeout < 0)
		tp->t_timeout = drainwait*hz;
	ttsetwater(tp);
	splx(s);
	return (0);
}

/*
 * Handle close() on a tty line: flush and set to initial state,
 * bumping generation number so that pending read/write calls
 * can detect recycling of the tty.
 * XXX our caller should have done `spltty(); l_close(); tty_close();'
 * and l_close() should have flushed, but we repeat the spltty() and
 * the flush in case there are buggy callers.
 */
int
tty_close(struct tty *tp)
{
	int ostate, s;

	funsetown(&tp->t_sigio);
	s = spltty();
	if (constty == tp)
		constty_clear();

	ttyflush(tp, FREAD | FWRITE);
	clist_free_cblocks(&tp->t_canq);
	clist_free_cblocks(&tp->t_outq);
	clist_free_cblocks(&tp->t_rawq);

	tp->t_gen++;
	tp->t_line = TTYDISC;
	tp->t_hotchar = 0;
	tp->t_pgrp = NULL;
	tp->t_session = NULL;
	ostate = tp->t_state;
	tp->t_state = 0;
	knlist_clear(&tp->t_rsel.si_note, 0);
	knlist_clear(&tp->t_wsel.si_note, 0);
	/*
	 * Both final close and revocation close might end up calling
	 * this method.  Only the thread clearing TS_ISOPEN should
	 * release the reference to the tty.
	 */
	if (ISSET(ostate, TS_ISOPEN))
		ttyrel(tp);
	splx(s);
	return (0);
}

#define	FLUSHQ(q) {							\
	if ((q)->c_cc)							\
		ndflush(q, (q)->c_cc);					\
}

/* Is 'c' a line delimiter ("break" character)? */
#define	TTBREAKC(c, lflag)							\
	((c) == '\n' || (((c) == cc[VEOF] ||				\
	  (c) == cc[VEOL] || ((c) == cc[VEOL2] && lflag & IEXTEN)) &&	\
	 (c) != _POSIX_VDISABLE))

/*
 * Process input of a single character received on a tty.
 */
int
ttyinput(int c, struct tty *tp)
{
	tcflag_t iflag, lflag;
	cc_t *cc;
	int i, err;

	/*
	 * If input is pending take it first.
	 */
	lflag = tp->t_lflag;
	if (ISSET(lflag, PENDIN))
		ttypend(tp);
	/*
	 * Gather stats.
	 */
	if (ISSET(lflag, ICANON)) {
		++tk_cancc;
		++tp->t_cancc;
	} else {
		++tk_rawcc;
		++tp->t_rawcc;
	}
	++tk_nin;

	/*
	 * Block further input iff:
	 * current input > threshold AND input is available to user program
	 * AND input flow control is enabled and not yet invoked.
	 * The 3 is slop for PARMRK.
	 */
	iflag = tp->t_iflag;
	if (tp->t_rawq.c_cc + tp->t_canq.c_cc > tp->t_ihiwat - 3 &&
	    (!ISSET(lflag, ICANON) || tp->t_canq.c_cc != 0) &&
	    (ISSET(tp->t_cflag, CRTS_IFLOW) || ISSET(iflag, IXOFF)) &&
	    !ISSET(tp->t_state, TS_TBLOCK))
		ttyblock(tp);

	/* Handle exceptional conditions (break, parity, framing). */
	cc = tp->t_cc;
	err = (ISSET(c, TTY_ERRORMASK));
	if (err) {
		CLR(c, TTY_ERRORMASK);
		if (ISSET(err, TTY_BI)) {
			if (ISSET(iflag, IGNBRK))
				return (0);
			if (ISSET(iflag, BRKINT)) {
				ttyflush(tp, FREAD | FWRITE);
				if (tp->t_pgrp != NULL) {
					PGRP_LOCK(tp->t_pgrp);
					pgsignal(tp->t_pgrp, SIGINT, 1);
					PGRP_UNLOCK(tp->t_pgrp);
				}
				goto endcase;
			}
			if (ISSET(iflag, PARMRK))
				goto parmrk;
		} else if ((ISSET(err, TTY_PE) && ISSET(iflag, INPCK))
			|| ISSET(err, TTY_FE)) {
			if (ISSET(iflag, IGNPAR))
				return (0);
			else if (ISSET(iflag, PARMRK)) {
parmrk:
				if (tp->t_rawq.c_cc + tp->t_canq.c_cc >
				    MAX_INPUT - 3)
					goto input_overflow;
				(void)putc(0377 | TTY_QUOTE, &tp->t_rawq);
				(void)putc(0 | TTY_QUOTE, &tp->t_rawq);
				(void)putc(c | TTY_QUOTE, &tp->t_rawq);
				goto endcase;
			} else
				c = 0;
		}
	}

	if (!ISSET(tp->t_state, TS_TYPEN) && ISSET(iflag, ISTRIP))
		CLR(c, 0x80);
	if (!ISSET(lflag, EXTPROC)) {
		/*
		 * Check for literal nexting very first
		 */
		if (ISSET(tp->t_state, TS_LNCH)) {
			SET(c, TTY_QUOTE);
			CLR(tp->t_state, TS_LNCH);
		}
		/*
		 * Scan for special characters.  This code
		 * is really just a big case statement with
		 * non-constant cases.  The bottom of the
		 * case statement is labeled ``endcase'', so goto
		 * it after a case match, or similar.
		 */

		/*
		 * Control chars which aren't controlled
		 * by ICANON, ISIG, or IXON.
		 */
		if (ISSET(lflag, IEXTEN)) {
			if (CCEQ(cc[VLNEXT], c)) {
				if (ISSET(lflag, ECHO)) {
					if (ISSET(lflag, ECHOE)) {
						(void)ttyoutput('^', tp);
						(void)ttyoutput('\b', tp);
					} else
						ttyecho(c, tp);
				}
				SET(tp->t_state, TS_LNCH);
				goto endcase;
			}
			if (CCEQ(cc[VDISCARD], c)) {
				if (ISSET(lflag, FLUSHO))
					CLR(tp->t_lflag, FLUSHO);
				else {
					ttyflush(tp, FWRITE);
					ttyecho(c, tp);
					if (tp->t_rawq.c_cc + tp->t_canq.c_cc)
						ttyretype(tp);
					SET(tp->t_lflag, FLUSHO);
				}
				goto startoutput;
			}
		}
		/*
		 * Signals.
		 */
		if (ISSET(lflag, ISIG)) {
			if (CCEQ(cc[VINTR], c) || CCEQ(cc[VQUIT], c)) {
				if (!ISSET(lflag, NOFLSH))
					ttyflush(tp, FREAD | FWRITE);
				ttyecho(c, tp);
				if (tp->t_pgrp != NULL) {
					PGRP_LOCK(tp->t_pgrp);
					pgsignal(tp->t_pgrp,
					    CCEQ(cc[VINTR], c) ? SIGINT : SIGQUIT, 1);
					PGRP_UNLOCK(tp->t_pgrp);
				}
				goto endcase;
			}
			if (CCEQ(cc[VSUSP], c)) {
				if (!ISSET(lflag, NOFLSH))
					ttyflush(tp, FREAD);
				ttyecho(c, tp);
				if (tp->t_pgrp != NULL) {
					PGRP_LOCK(tp->t_pgrp);
					pgsignal(tp->t_pgrp, SIGTSTP, 1);
					PGRP_UNLOCK(tp->t_pgrp);
				}
				goto endcase;
			}
		}
		/*
		 * Handle start/stop characters.
		 */
		if (ISSET(iflag, IXON)) {
			if (CCEQ(cc[VSTOP], c)) {
				if (!ISSET(tp->t_state, TS_TTSTOP)) {
					SET(tp->t_state, TS_TTSTOP);
					tt_stop(tp, 0);
					return (0);
				}
				if (!CCEQ(cc[VSTART], c))
					return (0);
				/*
				 * if VSTART == VSTOP then toggle
				 */
				goto endcase;
			}
			if (CCEQ(cc[VSTART], c))
				goto restartoutput;
		}
		/*
		 * IGNCR, ICRNL, & INLCR
		 */
		if (c == '\r') {
			if (ISSET(iflag, IGNCR))
				return (0);
			else if (ISSET(iflag, ICRNL))
				c = '\n';
		} else if (c == '\n' && ISSET(iflag, INLCR))
			c = '\r';
	}
	if (!ISSET(tp->t_lflag, EXTPROC) && ISSET(lflag, ICANON)) {
		/*
		 * From here on down canonical mode character
		 * processing takes place.
		 */
		/*
		 * erase or erase2 (^H / ^?)
		 */
		if (CCEQ(cc[VERASE], c) || CCEQ(cc[VERASE2], c) ) {
			if (tp->t_rawq.c_cc)
				ttyrub(unputc(&tp->t_rawq), tp);
			goto endcase;
		}
		/*
		 * kill (^U)
		 */
		if (CCEQ(cc[VKILL], c)) {
			if (ISSET(lflag, ECHOKE) &&
			    tp->t_rawq.c_cc == tp->t_rocount &&
			    !ISSET(lflag, ECHOPRT))
				while (tp->t_rawq.c_cc)
					ttyrub(unputc(&tp->t_rawq), tp);
			else {
				ttyecho(c, tp);
				if (ISSET(lflag, ECHOK) ||
				    ISSET(lflag, ECHOKE))
					ttyecho('\n', tp);
				FLUSHQ(&tp->t_rawq);
				tp->t_rocount = 0;
			}
			CLR(tp->t_state, TS_LOCAL);
			goto endcase;
		}
		/*
		 * word erase (^W)
		 */
		if (CCEQ(cc[VWERASE], c) && ISSET(lflag, IEXTEN)) {
			int ctype;

			/*
			 * erase whitespace
			 */
			while ((c = unputc(&tp->t_rawq)) == ' ' || c == '\t')
				ttyrub(c, tp);
			if (c == -1)
				goto endcase;
			/*
			 * erase last char of word and remember the
			 * next chars type (for ALTWERASE)
			 */
			ttyrub(c, tp);
			c = unputc(&tp->t_rawq);
			if (c == -1)
				goto endcase;
			if (c == ' ' || c == '\t') {
				(void)putc(c, &tp->t_rawq);
				goto endcase;
			}
			ctype = ISALPHA(c);
			/*
			 * erase rest of word
			 */
			do {
				ttyrub(c, tp);
				c = unputc(&tp->t_rawq);
				if (c == -1)
					goto endcase;
			} while (c != ' ' && c != '\t' &&
			    (!ISSET(lflag, ALTWERASE) || ISALPHA(c) == ctype));
			(void)putc(c, &tp->t_rawq);
			goto endcase;
		}
		/*
		 * reprint line (^R)
		 */
		if (CCEQ(cc[VREPRINT], c) && ISSET(lflag, IEXTEN)) {
			ttyretype(tp);
			goto endcase;
		}
		/*
		 * ^T - kernel info and generate SIGINFO
		 */
		if (CCEQ(cc[VSTATUS], c) && ISSET(lflag, IEXTEN)) {
			if (ISSET(lflag, ISIG) && tp->t_pgrp != NULL) {
				PGRP_LOCK(tp->t_pgrp);
				pgsignal(tp->t_pgrp, SIGINFO, 1);
				PGRP_UNLOCK(tp->t_pgrp);
			}
			if (!ISSET(lflag, NOKERNINFO))
				ttyinfo(tp);
			goto endcase;
		}
	}
	/*
	 * Check for input buffer overflow
	 */
	if (tp->t_rawq.c_cc + tp->t_canq.c_cc >= MAX_INPUT) {
input_overflow:
		if (ISSET(iflag, IMAXBEL)) {
			if (tp->t_outq.c_cc < tp->t_ohiwat)
				(void)ttyoutput(CTRL('g'), tp);
		}
		goto endcase;
	}

	if (   c == 0377 && ISSET(iflag, PARMRK) && !ISSET(iflag, ISTRIP)
	     && ISSET(iflag, IGNBRK|IGNPAR) != (IGNBRK|IGNPAR))
		(void)putc(0377 | TTY_QUOTE, &tp->t_rawq);

	/*
	 * Put data char in q for user and
	 * wakeup on seeing a line delimiter.
	 */
	if (putc(c, &tp->t_rawq) >= 0) {
		if (!ISSET(lflag, ICANON)) {
			ttwakeup(tp);
			ttyecho(c, tp);
			goto endcase;
		}
		if (TTBREAKC(c, lflag)) {
			tp->t_rocount = 0;
			catq(&tp->t_rawq, &tp->t_canq);
			ttwakeup(tp);
		} else if (tp->t_rocount++ == 0)
			tp->t_rocol = tp->t_column;
		if (ISSET(tp->t_state, TS_ERASE)) {
			/*
			 * end of prterase \.../
			 */
			CLR(tp->t_state, TS_ERASE);
			(void)ttyoutput('/', tp);
		}
		i = tp->t_column;
		ttyecho(c, tp);
		if (CCEQ(cc[VEOF], c) && ISSET(lflag, ECHO)) {
			/*
			 * Place the cursor over the '^' of the ^D.
			 */
			i = imin(2, tp->t_column - i);
			while (i > 0) {
				(void)ttyoutput('\b', tp);
				i--;
			}
		}
	}
endcase:
	/*
	 * IXANY means allow any character to restart output.
	 */
	if (ISSET(tp->t_state, TS_TTSTOP) &&
	    !ISSET(iflag, IXANY) && cc[VSTART] != cc[VSTOP])
		return (0);
restartoutput:
	CLR(tp->t_lflag, FLUSHO);
	CLR(tp->t_state, TS_TTSTOP);
startoutput:
	return (ttstart(tp));
}

/*
 * Output a single character on a tty, doing output processing
 * as needed (expanding tabs, newline processing, etc.).
 * Returns < 0 if succeeds, otherwise returns char to resend.
 * Must be recursive.
 */
static int
ttyoutput(int c, struct tty *tp)
{
	tcflag_t oflag;
	int col, s;

	oflag = tp->t_oflag;
	if (!ISSET(oflag, OPOST)) {
		if (ISSET(tp->t_lflag, FLUSHO))
			return (-1);
		if (putc(c, &tp->t_outq))
			return (c);
		tk_nout++;
		tp->t_outcc++;
		return (-1);
	}
	/*
	 * Do tab expansion if OXTABS is set.  Special case if we external
	 * processing, we don't do the tab expansion because we'll probably
	 * get it wrong.  If tab expansion needs to be done, let it happen
	 * externally.
	 */
	CLR(c, ~TTY_CHARMASK);
	if (c == '\t' &&
	    ISSET(oflag, OXTABS) && !ISSET(tp->t_lflag, EXTPROC)) {
		c = 8 - (tp->t_column & 7);
		if (!ISSET(tp->t_lflag, FLUSHO)) {
			s = spltty();		/* Don't interrupt tabs. */
			c -= b_to_q("        ", c, &tp->t_outq);
			tk_nout += c;
			tp->t_outcc += c;
			splx(s);
		}
		tp->t_column += c;
		return (c ? -1 : '\t');
	}
	if (c == CEOT && ISSET(oflag, ONOEOT))
		return (-1);

	/*
	 * Newline translation: if ONLCR is set,
	 * translate newline into "\r\n".
	 */
	if (c == '\n' && ISSET(tp->t_oflag, ONLCR)) {
		tk_nout++;
		tp->t_outcc++;
		if (!ISSET(tp->t_lflag, FLUSHO) && putc('\r', &tp->t_outq))
			return (c);
	}
	/* If OCRNL is set, translate "\r" into "\n". */
	else if (c == '\r' && ISSET(tp->t_oflag, OCRNL))
		c = '\n';
	/* If ONOCR is set, don't transmit CRs when on column 0. */
	else if (c == '\r' && ISSET(tp->t_oflag, ONOCR) && tp->t_column == 0)
		return (-1);

	tk_nout++;
	tp->t_outcc++;
	if (!ISSET(tp->t_lflag, FLUSHO) && putc(c, &tp->t_outq))
		return (c);

	col = tp->t_column;
	switch (CCLASS(c)) {
	case BACKSPACE:
		if (col > 0)
			--col;
		break;
	case CONTROL:
		break;
	case NEWLINE:
		if (ISSET(tp->t_oflag, ONLCR | ONLRET))
			col = 0;
		break;
	case RETURN:
		col = 0;
		break;
	case ORDINARY:
		++col;
		break;
	case TAB:
		col = (col + 8) & ~7;
		break;
	}
	tp->t_column = col;
	return (-1);
}

/*
 * Ioctls for all tty devices.  Called after line-discipline specific ioctl
 * has been called to do discipline-specific functions and/or reject any
 * of these ioctl commands.
 */
/* ARGSUSED */
int
ttioctl(struct tty *tp, u_long cmd, void *data, int flag)
{
	struct proc *p;
	struct thread *td;
	struct pgrp *pgrp;
	int s, error, bits, sig, sig2;

	td = curthread;			/* XXX */
	p = td->td_proc;

	/* If the ioctl involves modification, hang if in the background. */
	switch (cmd) {
	case  TIOCCBRK:
	case  TIOCCONS:
	case  TIOCDRAIN:
	case  TIOCEXCL:
	case  TIOCFLUSH:
#ifdef TIOCHPCL
	case  TIOCHPCL:
#endif
	case  TIOCNXCL:
	case  TIOCSBRK:
	case  TIOCSCTTY:
	case  TIOCSDRAINWAIT:
	case  TIOCSETA:
	case  TIOCSETAF:
	case  TIOCSETAW:
	case  TIOCSETD:
	case  TIOCSPGRP:
	case  TIOCSTART:
	case  TIOCSTAT:
	case  TIOCSTI:
	case  TIOCSTOP:
	case  TIOCSWINSZ:
#if defined(COMPAT_43TTY)
	case  TIOCLBIC:
	case  TIOCLBIS:
	case  TIOCLSET:
	case  TIOCSETC:
	case OTIOCSETD:
	case  TIOCSETN:
	case  TIOCSETP:
	case  TIOCSLTC:
#endif
		sx_slock(&proctree_lock);
		PROC_LOCK(p);
		while (isbackground(p, tp) && !(p->p_flag & P_PPWAIT) &&
		    !SIGISMEMBER(p->p_sigacts->ps_sigignore, SIGTTOU) &&
		    !SIGISMEMBER(td->td_sigmask, SIGTTOU)) {
			pgrp = p->p_pgrp;
			PROC_UNLOCK(p);
			if (pgrp->pg_jobc == 0) {
				sx_sunlock(&proctree_lock);
				return (EIO);
			}
			PGRP_LOCK(pgrp);
			sx_sunlock(&proctree_lock);
			pgsignal(pgrp, SIGTTOU, 1);
			PGRP_UNLOCK(pgrp);
			error = ttysleep(tp, &lbolt, TTOPRI | PCATCH, "ttybg1",
					 0);
			if (error)
				return (error);
			sx_slock(&proctree_lock);
			PROC_LOCK(p);
		}
		PROC_UNLOCK(p);
		sx_sunlock(&proctree_lock);
		break;
	}


	if (tp->t_modem != NULL) {
		switch (cmd) {
		case TIOCSDTR:
			tt_modem(tp, SER_DTR, 0);
			return (0);
		case TIOCCDTR:
			tt_modem(tp, 0, SER_DTR);
			return (0);
		case TIOCMSET:
			bits = *(int *)data;
			sig = (bits & (TIOCM_DTR | TIOCM_RTS)) >> 1;
			sig2 = ((~bits) & (TIOCM_DTR | TIOCM_RTS)) >> 1;
			tt_modem(tp, sig, sig2);
			return (0);
		case TIOCMBIS:
			bits = *(int *)data;
			sig = (bits & (TIOCM_DTR | TIOCM_RTS)) >> 1;
			tt_modem(tp, sig, 0);
			return (0);
		case TIOCMBIC:
			bits = *(int *)data;
			sig = (bits & (TIOCM_DTR | TIOCM_RTS)) >> 1;
			tt_modem(tp, 0, sig);
			return (0);
		case TIOCMGET:
			sig = tt_modem(tp, 0, 0);
			/* See <sys/serial.h. for the "<< 1" stuff */
			bits = TIOCM_LE + (sig << 1);
			*(int *)data = bits;
			return (0);
		default:
			break;
		}
	}

	if (tp->t_pps != NULL) {
		error = pps_ioctl(cmd, data, tp->t_pps);
		if (error != ENOIOCTL)
			return (error);
	}

	switch (cmd) {			/* Process the ioctl. */
	case FIOASYNC:			/* set/clear async i/o */
		s = spltty();
		if (*(int *)data)
			SET(tp->t_state, TS_ASYNC);
		else
			CLR(tp->t_state, TS_ASYNC);
		splx(s);
		break;
	case FIONBIO:			/* set/clear non-blocking i/o */
		break;			/* XXX: delete. */
	case FIONREAD:			/* get # bytes to read */
		s = spltty();
		*(int *)data = ttnread(tp);
		splx(s);
		break;

	case FIOSETOWN:
		/*
		 * Policy -- Don't allow FIOSETOWN on someone else's
		 *           controlling tty
		 */
		if (tp->t_session != NULL && !isctty(p, tp))
			return (ENOTTY);

		error = fsetown(*(int *)data, &tp->t_sigio);
		if (error)
			return (error);
		break;
	case FIOGETOWN:
		if (tp->t_session != NULL && !isctty(p, tp))
			return (ENOTTY);
		*(int *)data = fgetown(&tp->t_sigio);
		break;

	case TIOCEXCL:			/* set exclusive use of tty */
		s = spltty();
		SET(tp->t_state, TS_XCLUDE);
		splx(s);
		break;
	case TIOCFLUSH: {		/* flush buffers */
		int flags = *(int *)data;

		if (flags == 0)
			flags = FREAD | FWRITE;
		else
			flags &= FREAD | FWRITE;
		ttyflush(tp, flags);
		break;
	}
	case TIOCCONS:			/* become virtual console */
		if (*(int *)data) {
			struct nameidata nid;

			if (constty && constty != tp &&
			    ISSET(constty->t_state, TS_CONNECTED))
				return (EBUSY);

			/* Ensure user can open the real console. */
			NDINIT(&nid, LOOKUP, LOCKLEAF | FOLLOW, UIO_SYSSPACE,
			    "/dev/console", td);
			if ((error = namei(&nid)) != 0)
				return (error);
			NDFREE(&nid, NDF_ONLY_PNBUF);
			error = VOP_ACCESS(nid.ni_vp, VREAD, td->td_ucred, td);
			vput(nid.ni_vp);
			if (error)
				return (error);

			constty_set(tp);
		} else if (tp == constty)
			constty_clear();
		break;
	case TIOCDRAIN:			/* wait till output drained */
		error = ttywait(tp);
		if (error)
			return (error);
		break;
	case TIOCGETA: {		/* get termios struct */
		struct termios *t = (struct termios *)data;

		bcopy(&tp->t_termios, t, sizeof(struct termios));
		break;
	}
	case TIOCGETD:			/* get line discipline */
		*(int *)data = tp->t_line;
		break;
	case TIOCGWINSZ:		/* get window size */
		*(struct winsize *)data = tp->t_winsize;
		break;
	case TIOCGPGRP:			/* get pgrp of tty */
		if (!isctty(p, tp))
			return (ENOTTY);
		*(int *)data = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		break;
#ifdef TIOCHPCL
	case TIOCHPCL:			/* hang up on last close */
		s = spltty();
		SET(tp->t_cflag, HUPCL);
		splx(s);
		break;
#endif
	case TIOCMGDTRWAIT:
		*(int *)data = tp->t_dtr_wait * 100 / hz;
		break;
	case TIOCMSDTRWAIT:
		/* must be root since the wait applies to following logins */
		error = priv_check(td, PRIV_TTY_DTRWAIT);
		if (error)
			return (error);
		tp->t_dtr_wait = *(int *)data * hz / 100;
		break;
	case TIOCNXCL:			/* reset exclusive use of tty */
		s = spltty();
		CLR(tp->t_state, TS_XCLUDE);
		splx(s);
		break;
	case TIOCOUTQ:			/* output queue size */
		*(int *)data = tp->t_outq.c_cc;
		break;
	case TIOCSETA:			/* set termios struct */
	case TIOCSETAW:			/* drain output, set */
	case TIOCSETAF: {		/* drn out, fls in, set */
		struct termios *t = (struct termios *)data;

		if (t->c_ispeed == 0)
			t->c_ispeed = t->c_ospeed;
		if (t->c_ispeed == 0)
			t->c_ispeed = tp->t_ospeed;
		if (t->c_ispeed == 0)
			return (EINVAL);
		s = spltty();
		if (cmd == TIOCSETAW || cmd == TIOCSETAF) {
			error = ttywait(tp);
			if (error) {
				splx(s);
				return (error);
			}
			if (cmd == TIOCSETAF)
				ttyflush(tp, FREAD);
		}
		if (!ISSET(t->c_cflag, CIGNORE)) {
			/*
			 * Set device hardware.
			 */
			error = tt_param(tp, t);
			if (error) {
				splx(s);
				return (error);
			}
			if (ISSET(t->c_cflag, CLOCAL) &&
			    !ISSET(tp->t_cflag, CLOCAL)) {
				/*
				 * XXX disconnections would be too hard to
				 * get rid of without this kludge.  The only
				 * way to get rid of controlling terminals
				 * is to exit from the session leader.
				 */
				CLR(tp->t_state, TS_ZOMBIE);

				wakeup(TSA_CARR_ON(tp));
				ttwakeup(tp);
				ttwwakeup(tp);
			}
			if ((ISSET(tp->t_state, TS_CARR_ON) ||
			     ISSET(t->c_cflag, CLOCAL)) &&
			    !ISSET(tp->t_state, TS_ZOMBIE))
				SET(tp->t_state, TS_CONNECTED);
			else
				CLR(tp->t_state, TS_CONNECTED);
			tp->t_cflag = t->c_cflag;
			tp->t_ispeed = t->c_ispeed;
			if (t->c_ospeed != 0)
				tp->t_ospeed = t->c_ospeed;
			ttsetwater(tp);
		}
		if (ISSET(t->c_lflag, ICANON) != ISSET(tp->t_lflag, ICANON) &&
		    cmd != TIOCSETAF) {
			if (ISSET(t->c_lflag, ICANON))
				SET(tp->t_lflag, PENDIN);
			else {
				/*
				 * XXX we really shouldn't allow toggling
				 * ICANON while we're in a non-termios line
				 * discipline.  Now we have to worry about
				 * panicing for a null queue.
				 */
				if (tp->t_canq.c_cbreserved > 0 &&
				    tp->t_rawq.c_cbreserved > 0) {
					catq(&tp->t_rawq, &tp->t_canq);
					/*
					 * XXX the queue limits may be
					 * different, so the old queue
					 * swapping method no longer works.
					 */
					catq(&tp->t_canq, &tp->t_rawq);
				}
				CLR(tp->t_lflag, PENDIN);
			}
			ttwakeup(tp);
		}
		tp->t_iflag = t->c_iflag;
		tp->t_oflag = t->c_oflag;
		/*
		 * Make the EXTPROC bit read only.
		 */
		if (ISSET(tp->t_lflag, EXTPROC))
			SET(t->c_lflag, EXTPROC);
		else
			CLR(t->c_lflag, EXTPROC);
		tp->t_lflag = t->c_lflag | ISSET(tp->t_lflag, PENDIN);
		if (t->c_cc[VMIN] != tp->t_cc[VMIN] ||
		    t->c_cc[VTIME] != tp->t_cc[VTIME])
			ttwakeup(tp);
		bcopy(t->c_cc, tp->t_cc, sizeof(t->c_cc));
		splx(s);
		break;
	}
	case TIOCSETD: {		/* set line discipline */
		int t = *(int *)data;

		if ((u_int)t >= nlinesw)
			return (ENXIO);
		if (t == tp->t_line)
			return (0);
		s = spltty();
		ttyld_close(tp, flag);
		tp->t_line = t;
		/* XXX: we should use the correct cdev here */
		error = ttyld_open(tp, tp->t_dev);
		if (error) {
			/*
			 * If we fail to switch line discipline we cannot
			 * fall back to the previous, because we can not
			 * trust that ldisc to open successfully either.
			 * Fall back to the default ldisc which we know 
			 * will allways succeed.
			 */
			tp->t_line = TTYDISC;
			(void)ttyld_open(tp, tp->t_dev);
		}
		splx(s);
		return (error);
		break;
	}
	case TIOCSTART:			/* start output, like ^Q */
		s = spltty();
		if (ISSET(tp->t_state, TS_TTSTOP) ||
		    ISSET(tp->t_lflag, FLUSHO)) {
			CLR(tp->t_lflag, FLUSHO);
			CLR(tp->t_state, TS_TTSTOP);
			ttstart(tp);
		}
		splx(s);
		break;
	case TIOCSTI:			/* simulate terminal input */
		if ((flag & FREAD) == 0 && priv_check(td, PRIV_TTY_STI))
			return (EPERM);
		if (!isctty(p, tp) && priv_check(td, PRIV_TTY_STI))
			return (EACCES);
		s = spltty();
		ttyld_rint(tp, *(u_char *)data);
		splx(s);
		break;
	case TIOCSTOP:			/* stop output, like ^S */
		s = spltty();
		if (!ISSET(tp->t_state, TS_TTSTOP)) {
			SET(tp->t_state, TS_TTSTOP);
			tt_stop(tp, 0);
		}
		splx(s);
		break;
	case TIOCSCTTY:			/* become controlling tty */
		/* Session ctty vnode pointer set in vnode layer. */
		sx_slock(&proctree_lock);
		if (!SESS_LEADER(p) ||
		    ((p->p_session->s_ttyvp || tp->t_session) &&
		     (tp->t_session != p->p_session))) {
			sx_sunlock(&proctree_lock);
			return (EPERM);
		}
		tp->t_session = p->p_session;
		tp->t_pgrp = p->p_pgrp;
		SESS_LOCK(p->p_session);
		ttyref(tp);		/* ttyrel(): kern_proc.c:pgdelete() */
		p->p_session->s_ttyp = tp;
		SESS_UNLOCK(p->p_session);
		PROC_LOCK(p);
		p->p_flag |= P_CONTROLT;
		PROC_UNLOCK(p);
		sx_sunlock(&proctree_lock);
		break;
	case TIOCSPGRP: {		/* set pgrp of tty */
		sx_slock(&proctree_lock);
		pgrp = pgfind(*(int *)data);
		if (!isctty(p, tp)) {
			if (pgrp != NULL)
				PGRP_UNLOCK(pgrp);
			sx_sunlock(&proctree_lock);
			return (ENOTTY);
		}
		if (pgrp == NULL) {
			sx_sunlock(&proctree_lock);
			return (EPERM);
		}
		PGRP_UNLOCK(pgrp);
		if (pgrp->pg_session != p->p_session) {
			sx_sunlock(&proctree_lock);
			return (EPERM);
		}
		sx_sunlock(&proctree_lock);
		tp->t_pgrp = pgrp;
		break;
	}
	case TIOCSTAT:			/* simulate control-T */
		s = spltty();
		ttyinfo(tp);
		splx(s);
		break;
	case TIOCSWINSZ:		/* set window size */
		if (bcmp((caddr_t)&tp->t_winsize, data,
		    sizeof (struct winsize))) {
			tp->t_winsize = *(struct winsize *)data;
			if (tp->t_pgrp != NULL) {
				PGRP_LOCK(tp->t_pgrp);
				pgsignal(tp->t_pgrp, SIGWINCH, 1);
				PGRP_UNLOCK(tp->t_pgrp);
			}
		}
		break;
	case TIOCSDRAINWAIT:
		error = priv_check(td, PRIV_TTY_DRAINWAIT);
		if (error)
			return (error);
		tp->t_timeout = *(int *)data * hz;
		wakeup(TSA_OCOMPLETE(tp));
		wakeup(TSA_OLOWAT(tp));
		break;
	case TIOCGDRAINWAIT:
		*(int *)data = tp->t_timeout / hz;
		break;
	case TIOCSBRK:
		return (tt_break(tp, 1));
	case TIOCCBRK:
		return (tt_break(tp, 0));
	default:
#if defined(COMPAT_43TTY)
		return (ttcompat(tp, cmd, data, flag));
#else
		return (ENOIOCTL);
#endif
	}
	return (0);
}

int
ttypoll(struct cdev *dev, int events, struct thread *td)
{
	int s;
	int revents = 0;
	struct tty *tp;

	tp = tty_gettp(dev);

	if (tp == NULL)	/* XXX used to return ENXIO, but that means true! */
		return ((events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM))
			| POLLHUP);

	s = spltty();
	if (events & (POLLIN | POLLRDNORM)) {
		if (ISSET(tp->t_state, TS_ZOMBIE))
			revents |= (events & (POLLIN | POLLRDNORM)) |
			    POLLHUP;
		else if (ttnread(tp) > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &tp->t_rsel);
	}
	if (events & POLLOUT) {
		if (ISSET(tp->t_state, TS_ZOMBIE))
			revents |= POLLHUP;
		else if (tp->t_outq.c_cc <= tp->t_olowat &&
		    ISSET(tp->t_state, TS_CONNECTED))
			revents |= events & POLLOUT;
		else
			selrecord(td, &tp->t_wsel);
	}
	splx(s);
	return (revents);
}

static struct filterops ttyread_filtops =
	{ 1, NULL, filt_ttyrdetach, filt_ttyread };
static struct filterops ttywrite_filtops =
	{ 1, NULL, filt_ttywdetach, filt_ttywrite };

int
ttykqfilter(struct cdev *dev, struct knote *kn)
{
	struct tty *tp;
	struct knlist *klist;
	int s;

	tp = tty_gettp(dev);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &tp->t_rsel.si_note;
		kn->kn_fop = &ttyread_filtops;
		break;
	case EVFILT_WRITE:
		klist = &tp->t_wsel.si_note;
		kn->kn_fop = &ttywrite_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = (caddr_t)dev;

	s = spltty();
	knlist_add(klist, kn, 0);
	splx(s);

	return (0);
}

static void
filt_ttyrdetach(struct knote *kn)
{
	struct tty *tp = ((struct cdev *)kn->kn_hook)->si_tty;
	int s = spltty();

	knlist_remove(&tp->t_rsel.si_note, kn, 0);
	splx(s);
}

static int
filt_ttyread(struct knote *kn, long hint)
{
	struct tty *tp = ((struct cdev *)kn->kn_hook)->si_tty;

	kn->kn_data = ttnread(tp);
	if (ISSET(tp->t_state, TS_ZOMBIE)) {
		kn->kn_flags |= EV_EOF;
		return (1);
	}
	return (kn->kn_data > 0);
}

static void
filt_ttywdetach(struct knote *kn)
{
	struct tty *tp = ((struct cdev *)kn->kn_hook)->si_tty;
	int s = spltty();

	knlist_remove(&tp->t_wsel.si_note, kn, 0);
	splx(s);
}

static int
filt_ttywrite(struct knote *kn, long hint)
{
	struct tty *tp = ((struct cdev *)kn->kn_hook)->si_tty;

	kn->kn_data = tp->t_outq.c_cc;
	if (ISSET(tp->t_state, TS_ZOMBIE))
		return (1);
	return (kn->kn_data <= tp->t_olowat &&
	    ISSET(tp->t_state, TS_CONNECTED));
}

/*
 * Must be called at spltty().
 */
static int
ttnread(struct tty *tp)
{
	int nread;

	if (ISSET(tp->t_lflag, PENDIN))
		ttypend(tp);
	nread = tp->t_canq.c_cc;
	if (!ISSET(tp->t_lflag, ICANON)) {
		nread += tp->t_rawq.c_cc;
		if (nread < tp->t_cc[VMIN] && tp->t_cc[VTIME] == 0)
			nread = 0;
	}
	return (nread);
}

/*
 * Wait for output to drain.
 */
int
ttywait(struct tty *tp)
{
	int error, s;

	error = 0;
	s = spltty();
	while ((tp->t_outq.c_cc || ISSET(tp->t_state, TS_BUSY)) &&
	       ISSET(tp->t_state, TS_CONNECTED) && tp->t_oproc) {
		tt_oproc(tp);
		if ((tp->t_outq.c_cc || ISSET(tp->t_state, TS_BUSY)) &&
		    ISSET(tp->t_state, TS_CONNECTED)) {
			SET(tp->t_state, TS_SO_OCOMPLETE);
			error = ttysleep(tp, TSA_OCOMPLETE(tp),
					 TTOPRI | PCATCH, "ttywai",
					 tp->t_timeout);
			if (error) {
				if (error == EWOULDBLOCK)
					error = EIO;
				break;
			}
		} else
			break;
	}
	if (!error && (tp->t_outq.c_cc || ISSET(tp->t_state, TS_BUSY)))
		error = EIO;
	splx(s);
	return (error);
}

/*
 * Flush if successfully wait.
 */
static int
ttywflush(struct tty *tp)
{
	int error;

	if ((error = ttywait(tp)) == 0)
		ttyflush(tp, FREAD);
	return (error);
}

/*
 * Flush tty read and/or write queues, notifying anyone waiting.
 */
void
ttyflush(struct tty *tp, int rw)
{
	int s;

	s = spltty();
#if 0
again:
#endif
	if (rw & FWRITE) {
		FLUSHQ(&tp->t_outq);
		CLR(tp->t_state, TS_TTSTOP);
	}
	tt_stop(tp, rw);
	if (rw & FREAD) {
		FLUSHQ(&tp->t_canq);
		FLUSHQ(&tp->t_rawq);
		CLR(tp->t_lflag, PENDIN);
		tp->t_rocount = 0;
		tp->t_rocol = 0;
		CLR(tp->t_state, TS_LOCAL);
		ttwakeup(tp);
		if (ISSET(tp->t_state, TS_TBLOCK)) {
			if (rw & FWRITE)
				FLUSHQ(&tp->t_outq);
			ttyunblock(tp);

			/*
			 * Don't let leave any state that might clobber the
			 * next line discipline (although we should do more
			 * to send the START char).  Not clearing the state
			 * may have caused the "putc to a clist with no
			 * reserved cblocks" panic/printf.
			 */
			CLR(tp->t_state, TS_TBLOCK);

#if 0 /* forget it, sleeping isn't always safe and we don't know when it is */
			if (ISSET(tp->t_iflag, IXOFF)) {
				/*
				 * XXX wait a bit in the hope that the stop
				 * character (if any) will go out.  Waiting
				 * isn't good since it allows races.  This
				 * will be fixed when the stop character is
				 * put in a special queue.  Don't bother with
				 * the checks in ttywait() since the timeout
				 * will save us.
				 */
				SET(tp->t_state, TS_SO_OCOMPLETE);
				ttysleep(tp, TSA_OCOMPLETE(tp), TTOPRI,
					 "ttyfls", hz / 10);
				/*
				 * Don't try sending the stop character again.
				 */
				CLR(tp->t_state, TS_TBLOCK);
				goto again;
			}
#endif
		}
	}
	if (rw & FWRITE) {
		FLUSHQ(&tp->t_outq);
		ttwwakeup(tp);
	}
	splx(s);
}

/*
 * Copy in the default termios characters.
 */
void
termioschars(struct termios *t)
{

	bcopy(ttydefchars, t->c_cc, sizeof t->c_cc);
}

/*
 * Old interface.
 */
void
ttychars(struct tty *tp)
{

	termioschars(&tp->t_termios);
}

/*
 * Handle input high water.  Send stop character for the IXOFF case.  Turn
 * on our input flow control bit and propagate the changes to the driver.
 * XXX the stop character should be put in a special high priority queue.
 */
void
ttyblock(struct tty *tp)
{

	SET(tp->t_state, TS_TBLOCK);
	if (ISSET(tp->t_iflag, IXOFF) && tp->t_cc[VSTOP] != _POSIX_VDISABLE &&
	    putc(tp->t_cc[VSTOP], &tp->t_outq) != 0)
		CLR(tp->t_state, TS_TBLOCK);	/* try again later */
	ttstart(tp);
}

/*
 * Handle input low water.  Send start character for the IXOFF case.  Turn
 * off our input flow control bit and propagate the changes to the driver.
 * XXX the start character should be put in a special high priority queue.
 */
static void
ttyunblock(struct tty *tp)
{

	CLR(tp->t_state, TS_TBLOCK);
	if (ISSET(tp->t_iflag, IXOFF) && tp->t_cc[VSTART] != _POSIX_VDISABLE &&
	    putc(tp->t_cc[VSTART], &tp->t_outq) != 0)
		SET(tp->t_state, TS_TBLOCK);	/* try again later */
	ttstart(tp);
}

#ifdef notyet
/* Not used by any current (i386) drivers. */
/*
 * Restart after an inter-char delay.
 */
void
ttrstrt(void *tp_arg)
{
	struct tty *tp;
	int s;

	KASSERT(tp_arg != NULL, ("ttrstrt"));

	tp = tp_arg;
	s = spltty();

	CLR(tp->t_state, TS_TIMEOUT);
	ttstart(tp);

	splx(s);
}
#endif

int
ttstart(struct tty *tp)
{

	tt_oproc(tp);
	return (0);
}

/*
 * "close" a line discipline
 */
int
ttylclose(struct tty *tp, int flag)
{

	if (flag & FNONBLOCK || ttywflush(tp))
		ttyflush(tp, FREAD | FWRITE);
	return (0);
}

/*
 * Handle modem control transition on a tty.
 * Flag indicates new state of carrier.
 * Returns 0 if the line should be turned off, otherwise 1.
 */
int
ttymodem(struct tty *tp, int flag)
{

	if (ISSET(tp->t_state, TS_CARR_ON) && ISSET(tp->t_cflag, MDMBUF)) {
		/*
		 * MDMBUF: do flow control according to carrier flag
		 * XXX TS_CAR_OFLOW doesn't do anything yet.  TS_TTSTOP
		 * works if IXON and IXANY are clear.
		 */
		if (flag) {
			CLR(tp->t_state, TS_CAR_OFLOW);
			CLR(tp->t_state, TS_TTSTOP);
			ttstart(tp);
		} else if (!ISSET(tp->t_state, TS_CAR_OFLOW)) {
			SET(tp->t_state, TS_CAR_OFLOW);
			SET(tp->t_state, TS_TTSTOP);
			tt_stop(tp, 0);
		}
	} else if (flag == 0) {
		/*
		 * Lost carrier.
		 */
		CLR(tp->t_state, TS_CARR_ON);
		if (ISSET(tp->t_state, TS_ISOPEN) &&
		    !ISSET(tp->t_cflag, CLOCAL)) {
			SET(tp->t_state, TS_ZOMBIE);
			CLR(tp->t_state, TS_CONNECTED);
			if (tp->t_session) {
				sx_slock(&proctree_lock);
				if (tp->t_session && tp->t_session->s_leader) {
					struct proc *p;

					p = tp->t_session->s_leader;
					PROC_LOCK(p);
					psignal(p, SIGHUP);
					PROC_UNLOCK(p);
				}
				sx_sunlock(&proctree_lock);
			}
			ttyflush(tp, FREAD | FWRITE);
			return (0);
		}
	} else {
		/*
		 * Carrier now on.
		 */
		SET(tp->t_state, TS_CARR_ON);
		if (!ISSET(tp->t_state, TS_ZOMBIE))
			SET(tp->t_state, TS_CONNECTED);
		wakeup(TSA_CARR_ON(tp));
		ttwakeup(tp);
		ttwwakeup(tp);
	}
	return (1);
}

/*
 * Reinput pending characters after state switch
 * call at spltty().
 */
static void
ttypend(struct tty *tp)
{
	struct clist tq;
	int c;

	CLR(tp->t_lflag, PENDIN);
	SET(tp->t_state, TS_TYPEN);
	/*
	 * XXX this assumes too much about clist internals.  It may even
	 * fail if the cblock slush pool is empty.  We can't allocate more
	 * cblocks here because we are called from an interrupt handler
	 * and clist_alloc_cblocks() can wait.
	 */
	tq = tp->t_rawq;
	bzero(&tp->t_rawq, sizeof tp->t_rawq);
	tp->t_rawq.c_cbmax = tq.c_cbmax;
	tp->t_rawq.c_cbreserved = tq.c_cbreserved;
	while ((c = getc(&tq)) >= 0)
		ttyinput(c, tp);
	CLR(tp->t_state, TS_TYPEN);
}

/*
 * Process a read call on a tty device.
 */
int
ttread(struct tty *tp, struct uio *uio, int flag)
{
	struct clist *qp;
	int c;
	tcflag_t lflag;
	cc_t *cc = tp->t_cc;
	struct thread *td;
	struct proc *p;
	int s, first, error = 0;
	int has_stime = 0, last_cc = 0;
	long slp = 0;		/* XXX this should be renamed `timo'. */
	struct timeval stime = { 0, 0 };
	struct pgrp *pg;

	td = curthread;
	p = td->td_proc;
loop:
	s = spltty();
	lflag = tp->t_lflag;
	/*
	 * take pending input first
	 */
	if (ISSET(lflag, PENDIN)) {
		ttypend(tp);
		splx(s);	/* reduce latency */
		s = spltty();
		lflag = tp->t_lflag;	/* XXX ttypend() clobbers it */
	}

	/*
	 * Hang process if it's in the background.
	 */
	if (isbackground(p, tp)) {
		splx(s);
		sx_slock(&proctree_lock);
		PROC_LOCK(p);
		if (SIGISMEMBER(p->p_sigacts->ps_sigignore, SIGTTIN) ||
		    SIGISMEMBER(td->td_sigmask, SIGTTIN) ||
		    (p->p_flag & P_PPWAIT) || p->p_pgrp->pg_jobc == 0) {
			PROC_UNLOCK(p);
			sx_sunlock(&proctree_lock);
			return (EIO);
		}
		pg = p->p_pgrp;
		PROC_UNLOCK(p);
		PGRP_LOCK(pg);
		sx_sunlock(&proctree_lock);
		pgsignal(pg, SIGTTIN, 1);
		PGRP_UNLOCK(pg);
		error = ttysleep(tp, &lbolt, TTIPRI | PCATCH, "ttybg2", 0);
		if (error)
			return (error);
		goto loop;
	}

	if (ISSET(tp->t_state, TS_ZOMBIE)) {
		splx(s);
		return (0);	/* EOF */
	}

	/*
	 * If canonical, use the canonical queue,
	 * else use the raw queue.
	 *
	 * (should get rid of clists...)
	 */
	qp = ISSET(lflag, ICANON) ? &tp->t_canq : &tp->t_rawq;

	if (flag & IO_NDELAY) {
		if (qp->c_cc > 0)
			goto read;
		if (!ISSET(lflag, ICANON) && cc[VMIN] == 0) {
			splx(s);
			return (0);
		}
		splx(s);
		return (EWOULDBLOCK);
	}
	if (!ISSET(lflag, ICANON)) {
		int m = cc[VMIN];
		long t = cc[VTIME];
		struct timeval timecopy;

		/*
		 * Check each of the four combinations.
		 * (m > 0 && t == 0) is the normal read case.
		 * It should be fairly efficient, so we check that and its
		 * companion case (m == 0 && t == 0) first.
		 * For the other two cases, we compute the target sleep time
		 * into slp.
		 */
		if (t == 0) {
			if (qp->c_cc < m)
				goto sleep;
			if (qp->c_cc > 0)
				goto read;

			/* m, t and qp->c_cc are all 0.  0 is enough input. */
			splx(s);
			return (0);
		}
		t *= 100000;		/* time in us */
#define diff(t1, t2) (((t1).tv_sec - (t2).tv_sec) * 1000000 + \
			 ((t1).tv_usec - (t2).tv_usec))
		if (m > 0) {
			if (qp->c_cc <= 0)
				goto sleep;
			if (qp->c_cc >= m)
				goto read;
			getmicrotime(&timecopy);
			if (!has_stime) {
				/* first character, start timer */
				has_stime = 1;
				stime = timecopy;
				slp = t;
			} else if (qp->c_cc > last_cc) {
				/* got a character, restart timer */
				stime = timecopy;
				slp = t;
			} else {
				/* nothing, check expiration */
				slp = t - diff(timecopy, stime);
				if (slp <= 0)
					goto read;
			}
			last_cc = qp->c_cc;
		} else {	/* m == 0 */
			if (qp->c_cc > 0)
				goto read;
			getmicrotime(&timecopy);
			if (!has_stime) {
				has_stime = 1;
				stime = timecopy;
				slp = t;
			} else {
				slp = t - diff(timecopy, stime);
				if (slp <= 0) {
					/* Timed out, but 0 is enough input. */
					splx(s);
					return (0);
				}
			}
		}
#undef diff
		if (slp != 0) {
			struct timeval tv;	/* XXX style bug. */

			tv.tv_sec = slp / 1000000;
			tv.tv_usec = slp % 1000000;
			slp = tvtohz(&tv);
			/*
			 * XXX bad variable names.  slp was the timeout in
			 * usec.  Now it is the timeout in ticks.
			 */
		}
		goto sleep;
	}
	if (qp->c_cc <= 0) {
sleep:
		/*
		 * There is no input, or not enough input and we can block.
		 */
		error = ttysleep(tp, TSA_HUP_OR_INPUT(tp), TTIPRI | PCATCH,
				 ISSET(tp->t_state, TS_CONNECTED) ?
				 "ttyin" : "ttyhup", (int)slp);
		splx(s);
		if (error == EWOULDBLOCK)
			error = 0;
		else if (error)
			return (error);
		/*
		 * XXX what happens if another process eats some input
		 * while we are asleep (not just here)?  It would be
		 * safest to detect changes and reset our state variables
		 * (has_stime and last_cc).
		 */
		slp = 0;
		goto loop;
	}
read:
	splx(s);
	/*
	 * Input present, check for input mapping and processing.
	 */
	first = 1;
	if (ISSET(lflag, ICANON | ISIG))
		goto slowcase;
	for (;;) {
		char ibuf[IBUFSIZ];
		int icc;

		icc = imin(uio->uio_resid, IBUFSIZ);
		icc = q_to_b(qp, ibuf, icc);
		if (icc <= 0) {
			if (first)
				goto loop;
			break;
		}
		error = uiomove(ibuf, icc, uio);
		/*
		 * XXX if there was an error then we should ungetc() the
		 * unmoved chars and reduce icc here.
		 */
		if (error)
			break;
		if (uio->uio_resid == 0)
			break;
		first = 0;
	}
	goto out;
slowcase:
	for (;;) {
		c = getc(qp);
		if (c < 0) {
			if (first)
				goto loop;
			break;
		}
		/*
		 * delayed suspend (^Y)
		 */
		if (CCEQ(cc[VDSUSP], c) &&
		    ISSET(lflag, IEXTEN | ISIG) == (IEXTEN | ISIG)) {
			if (tp->t_pgrp != NULL) {
				PGRP_LOCK(tp->t_pgrp);
				pgsignal(tp->t_pgrp, SIGTSTP, 1);
				PGRP_UNLOCK(tp->t_pgrp);
			}
			if (first) {
				error = ttysleep(tp, &lbolt, TTIPRI | PCATCH,
						 "ttybg3", 0);
				if (error)
					break;
				goto loop;
			}
			break;
		}
		/*
		 * Interpret EOF only in canonical mode.
		 */
		if (CCEQ(cc[VEOF], c) && ISSET(lflag, ICANON))
			break;
		/*
		 * Give user character.
		 */
		error = ureadc(c, uio);
		if (error)
			/* XXX should ungetc(c, qp). */
			break;
		if (uio->uio_resid == 0)
			break;
		/*
		 * In canonical mode check for a "break character"
		 * marking the end of a "line of input".
		 */
		if (ISSET(lflag, ICANON) && TTBREAKC(c, lflag))
			break;
		first = 0;
	}

out:
	/*
	 * Look to unblock input now that (presumably)
	 * the input queue has gone down.
	 */
	s = spltty();
	if (ISSET(tp->t_state, TS_TBLOCK) &&
	    tp->t_rawq.c_cc + tp->t_canq.c_cc <= tp->t_ilowat)
		ttyunblock(tp);
	splx(s);

	return (error);
}

/*
 * Check the output queue on tp for space for a kernel message (from uprintf
 * or tprintf).  Allow some space over the normal hiwater mark so we don't
 * lose messages due to normal flow control, but don't let the tty run amok.
 * Sleeps here are not interruptible, but we return prematurely if new signals
 * arrive.
 */
int
ttycheckoutq(struct tty *tp, int wait)
{
	int hiwat, s;
	sigset_t oldmask;
	struct thread *td;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	hiwat = tp->t_ohiwat;
	SIGEMPTYSET(oldmask);
	s = spltty();
	if (wait) {
		PROC_LOCK(p);
		oldmask = td->td_siglist;
		PROC_UNLOCK(p);
	}
	if (tp->t_outq.c_cc > hiwat + OBUFSIZ + 100)
		while (tp->t_outq.c_cc > hiwat) {
			ttstart(tp);
			if (tp->t_outq.c_cc <= hiwat)
				break;
			if (!wait) {
				splx(s);
				return (0);
			}
			PROC_LOCK(p);
			if (!SIGSETEQ(td->td_siglist, oldmask)) {
				PROC_UNLOCK(p);
				splx(s);
				return (0);
			}
			PROC_UNLOCK(p);
			SET(tp->t_state, TS_SO_OLOWAT);
			tsleep(TSA_OLOWAT(tp), PZERO - 1, "ttoutq", hz);
		}
	splx(s);
	return (1);
}

/*
 * Process a write call on a tty device.
 */
int
ttwrite(struct tty *tp, struct uio *uio, int flag)
{
	char *cp = NULL;
	int cc, ce;
	struct thread *td;
	struct proc *p;
	int i, hiwat, cnt, error, s;
	char obuf[OBUFSIZ];

	hiwat = tp->t_ohiwat;
	cnt = uio->uio_resid;
	error = 0;
	cc = 0;
	td = curthread;
	p = td->td_proc;
loop:
	s = spltty();
	if (ISSET(tp->t_state, TS_ZOMBIE)) {
		splx(s);
		if (uio->uio_resid == cnt)
			error = EIO;
		goto out;
	}
	if (!ISSET(tp->t_state, TS_CONNECTED)) {
		if (flag & IO_NDELAY) {
			splx(s);
			error = EWOULDBLOCK;
			goto out;
		}
		error = ttysleep(tp, TSA_CARR_ON(tp), TTIPRI | PCATCH,
				 "ttywdcd", 0);
		splx(s);
		if (error)
			goto out;
		goto loop;
	}
	splx(s);
	/*
	 * Hang the process if it's in the background.
	 */
	sx_slock(&proctree_lock);
	PROC_LOCK(p);
	if (isbackground(p, tp) &&
	    ISSET(tp->t_lflag, TOSTOP) && !(p->p_flag & P_PPWAIT) &&
	    !SIGISMEMBER(p->p_sigacts->ps_sigignore, SIGTTOU) &&
	    !SIGISMEMBER(td->td_sigmask, SIGTTOU)) {
		if (p->p_pgrp->pg_jobc == 0) {
			PROC_UNLOCK(p);
			sx_sunlock(&proctree_lock);
			error = EIO;
			goto out;
		}
		PROC_UNLOCK(p);
		PGRP_LOCK(p->p_pgrp);
		sx_sunlock(&proctree_lock);
		pgsignal(p->p_pgrp, SIGTTOU, 1);
		PGRP_UNLOCK(p->p_pgrp);
		error = ttysleep(tp, &lbolt, TTIPRI | PCATCH, "ttybg4", 0);
		if (error)
			goto out;
		goto loop;
	} else {
		PROC_UNLOCK(p);
		sx_sunlock(&proctree_lock);
	}
	/*
	 * Process the user's data in at most OBUFSIZ chunks.  Perform any
	 * output translation.  Keep track of high water mark, sleep on
	 * overflow awaiting device aid in acquiring new space.
	 */
	while (uio->uio_resid > 0 || cc > 0) {
		if (ISSET(tp->t_lflag, FLUSHO)) {
			uio->uio_resid = 0;
			return (0);
		}
		if (tp->t_outq.c_cc > hiwat)
			goto ovhiwat;
		/*
		 * Grab a hunk of data from the user, unless we have some
		 * leftover from last time.
		 */
		if (cc == 0) {
			cc = imin(uio->uio_resid, OBUFSIZ);
			cp = obuf;
			error = uiomove(cp, cc, uio);
			if (error) {
				cc = 0;
				break;
			}
		}
		/*
		 * If nothing fancy need be done, grab those characters we
		 * can handle without any of ttyoutput's processing and
		 * just transfer them to the output q.  For those chars
		 * which require special processing (as indicated by the
		 * bits in char_type), call ttyoutput.  After processing
		 * a hunk of data, look for FLUSHO so ^O's will take effect
		 * immediately.
		 */
		while (cc > 0) {
			if (!ISSET(tp->t_oflag, OPOST))
				ce = cc;
			else {
				ce = cc - scanc((u_int)cc, (u_char *)cp,
						char_type, CCLASSMASK);
				/*
				 * If ce is zero, then we're processing
				 * a special character through ttyoutput.
				 */
				if (ce == 0) {
					tp->t_rocount = 0;
					if (ttyoutput(*cp, tp) >= 0) {
						/* No Clists, wait a bit. */
						ttstart(tp);
						if (flag & IO_NDELAY) {
							error = EWOULDBLOCK;
							goto out;
						}
						error = ttysleep(tp, &lbolt,
								 TTOPRI|PCATCH,
								 "ttybf1", 0);
						if (error)
							goto out;
						goto loop;
					}
					cp++;
					cc--;
					if (ISSET(tp->t_lflag, FLUSHO) ||
					    tp->t_outq.c_cc > hiwat)
						goto ovhiwat;
					continue;
				}
			}
			/*
			 * A bunch of normal characters have been found.
			 * Transfer them en masse to the output queue and
			 * continue processing at the top of the loop.
			 * If there are any further characters in this
			 * <= OBUFSIZ chunk, the first should be a character
			 * requiring special handling by ttyoutput.
			 */
			tp->t_rocount = 0;
			i = b_to_q(cp, ce, &tp->t_outq);
			ce -= i;
			tp->t_column += ce;
			cp += ce, cc -= ce, tk_nout += ce;
			tp->t_outcc += ce;
			if (i > 0) {
				/* No Clists, wait a bit. */
				ttstart(tp);
				if (flag & IO_NDELAY) {
					error = EWOULDBLOCK;
					goto out;
				}
				error = ttysleep(tp, &lbolt, TTOPRI | PCATCH,
						 "ttybf2", 0);
				if (error)
					goto out;
				goto loop;
			}
			if (ISSET(tp->t_lflag, FLUSHO) ||
			    tp->t_outq.c_cc > hiwat)
				break;
		}
		ttstart(tp);
	}
out:
	/*
	 * If cc is nonzero, we leave the uio structure inconsistent, as the
	 * offset and iov pointers have moved forward, but it doesn't matter
	 * (the call will either return short or restart with a new uio).
	 */
	uio->uio_resid += cc;
	return (error);

ovhiwat:
	ttstart(tp);
	s = spltty();
	/*
	 * This can only occur if FLUSHO is set in t_lflag,
	 * or if ttstart/oproc is synchronous (or very fast).
	 */
	if (tp->t_outq.c_cc <= hiwat) {
		splx(s);
		goto loop;
	}
	if (flag & IO_NDELAY) {
		splx(s);
		uio->uio_resid += cc;
		return (uio->uio_resid == cnt ? EWOULDBLOCK : 0);
	}
	SET(tp->t_state, TS_SO_OLOWAT);
	error = ttysleep(tp, TSA_OLOWAT(tp), TTOPRI | PCATCH, "ttywri",
			 tp->t_timeout);
	splx(s);
	if (error == EWOULDBLOCK)
		error = EIO;
	if (error)
		goto out;
	goto loop;
}

/*
 * Rubout one character from the rawq of tp
 * as cleanly as possible.
 */
static void
ttyrub(int c, struct tty *tp)
{
	char *cp;
	int savecol;
	int tabc, s;

	if (!ISSET(tp->t_lflag, ECHO) || ISSET(tp->t_lflag, EXTPROC))
		return;
	CLR(tp->t_lflag, FLUSHO);
	if (ISSET(tp->t_lflag, ECHOE)) {
		if (tp->t_rocount == 0) {
			/*
			 * Screwed by ttwrite; retype
			 */
			ttyretype(tp);
			return;
		}
		if (c == ('\t' | TTY_QUOTE) || c == ('\n' | TTY_QUOTE))
			ttyrubo(tp, 2);
		else {
			CLR(c, ~TTY_CHARMASK);
			switch (CCLASS(c)) {
			case ORDINARY:
				ttyrubo(tp, 1);
				break;
			case BACKSPACE:
			case CONTROL:
			case NEWLINE:
			case RETURN:
			case VTAB:
				if (ISSET(tp->t_lflag, ECHOCTL))
					ttyrubo(tp, 2);
				break;
			case TAB:
				if (tp->t_rocount < tp->t_rawq.c_cc) {
					ttyretype(tp);
					return;
				}
				s = spltty();
				savecol = tp->t_column;
				SET(tp->t_state, TS_CNTTB);
				SET(tp->t_lflag, FLUSHO);
				tp->t_column = tp->t_rocol;
				cp = tp->t_rawq.c_cf;
				if (cp)
					tabc = *cp;	/* XXX FIX NEXTC */
				for (; cp; cp = nextc(&tp->t_rawq, cp, &tabc))
					ttyecho(tabc, tp);
				CLR(tp->t_lflag, FLUSHO);
				CLR(tp->t_state, TS_CNTTB);
				splx(s);

				/* savecol will now be length of the tab. */
				savecol -= tp->t_column;
				tp->t_column += savecol;
				if (savecol > 8)
					savecol = 8;	/* overflow screw */
				while (--savecol >= 0)
					(void)ttyoutput('\b', tp);
				break;
			default:			/* XXX */
#define	PANICSTR	"ttyrub: would panic c = %d, val = %d\n"
				(void)printf(PANICSTR, c, CCLASS(c));
#ifdef notdef
				panic(PANICSTR, c, CCLASS(c));
#endif
			}
		}
	} else if (ISSET(tp->t_lflag, ECHOPRT)) {
		if (!ISSET(tp->t_state, TS_ERASE)) {
			SET(tp->t_state, TS_ERASE);
			(void)ttyoutput('\\', tp);
		}
		ttyecho(c, tp);
	} else {
		ttyecho(tp->t_cc[VERASE], tp);
		/*
		 * This code may be executed not only when an ERASE key
		 * is pressed, but also when ^U (KILL) or ^W (WERASE) are.
		 * So, I didn't think it was worthwhile to pass the extra
		 * information (which would need an extra parameter,
		 * changing every call) needed to distinguish the ERASE2
		 * case from the ERASE.
		 */
	}
	--tp->t_rocount;
}

/*
 * Back over cnt characters, erasing them.
 */
static void
ttyrubo(struct tty *tp, int cnt)
{

	while (cnt-- > 0) {
		(void)ttyoutput('\b', tp);
		(void)ttyoutput(' ', tp);
		(void)ttyoutput('\b', tp);
	}
}

/*
 * ttyretype --
 *	Reprint the rawq line.  Note, it is assumed that c_cc has already
 *	been checked.
 */
static void
ttyretype(struct tty *tp)
{
	char *cp;
	int s, c;

	/* Echo the reprint character. */
	if (tp->t_cc[VREPRINT] != _POSIX_VDISABLE)
		ttyecho(tp->t_cc[VREPRINT], tp);

	(void)ttyoutput('\n', tp);

	/*
	 * XXX
	 * FIX: NEXTC IS BROKEN - DOESN'T CHECK QUOTE
	 * BIT OF FIRST CHAR.
	 */
	s = spltty();
	for (cp = tp->t_canq.c_cf, c = (cp != NULL ? *cp : 0);
	    cp != NULL; cp = nextc(&tp->t_canq, cp, &c))
		ttyecho(c, tp);
	for (cp = tp->t_rawq.c_cf, c = (cp != NULL ? *cp : 0);
	    cp != NULL; cp = nextc(&tp->t_rawq, cp, &c))
		ttyecho(c, tp);
	CLR(tp->t_state, TS_ERASE);
	splx(s);

	tp->t_rocount = tp->t_rawq.c_cc;
	tp->t_rocol = 0;
}

/*
 * Echo a typed character to the terminal.
 */
static void
ttyecho(int c, struct tty *tp)
{

	if (!ISSET(tp->t_state, TS_CNTTB))
		CLR(tp->t_lflag, FLUSHO);
	if ((!ISSET(tp->t_lflag, ECHO) &&
	     (c != '\n' || !ISSET(tp->t_lflag, ECHONL))) ||
	    ISSET(tp->t_lflag, EXTPROC))
		return;
	if (ISSET(tp->t_lflag, ECHOCTL) &&
	    ((ISSET(c, TTY_CHARMASK) <= 037 && c != '\t' && c != '\n') ||
	    ISSET(c, TTY_CHARMASK) == 0177)) {
		(void)ttyoutput('^', tp);
		CLR(c, ~TTY_CHARMASK);
		if (c == 0177)
			c = '?';
		else
			c += 'A' - 1;
	}
	(void)ttyoutput(c, tp);
}

/*
 * Wake up any readers on a tty.
 */
void
ttwakeup(struct tty *tp)
{

	if (SEL_WAITING(&tp->t_rsel))
		selwakeuppri(&tp->t_rsel, TTIPRI);
	if (ISSET(tp->t_state, TS_ASYNC) && tp->t_sigio != NULL)
		pgsigio(&tp->t_sigio, SIGIO, (tp->t_session != NULL));
	wakeup(TSA_HUP_OR_INPUT(tp));
	KNOTE_UNLOCKED(&tp->t_rsel.si_note, 0);
}

/*
 * Wake up any writers on a tty.
 */
void
ttwwakeup(struct tty *tp)
{

	if (SEL_WAITING(&tp->t_wsel) && tp->t_outq.c_cc <= tp->t_olowat)
		selwakeuppri(&tp->t_wsel, TTOPRI);
	if (ISSET(tp->t_state, TS_ASYNC) && tp->t_sigio != NULL)
		pgsigio(&tp->t_sigio, SIGIO, (tp->t_session != NULL));
	if (ISSET(tp->t_state, TS_BUSY | TS_SO_OCOMPLETE) ==
	    TS_SO_OCOMPLETE && tp->t_outq.c_cc == 0) {
		CLR(tp->t_state, TS_SO_OCOMPLETE);
		wakeup(TSA_OCOMPLETE(tp));
	}
	if (ISSET(tp->t_state, TS_SO_OLOWAT) &&
	    tp->t_outq.c_cc <= tp->t_olowat) {
		CLR(tp->t_state, TS_SO_OLOWAT);
		wakeup(TSA_OLOWAT(tp));
	}
	KNOTE_UNLOCKED(&tp->t_wsel.si_note, 0);
}

/*
 * Look up a code for a specified speed in a conversion table;
 * used by drivers to map software speed values to hardware parameters.
 */
int
ttspeedtab(int speed, struct speedtab *table)
{

	for ( ; table->sp_speed != -1; table++)
		if (table->sp_speed == speed)
			return (table->sp_code);
	return (-1);
}

/*
 * Set input and output watermarks and buffer sizes.  For input, the
 * high watermark is about one second's worth of input above empty, the
 * low watermark is slightly below high water, and the buffer size is a
 * driver-dependent amount above high water.  For output, the watermarks
 * are near the ends of the buffer, with about 1 second's worth of input
 * between them.  All this only applies to the standard line discipline.
 */
void
ttsetwater(struct tty *tp)
{
	int cps, ttmaxhiwat, x;

	/* Input. */
	clist_alloc_cblocks(&tp->t_canq, TTYHOG, 512);
	switch (tp->t_ispeedwat) {
	case (speed_t)-1:
		cps = tp->t_ispeed / 10;
		break;
	case 0:
		/*
		 * This case is for old drivers that don't know about
		 * t_ispeedwat.  Arrange for them to get the old buffer
		 * sizes and watermarks.
		 */
		cps = TTYHOG - 2 * 256;
		tp->t_ififosize = 2 * 256;
		break;
	default:
		cps = tp->t_ispeedwat / 10;
		break;
	}
	tp->t_ihiwat = cps;
	tp->t_ilowat = 7 * cps / 8;
	x = cps + tp->t_ififosize;
	clist_alloc_cblocks(&tp->t_rawq, x, x);

	/* Output. */
	switch (tp->t_ospeedwat) {
	case (speed_t)-1:
		cps = tp->t_ospeed / 10;
		ttmaxhiwat = 2 * TTMAXHIWAT;
		break;
	case 0:
		cps = tp->t_ospeed / 10;
		ttmaxhiwat = TTMAXHIWAT;
		break;
	default:
		cps = tp->t_ospeedwat / 10;
		ttmaxhiwat = 8 * TTMAXHIWAT;
		break;
	}
#define CLAMP(x, h, l)	((x) > h ? h : ((x) < l) ? l : (x))
	tp->t_olowat = x = CLAMP(cps / 2, TTMAXLOWAT, TTMINLOWAT);
	x += cps;
	x = CLAMP(x, ttmaxhiwat, TTMINHIWAT);	/* XXX clamps are too magic */
	tp->t_ohiwat = roundup(x, CBSIZE);	/* XXX for compat */
	x = imax(tp->t_ohiwat, TTMAXHIWAT);	/* XXX for compat/safety */
	x += OBUFSIZ + 100;
	clist_alloc_cblocks(&tp->t_outq, x, x);
#undef	CLAMP
}

/*
 * Report on state of foreground process group.
 */
void
ttyinfo(struct tty *tp)
{
	struct timeval utime, stime;
	struct proc *p, *pick;
	struct thread *td, *picktd;
	const char *stateprefix, *state;
	long rss;
	int load, pctcpu;
	pid_t pid;
	char comm[MAXCOMLEN + 1];

	if (ttycheckoutq(tp,0) == 0)
		return;

	/* Print load average. */
	load = (averunnable.ldavg[0] * 100 + FSCALE / 2) >> FSHIFT;
	ttyprintf(tp, "load: %d.%02d ", load / 100, load % 100);

	/*
	 * On return following a ttyprintf(), we set tp->t_rocount to 0 so
	 * that pending input will be retyped on BS.
	 */
	if (tp->t_session == NULL) {
		ttyprintf(tp, "not a controlling terminal\n");
		tp->t_rocount = 0;
		return;
	}
	if (tp->t_pgrp == NULL) {
		ttyprintf(tp, "no foreground process group\n");
		tp->t_rocount = 0;
		return;
	}
	PGRP_LOCK(tp->t_pgrp);
	if (LIST_EMPTY(&tp->t_pgrp->pg_members)) {
		PGRP_UNLOCK(tp->t_pgrp);
		ttyprintf(tp, "empty foreground process group\n");
		tp->t_rocount = 0;
		return;
	}

	/*
	 * Pick the most interesting process and copy some of its
	 * state for printing later.  This operation could rely on stale
	 * data as we can't hold the proc slock or thread locks over the
	 * whole list. However, we're guaranteed not to reference an exited
	 * thread or proc since we hold the tty locked.
	 */
	pick = NULL;
	LIST_FOREACH(p, &tp->t_pgrp->pg_members, p_pglist)
		if (proc_compare(pick, p))
			pick = p;

	PROC_SLOCK(pick);
	picktd = NULL;
	td = FIRST_THREAD_IN_PROC(pick);
	FOREACH_THREAD_IN_PROC(pick, td)
		if (thread_compare(picktd, td))
			picktd = td;
	td = picktd;
	stateprefix = "";
	thread_lock(td);
	if (TD_IS_RUNNING(td))
		state = "running";
	else if (TD_ON_RUNQ(td) || TD_CAN_RUN(td))
		state = "runnable";
	else if (TD_IS_SLEEPING(td)) {
		/* XXX: If we're sleeping, are we ever not in a queue? */
		if (TD_ON_SLEEPQ(td))
			state = td->td_wmesg;
		else
			state = "sleeping without queue";
	} else if (TD_ON_LOCK(td)) {
		state = td->td_lockname;
		stateprefix = "*";
	} else if (TD_IS_SUSPENDED(td))
		state = "suspended";
	else if (TD_AWAITING_INTR(td))
		state = "intrwait";
	else
		state = "unknown";
	pctcpu = (sched_pctcpu(td) * 10000 + FSCALE / 2) >> FSHIFT;
	thread_unlock(td);
	if (pick->p_state == PRS_NEW || pick->p_state == PRS_ZOMBIE)
		rss = 0;
	else
		rss = pgtok(vmspace_resident_count(pick->p_vmspace));
	PROC_SUNLOCK(pick);
	PROC_LOCK(pick);
	PGRP_UNLOCK(tp->t_pgrp);
	PROC_SLOCK(pick);
	calcru(pick, &utime, &stime);
	PROC_SUNLOCK(pick);
	pid = pick->p_pid;
	bcopy(pick->p_comm, comm, sizeof(comm));
	PROC_UNLOCK(pick);

	/* Print command, pid, state, utime, stime, %cpu, and rss. */
	ttyprintf(tp,
	    " cmd: %s %d [%s%s] %ld.%02ldu %ld.%02lds %d%% %ldk\n",
	    comm, pid, stateprefix, state,
	    (long)utime.tv_sec, utime.tv_usec / 10000,
	    (long)stime.tv_sec, stime.tv_usec / 10000,
	    pctcpu / 100, rss);
	tp->t_rocount = 0;
}

/*
 * Returns 1 if p2 is "better" than p1
 *
 * The algorithm for picking the "interesting" process is thus:
 *
 *	1) Only foreground processes are eligible - implied.
 *	2) Runnable processes are favored over anything else.  The runner
 *	   with the highest cpu utilization is picked (p_estcpu).  Ties are
 *	   broken by picking the highest pid.
 *	3) The sleeper with the shortest sleep time is next.  With ties,
 *	   we pick out just "short-term" sleepers (P_SINTR == 0).
 *	4) Further ties are broken by picking the highest pid.
 */

#define TESTAB(a, b)    ((a)<<1 | (b))
#define ONLYA   2
#define ONLYB   1
#define BOTH    3

static int
proc_sum(struct proc *p, int *estcpup)
{
	struct thread *td;
	int estcpu;
	int val;

	val = 0;
	estcpu = 0;
	FOREACH_THREAD_IN_PROC(p, td) {
		thread_lock(td);
		if (TD_ON_RUNQ(td) ||
		    TD_IS_RUNNING(td))
			val = 1;
		estcpu += sched_pctcpu(td);
		thread_unlock(td);
	}
	*estcpup = estcpu;

	return (val);
}

static int
thread_compare(struct thread *td, struct thread *td2)
{
	int runa, runb;
	int slpa, slpb;
	fixpt_t esta, estb;

	if (td == NULL)
		return (1);

	/*
	 * Fetch running stats, pctcpu usage, and interruptable flag.
 	 */
	thread_lock(td);
	runa = TD_IS_RUNNING(td) | TD_ON_RUNQ(td);
	slpa = td->td_flags & TDF_SINTR;
	esta = sched_pctcpu(td);
	thread_unlock(td);
	thread_lock(td2);
	runb = TD_IS_RUNNING(td2) | TD_ON_RUNQ(td2);
	estb = sched_pctcpu(td2);
	slpb = td2->td_flags & TDF_SINTR;
	thread_unlock(td2);
	/*
	 * see if at least one of them is runnable
	 */
	switch (TESTAB(runa, runb)) {
	case ONLYA:
		return (0);
	case ONLYB:
		return (1);
	case BOTH:
		break;
	}
	/*
	 *  favor one with highest recent cpu utilization
	 */
	if (estb > esta)
		return (1);
	if (esta > estb)
		return (0);
	/*
	 * favor one sleeping in a non-interruptible sleep
	 */
	switch (TESTAB(slpa, slpb)) {
	case ONLYA:
		return (0);
	case ONLYB:
		return (1);
	case BOTH:
		break;
	}

	return (td < td2);
}

static int
proc_compare(struct proc *p1, struct proc *p2)
{

	int runa, runb;
	fixpt_t esta, estb;

	if (p1 == NULL)
		return (1);

	/*
	 * Fetch various stats about these processes.  After we drop the
	 * lock the information could be stale but the race is unimportant.
	 */
	PROC_SLOCK(p1);
	runa = proc_sum(p1, &esta);
	PROC_SUNLOCK(p1);
	PROC_SLOCK(p2);
	runb = proc_sum(p2, &estb);
	PROC_SUNLOCK(p2);
	
	/*
	 * see if at least one of them is runnable
	 */
	switch (TESTAB(runa, runb)) {
	case ONLYA:
		return (0);
	case ONLYB:
		return (1);
	case BOTH:
		break;
	}
	/*
	 *  favor one with highest recent cpu utilization
	 */
	if (estb > esta)
		return (1);
	if (esta > estb)
		return (0);
	/*
	 * weed out zombies
	 */
	switch (TESTAB(p1->p_state == PRS_ZOMBIE, p2->p_state == PRS_ZOMBIE)) {
	case ONLYA:
		return (1);
	case ONLYB:
		return (0);
	case BOTH:
		break;
	}

	return (p2->p_pid > p1->p_pid);		/* tie - return highest pid */
}

/*
 * Output char to tty; console putchar style.
 */
int
tputchar(int c, struct tty *tp)
{
	int s;

	s = spltty();
	if (!ISSET(tp->t_state, TS_CONNECTED)) {
		splx(s);
		return (-1);
	}
	if (c == '\n')
		(void)ttyoutput('\r', tp);
	(void)ttyoutput(c, tp);
	ttstart(tp);
	splx(s);
	return (0);
}

/*
 * Sleep on chan, returning ERESTART if tty changed while we napped and
 * returning any errors (e.g. EINTR/EWOULDBLOCK) reported by tsleep.  If
 * the tty is revoked, restarting a pending call will redo validation done
 * at the start of the call.
 */
int
ttysleep(struct tty *tp, void *chan, int pri, char *wmesg, int timo)
{
	int error;
	int gen;

	gen = tp->t_gen;
	error = tsleep(chan, pri, wmesg, timo);
	if (tp->t_state & TS_GONE)
		return (ENXIO);
	if (error)
		return (error);
	return (tp->t_gen == gen ? 0 : ERESTART);
}

/*
 * Gain a reference to a TTY
 */
int
ttyref(struct tty *tp)
{
	int i;
	
	mtx_lock(&tp->t_mtx);
	KASSERT(tp->t_refcnt > 0,
	    ("ttyref(): tty refcnt is %d (%s)",
	    tp->t_refcnt, tp->t_dev != NULL ? devtoname(tp->t_dev) : "??"));
	i = ++tp->t_refcnt;
	mtx_unlock(&tp->t_mtx);
	return (i);
}

/*
 * Drop a reference to a TTY.
 * When reference count drops to zero, we free it.
 */
int
ttyrel(struct tty *tp)
{
	int i;
	
	mtx_lock(&tty_list_mutex);
	mtx_lock(&tp->t_mtx);
	KASSERT(tp->t_refcnt > 0,
	    ("ttyrel(): tty refcnt is %d (%s)",
	    tp->t_refcnt, tp->t_dev != NULL ? devtoname(tp->t_dev) : "??"));
	i = --tp->t_refcnt;
	if (i != 0) {
		mtx_unlock(&tp->t_mtx);
		mtx_unlock(&tty_list_mutex);
		return (i);
	}
	TAILQ_REMOVE(&tty_list, tp, t_list);
	mtx_unlock(&tp->t_mtx);
	mtx_unlock(&tty_list_mutex);
	knlist_destroy(&tp->t_rsel.si_note);
	knlist_destroy(&tp->t_wsel.si_note);
	mtx_destroy(&tp->t_mtx);
	free(tp, M_TTYS);
	return (i);
}

/*
 * Allocate a tty struct.  Clists in the struct will be allocated by
 * tty_open().
 */
struct tty *
ttyalloc()
{
	struct tty *tp;

	tp = malloc(sizeof *tp, M_TTYS, M_WAITOK | M_ZERO);
	mtx_init(&tp->t_mtx, "tty", NULL, MTX_DEF);

	/*
	 * Set up the initial state
	 */
	tp->t_refcnt = 1;
	tp->t_timeout = -1;
	tp->t_dtr_wait = 3 * hz;

	ttyinitmode(tp, 0, 0);
	bcopy(ttydefchars, tp->t_init_in.c_cc, sizeof tp->t_init_in.c_cc);

	/* Make callout the same as callin */
	tp->t_init_out = tp->t_init_in;

	mtx_lock(&tty_list_mutex);
	TAILQ_INSERT_TAIL(&tty_list, tp, t_list);
	mtx_unlock(&tty_list_mutex);
	knlist_init(&tp->t_rsel.si_note, &tp->t_mtx, NULL, NULL, NULL);
	knlist_init(&tp->t_wsel.si_note, &tp->t_mtx, NULL, NULL, NULL);
	return (tp);
}

static void
ttypurge(struct cdev *dev)
{

	if (dev->si_tty == NULL)
		return;
	ttygone(dev->si_tty);
}

/*
 * ttycreate()
 *
 * Create the device entries for this tty thereby opening it for business.
 *
 * The flags argument controls if "cua" units are created.
 *
 * The t_sc filed is copied to si_drv1 in the created cdevs.  This 
 * is particularly important for ->t_cioctl() users.
 *
 * XXX: implement the init and lock devices by cloning.
 */

int 
ttycreate(struct tty *tp, int flags, const char *fmt, ...)
{
	char namebuf[SPECNAMELEN - 3];		/* XXX space for "tty" */
	struct cdevsw *csw = NULL;
	int unit = 0;
	va_list ap;
	struct cdev *cp;
	int i, minor, sminor, sunit;

	mtx_assert(&Giant, MA_OWNED);

	if (tty_unit == NULL)
		tty_unit = new_unrhdr(0, 0xffff, NULL);

	sunit = alloc_unr(tty_unit);
	tp->t_devunit = sunit;

	if (csw == NULL) {
		csw = &tty_cdevsw;
		unit = sunit;
	}
	KASSERT(csw->d_purge == NULL || csw->d_purge == ttypurge,
	    ("tty should not have d_purge"));

	csw->d_purge = ttypurge;

	minor = unit2minor(unit);
	sminor = unit2minor(sunit);
	va_start(ap, fmt);
	i = vsnrprintf(namebuf, sizeof namebuf, 32, fmt, ap);
	va_end(ap);
	KASSERT(i < sizeof namebuf, ("Too long tty name (%s)", namebuf));

	cp = make_dev(csw, minor,
	    UID_ROOT, GID_WHEEL, 0600, "tty%s", namebuf);
	tp->t_dev = cp;
	tp->t_mdev = cp;
	cp->si_tty = tp;
	cp->si_drv1 = tp->t_sc;

	cp = make_dev(&ttys_cdevsw, sminor | MINOR_INIT,
	    UID_ROOT, GID_WHEEL, 0600, "tty%s.init", namebuf);
	dev_depends(tp->t_dev, cp);
	cp->si_drv1 = tp->t_sc;
	cp->si_drv2 = &tp->t_init_in;
	cp->si_tty = tp;

	cp = make_dev(&ttys_cdevsw, sminor | MINOR_LOCK,
	    UID_ROOT, GID_WHEEL, 0600, "tty%s.lock", namebuf);
	dev_depends(tp->t_dev, cp);
	cp->si_drv1 = tp->t_sc;
	cp->si_drv2 = &tp->t_lock_in;
	cp->si_tty = tp;

	if (flags & TS_CALLOUT) {
		cp = make_dev(csw, minor | MINOR_CALLOUT,
		    UID_UUCP, GID_DIALER, 0660, "cua%s", namebuf);
		dev_depends(tp->t_dev, cp);
		cp->si_drv1 = tp->t_sc;
		cp->si_tty = tp;

		cp = make_dev(&ttys_cdevsw, sminor | MINOR_CALLOUT | MINOR_INIT,
		    UID_UUCP, GID_DIALER, 0660, "cua%s.init", namebuf);
		dev_depends(tp->t_dev, cp);
		cp->si_drv1 = tp->t_sc;
		cp->si_drv2 = &tp->t_init_out;
		cp->si_tty = tp;

		cp = make_dev(&ttys_cdevsw, sminor | MINOR_CALLOUT | MINOR_LOCK,
		    UID_UUCP, GID_DIALER, 0660, "cua%s.lock", namebuf);
		dev_depends(tp->t_dev, cp);
		cp->si_drv1 = tp->t_sc;
		cp->si_drv2 = &tp->t_lock_out;
		cp->si_tty = tp;
	}

	return (0);
}

/*
 * This function is called when the hardware disappears.  We set a flag
 * and wake up stuff so all sleeping threads will notice.
 */
void	
ttygone(struct tty *tp)
{

	tp->t_state |= TS_GONE;
	wakeup(&tp->t_dtr_wait);
	wakeup(TSA_CARR_ON(tp));
	wakeup(TSA_HUP_OR_INPUT(tp));
	wakeup(TSA_OCOMPLETE(tp));
	wakeup(TSA_OLOWAT(tp));
	tt_purge(tp);
}

/*
 * ttyfree()
 *    
 * Called when the driver is ready to free the tty structure.
 *
 * XXX: This shall sleep until all threads have left the driver.
 */
 
void
ttyfree(struct tty *tp)
{
	u_int unit;
 
	mtx_assert(&Giant, MA_OWNED);
	ttygone(tp);
	unit = tp->t_devunit;
	destroy_dev(tp->t_mdev);
	free_unr(tty_unit, unit);
}

static int
sysctl_kern_ttys(SYSCTL_HANDLER_ARGS)
{
	struct tty *tp, *tp2;
	struct xtty xt;
	int error;

	error = 0;
	mtx_lock(&tty_list_mutex);
	tp = TAILQ_FIRST(&tty_list);
	if (tp != NULL)
		ttyref(tp);
	mtx_unlock(&tty_list_mutex);
	while (tp != NULL) {
		bzero(&xt, sizeof xt);
		xt.xt_size = sizeof xt;
#define XT_COPY(field) xt.xt_##field = tp->t_##field
		xt.xt_rawcc = tp->t_rawq.c_cc;
		xt.xt_cancc = tp->t_canq.c_cc;
		xt.xt_outcc = tp->t_outq.c_cc;
		XT_COPY(line);
		if (tp->t_dev != NULL)
			xt.xt_dev = dev2udev(tp->t_dev);
		XT_COPY(state);
		XT_COPY(flags);
		XT_COPY(timeout);
		if (tp->t_pgrp != NULL)
			xt.xt_pgid = tp->t_pgrp->pg_id;
		if (tp->t_session != NULL)
			xt.xt_sid = tp->t_session->s_sid;
		XT_COPY(termios);
		XT_COPY(winsize);
		XT_COPY(column);
		XT_COPY(rocount);
		XT_COPY(rocol);
		XT_COPY(ififosize);
		XT_COPY(ihiwat);
		XT_COPY(ilowat);
		XT_COPY(ispeedwat);
		XT_COPY(ohiwat);
		XT_COPY(olowat);
		XT_COPY(ospeedwat);
#undef XT_COPY
		error = SYSCTL_OUT(req, &xt, sizeof xt);
		if (error != 0) {
			ttyrel(tp);
			return (error);
		}
		mtx_lock(&tty_list_mutex);
		tp2 = TAILQ_NEXT(tp, t_list);
		if (tp2 != NULL)
			ttyref(tp2);
		mtx_unlock(&tty_list_mutex);
		ttyrel(tp);
		tp = tp2;
	}
	return (0);
}

SYSCTL_PROC(_kern, OID_AUTO, ttys, CTLTYPE_OPAQUE|CTLFLAG_RD,
	0, 0, sysctl_kern_ttys, "S,xtty", "All ttys");
SYSCTL_LONG(_kern, OID_AUTO, tty_nin, CTLFLAG_RD,
	&tk_nin, 0, "Total TTY in characters");
SYSCTL_LONG(_kern, OID_AUTO, tty_nout, CTLFLAG_RD,
	&tk_nout, 0, "Total TTY out characters");

void
nottystop(struct tty *tp, int rw)
{

	return;
}

int
ttyopen(struct cdev *dev, int flag, int mode, struct thread *td)
{
	int		error;
	int		s;
	struct tty	*tp;

	tp = dev->si_tty;

	s = spltty();
	/*
	 * We jump to this label after all non-interrupted sleeps to pick
	 * up any changes of the device state.
	 */
open_top:
	if (tp->t_state & TS_GONE)
		return (ENXIO);
	error = ttydtrwaitsleep(tp);
	if (error)
		goto out;
	if (tp->t_state & TS_ISOPEN) {
		/*
		 * The device is open, so everything has been initialized.
		 * Handle conflicts.
		 */
		if (ISCALLOUT(dev) && !tp->t_actout)
			return (EBUSY);
		if (tp->t_actout && !ISCALLOUT(dev)) {
			if (flag & O_NONBLOCK)
				return (EBUSY);
			error =	tsleep(&tp->t_actout,
				       TTIPRI | PCATCH, "ttybi", 0);
			if (error != 0 || (tp->t_flags & TS_GONE))
				goto out;
			goto open_top;
		}
		if (tp->t_state & TS_XCLUDE && priv_check(td,
		    PRIV_TTY_EXCLUSIVE))
			return (EBUSY);
	} else {
		/*
		 * The device isn't open, so there are no conflicts.
		 * Initialize it.  Initialization is done twice in many
		 * cases: to preempt sleeping callin opens if we are
		 * callout, and to complete a callin open after DCD rises.
		 */
		tp->t_termios = ISCALLOUT(dev) ? tp->t_init_out : tp->t_init_in;
		tp->t_cflag = tp->t_termios.c_cflag;
		if (tp->t_modem != NULL)
			tt_modem(tp, SER_DTR | SER_RTS, 0);
		++tp->t_wopeners;
		error = tt_param(tp, &tp->t_termios);
		--tp->t_wopeners;
		if (error == 0)
			error = tt_open(tp, dev);
		if (error != 0)
			goto out;
		if (ISCALLOUT(dev) || (tt_modem(tp, 0, 0) & SER_DCD))
			ttyld_modem(tp, 1);
	}
	/*
	 * Wait for DCD if necessary.
	 */
	if (!(tp->t_state & TS_CARR_ON) && !ISCALLOUT(dev)
	    && !(tp->t_cflag & CLOCAL) && !(flag & O_NONBLOCK)) {
		++tp->t_wopeners;
		error = tsleep(TSA_CARR_ON(tp), TTIPRI | PCATCH, "ttydcd", 0);
		--tp->t_wopeners;
		if (error != 0 || (tp->t_state & TS_GONE))
			goto out;
		goto open_top;
	}
	error =	ttyld_open(tp, dev);
	ttyldoptim(tp);
	if (tp->t_state & TS_ISOPEN && ISCALLOUT(dev))
		tp->t_actout = TRUE;
out:
	splx(s);
	if (!(tp->t_state & TS_ISOPEN) && tp->t_wopeners == 0)
		tt_close(tp);
	return (error);
}

int
ttyclose(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct tty *tp;

	tp = dev->si_tty;
	ttyld_close(tp, flag);
	ttyldoptim(tp);
	tt_close(tp);
	tp->t_do_timestamp = 0;
	if (tp->t_pps != NULL)
		tp->t_pps->ppsparam.mode = 0;
	tty_close(tp);
	return (0);
}

int
ttyread(struct cdev *dev, struct uio *uio, int flag)
{
	struct tty *tp;

	tp = tty_gettp(dev);

	if (tp->t_state & TS_GONE)
		return (ENODEV);
	return (ttyld_read(tp, uio, flag));
}

int
ttywrite(struct cdev *dev, struct uio *uio, int flag)
{
	struct tty *tp;

	tp = tty_gettp(dev);

	if (tp->t_state & TS_GONE)
		return (ENODEV);
	return (ttyld_write(tp, uio, flag));
}

int
ttyioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct	tty *tp;
	int	error;

	tp = dev->si_tty;

	if (cmd == TIOCSETA || cmd == TIOCSETAW || cmd == TIOCSETAF) {
		int cc;
		struct termios *dt = (struct termios *)data;
		struct termios *lt =
		    ISCALLOUT(dev) ?  &tp->t_lock_out : &tp->t_lock_in;

		dt->c_iflag = (tp->t_iflag & lt->c_iflag)
		    | (dt->c_iflag & ~lt->c_iflag);
		dt->c_oflag = (tp->t_oflag & lt->c_oflag)
		    | (dt->c_oflag & ~lt->c_oflag);
		dt->c_cflag = (tp->t_cflag & lt->c_cflag)
		    | (dt->c_cflag & ~lt->c_cflag);
		dt->c_lflag = (tp->t_lflag & lt->c_lflag)
		    | (dt->c_lflag & ~lt->c_lflag);
		for (cc = 0; cc < NCCS; ++cc)
		    if (lt->c_cc[cc] != 0)
		        dt->c_cc[cc] = tp->t_cc[cc];
		if (lt->c_ispeed != 0)
		    dt->c_ispeed = tp->t_ispeed;
		if (lt->c_ospeed != 0)
		    dt->c_ospeed = tp->t_ospeed;
	}

	error = ttyld_ioctl(tp, cmd, data, flag, td);
	if (error == ENOIOCTL)
		error = ttioctl(tp, cmd, data, flag);
	ttyldoptim(tp);
	if (error != ENOIOCTL)
		return (error);
	return (ENOTTY);
}

void
ttyldoptim(struct tty *tp)
{
	struct termios	*t;

	t = &tp->t_termios;
	if (!(t->c_iflag & (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP | IXON))
	    && (!(t->c_iflag & BRKINT) || (t->c_iflag & IGNBRK))
	    && (!(t->c_iflag & PARMRK)
		|| (t->c_iflag & (IGNPAR | IGNBRK)) == (IGNPAR | IGNBRK))
	    && !(t->c_lflag & (ECHO | ICANON | IEXTEN | ISIG | PENDIN))
	    && linesw[tp->t_line]->l_rint == ttyinput)
		tp->t_state |= TS_CAN_BYPASS_L_RINT;
	else
		tp->t_state &= ~TS_CAN_BYPASS_L_RINT;
}

static void
ttydtrwaitwakeup(void *arg)
{
	struct tty *tp;

	tp = arg;
	tp->t_state &= ~TS_DTR_WAIT;
	wakeup(&tp->t_dtr_wait);
}


void	
ttydtrwaitstart(struct tty *tp)
{

	if (tp->t_dtr_wait == 0)
		return;
	if (tp->t_state & TS_DTR_WAIT)
		return;
	timeout(ttydtrwaitwakeup, tp, tp->t_dtr_wait);
	tp->t_state |= TS_DTR_WAIT;
}

int
ttydtrwaitsleep(struct tty *tp)
{
	int error;

	error = 0;
	while (error == 0) {
		if (tp->t_state & TS_GONE)
			error = ENXIO;
		else if (!(tp->t_state & TS_DTR_WAIT))
			break;
		else
			error = tsleep(&tp->t_dtr_wait, TTIPRI | PCATCH,
			    "dtrwait", 0);
	}
	return (error);
}

static int
ttysopen(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct tty *tp;

	tp = dev->si_tty;
	KASSERT(tp != NULL,
	    ("ttysopen(): no tty pointer on device (%s)", devtoname(dev)));
	if (tp->t_state & TS_GONE)
		return (ENODEV);
	return (0);
}

static int
ttysclose(struct cdev *dev, int flag, int mode, struct thread *td)
{

	return (0);
}

static int
ttysrdwr(struct cdev *dev, struct uio *uio, int flag)
{

	return (ENODEV);
}

static int
ttysioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct tty	*tp;
	int		error;
	struct termios	*ct;

	tp = dev->si_tty;
	KASSERT(tp != NULL,
	    ("ttysopen(): no tty pointer on device (%s)", devtoname(dev)));
	if (tp->t_state & TS_GONE)
		return (ENODEV);
	ct = dev->si_drv2;
	switch (cmd) {
	case TIOCSETA:
		error = priv_check(td, PRIV_TTY_SETA);
		if (error != 0)
			return (error);
		*ct = *(struct termios *)data;
		return (0);
	case TIOCGETA:
		*(struct termios *)data = *ct;
		return (0);
	case TIOCGETD:
		*(int *)data = TTYDISC;
		return (0);
	case TIOCGWINSZ:
		bzero(data, sizeof(struct winsize));
		return (0);
	default:
		if (tp->t_cioctl != NULL)
			return(tp->t_cioctl(dev, cmd, data, flag, td));
		return (ENOTTY);
	}
}

/*
 * Initialize a tty to sane modes.
 */
void
ttyinitmode(struct tty *tp, int echo, int speed)
{

	if (speed == 0)
		speed = TTYDEF_SPEED;
	tp->t_init_in.c_iflag = TTYDEF_IFLAG;
	tp->t_init_in.c_oflag = TTYDEF_OFLAG;
	tp->t_init_in.c_cflag = TTYDEF_CFLAG;
	if (echo)
		tp->t_init_in.c_lflag = TTYDEF_LFLAG_ECHO;
	else
		tp->t_init_in.c_lflag = TTYDEF_LFLAG_NOECHO;

	tp->t_init_in.c_ispeed = tp->t_init_in.c_ospeed = speed;
	termioschars(&tp->t_init_in);
	tp->t_init_out = tp->t_init_in;
	tp->t_termios = tp->t_init_in;
}

/*
 * Use more "normal" termios paramters for consoles.
 */
void
ttyconsolemode(struct tty *tp, int speed)
{

	if (speed == 0)
		speed = TTYDEF_SPEED;
	ttyinitmode(tp, 1, speed);
	tp->t_init_in.c_cflag |= CLOCAL;
	tp->t_lock_out.c_cflag = tp->t_lock_in.c_cflag = CLOCAL;
	tp->t_lock_out.c_ispeed = tp->t_lock_out.c_ospeed =
	tp->t_lock_in.c_ispeed = tp->t_lock_in.c_ospeed = speed;
	tp->t_init_out = tp->t_init_in;
	tp->t_termios = tp->t_init_in;
	ttsetwater(tp);
}

/*
 * Record the relationship between the serial ports notion of modem control
 * signals and the one used in certain ioctls in a way the compiler can enforce
 * XXX: We should define TIOCM_* in terms of SER_ if we can limit the
 * XXX: consequences of the #include work that would take.
 */
CTASSERT(SER_DTR == TIOCM_DTR / 2);
CTASSERT(SER_RTS == TIOCM_RTS / 2);
CTASSERT(SER_STX == TIOCM_ST / 2);
CTASSERT(SER_SRX == TIOCM_SR / 2);
CTASSERT(SER_CTS == TIOCM_CTS / 2);
CTASSERT(SER_DCD == TIOCM_DCD / 2);
CTASSERT(SER_RI == TIOCM_RI / 2);
CTASSERT(SER_DSR == TIOCM_DSR / 2);

