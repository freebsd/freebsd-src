/*-
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)tty.c	7.44 (Berkeley) 5/28/91
 *	$Id: tty.c,v 1.5 1993/10/16 15:24:54 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "ioctl.h"
#define TTYDEFCHARS
#include "tty.h"
#undef TTYDEFCHARS
#include "proc.h"
#include "file.h"
#include "conf.h"
#include "dkstat.h"
#include "uio.h"
#include "kernel.h"
#include "vnode.h"
#include "syslog.h"

#include "vm/vm.h"

static int proc_compare __P((struct proc *p1, struct proc *p2));

/* symbolic sleep message strings */
char ttyin[] = "ttyin";
char ttyout[] = "ttyout";
char ttopen[] = "ttyopn";
char ttclos[] = "ttycls";
char ttybg[] = "ttybg";
char ttybuf[] = "ttybuf";

/*
 * Table giving parity for characters and indicating
 * character classes to tty driver. The 8th bit
 * indicates parity, the 7th bit indicates the character
 * is an alphameric or underscore (for ALTWERASE), and the 
 * low 6 bits indicate delay type.  If the low 6 bits are 0
 * then the character needs no special processing on output;
 * classes other than 0 might be translated or (not currently)
 * require delays.
 */
#define	PARITY(c)	(partab[c] & 0x80)
#define	ISALPHA(c)	(partab[(c)&TTY_CHARMASK] & 0x40)
#define	CCLASSMASK	0x3f
#define	CCLASS(c)	(partab[c] & CCLASSMASK)

#define	E	0x00	/* even parity */
#define	O	0x80	/* odd parity */
#define	ALPHA	0x40	/* alpha or underscore */

#define	NO	ORDINARY
#define	NA	ORDINARY|ALPHA
#define	CC	CONTROL
#define	BS	BACKSPACE
#define	NL	NEWLINE
#define	TB	TAB
#define	VT	VTAB
#define	CR	RETURN

char partab[] = {
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
	 * "meta" chars; should be settable per charset.
	 * For now, treat all as normal characters.
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
#undef	NO
#undef	NA
#undef	CC
#undef	BS
#undef	NL
#undef	TB
#undef	VT
#undef	CR

extern struct tty *constty;		/* temporary virtual console */

/*
 * Is 'c' a line delimiter ("break" character)?
 */
#define ttbreakc(c) ((c) == '\n' || ((c) == cc[VEOF] || \
	(c) == cc[VEOL] || (c) == cc[VEOL2]) && (c) != _POSIX_VDISABLE)

ttychars(tp)
	struct tty *tp;
{

	bcopy(ttydefchars, tp->t_cc, sizeof(ttydefchars));
}

/*
 * Flush tty after output has drained.
 */
ttywflush(tp)
	struct tty *tp;
{
	int error;

	if ((error = ttywait(tp)) == 0)
		ttyflush(tp, FREAD);
	return (error);
}

/*
 * Wait for output to drain.
 */
ttywait(tp)
	register struct tty *tp;
{
	int error = 0, s = spltty();

	while ((RB_LEN(&tp->t_out) || tp->t_state&TS_BUSY) &&
	    (tp->t_state&TS_CARR_ON || tp->t_cflag&CLOCAL) && 
	    tp->t_oproc) {
		(*tp->t_oproc)(tp);
		tp->t_state |= TS_ASLEEP;
		if (error = ttysleep(tp, (caddr_t)&tp->t_out, 
		    TTOPRI | PCATCH, ttyout, 0))
			break;
	}
	splx(s);
	return (error);
}

#define	flushq(qq) { \
	register struct ringb *r = qq; \
	r->rb_hd = r->rb_tl; \
}

/*
 * Flush TTY read and/or write queues,
 * notifying anyone waiting.
 */
ttyflush(tp, rw)
	register struct tty *tp;
{
	register s;

	s = spltty();
	if (rw & FREAD) {
		flushq(&tp->t_can);
		flushq(&tp->t_raw);
		tp->t_rocount = 0;
		tp->t_rocol = 0;
		tp->t_state &= ~(TS_LOCAL|TS_TBLOCK);	/* XXX - should be TS_RTSBLOCK */
		ttwakeup(tp);
	}
	if (rw & FWRITE) {
		tp->t_state &= ~TS_TTSTOP;
		(*cdevsw[major(tp->t_dev)].d_stop)(tp, rw);
		flushq(&tp->t_out);
		wakeup((caddr_t)&tp->t_out);
		if (tp->t_wsel) {
			selwakeup(tp->t_wsel, tp->t_state & TS_WCOLL);
			tp->t_wsel = 0;
			tp->t_state &= ~TS_WCOLL;
		}
	}
	splx(s);
}

/*
 * Send stop character on input overflow.
 */
ttyblock(tp)
	register struct tty *tp;
{
	register x;
	int rawcc, cancc;

	rawcc = RB_LEN(&tp->t_raw);
	cancc = RB_LEN(&tp->t_can);
	x = rawcc + cancc;
	if (rawcc > TTYHOG) {
		ttyflush(tp, FREAD|FWRITE);
	}
	/*
	 * Block further input iff:
	 * Current input > threshold AND input is available to user program
	 */
	if (x >= TTYHOG/2 && (tp->t_state & TS_TBLOCK) == 0 &&
	    ((tp->t_lflag&ICANON) == 0) || (cancc > 0)) {
		if (tp->t_cc[VSTOP] != _POSIX_VDISABLE) {
		    putc(tp->t_cc[VSTOP], &tp->t_out);
		}
		tp->t_state |= TS_TBLOCK;	/* XXX - should be TS_RTSBLOCK? */
		ttstart(tp);
	}
}

ttstart(tp)
	struct tty *tp;
{

	if (tp->t_oproc)		/* kludge for pty */
		(*tp->t_oproc)(tp);
}

ttrstrt(tp)				/* XXX */
	struct tty *tp;
{

#ifdef DIAGNOSTIC
	if (tp == 0)
		panic("ttrstrt");
#endif
	tp->t_state &= ~TS_TIMEOUT;
	ttstart(tp);
}


/*
 * Common code for ioctls on tty devices.
 * Called after line-discipline-specific ioctl
 * has been called to do discipline-specific functions
 * and/or reject any of these ioctl commands.
 */
/*ARGSUSED*/
ttioctl(tp, com, data, flag)
	register struct tty *tp;
	caddr_t data;
{
	register struct proc *p = curproc;		/* XXX */
	extern int nldisp;
	int s, error;

	/*
	 * If the ioctl involves modification,
	 * hang if in the background.
	 */
	switch (com) {

	case TIOCSETD: 
	case TIOCFLUSH:
	/*case TIOCSPGRP:*/
	case TIOCSTI:
	case TIOCSWINSZ:
	case TIOCSETA:
	case TIOCSETAW:
	case TIOCSETAF:
	case TIOCSTAT:
#ifdef COMPAT_43
	case TIOCSETP:
	case TIOCSETN:
	case TIOCSETC:
	case TIOCSLTC:
	case TIOCLBIS:
	case TIOCLBIC:
	case TIOCLSET:
	case OTIOCSETD:
#endif
		while (isbackground(curproc, tp) && 
		   p->p_pgrp->pg_jobc && (p->p_flag&SPPWAIT) == 0 &&
		   (p->p_sigignore & sigmask(SIGTTOU)) == 0 &&
		   (p->p_sigmask & sigmask(SIGTTOU)) == 0) {
			pgsignal(p->p_pgrp, SIGTTOU, 1);
			if (error = ttysleep(tp, (caddr_t)&lbolt, 
			    TTOPRI | PCATCH, ttybg, 0)) 
				return (error);
		}
		break;
	}

	/*
	 * Process the ioctl.
	 */
	switch (com) {

	/* get discipline number */
	case TIOCGETD:
		*(int *)data = tp->t_line;
		break;

	/* set line discipline */
	case TIOCSETD: {
		register int t = *(int *)data;
		dev_t dev = tp->t_dev;

		if ((unsigned)t >= nldisp)
			return (ENXIO);
		if (t != tp->t_line) {
			s = spltty();
			(*linesw[tp->t_line].l_close)(tp, flag);
			error = (*linesw[t].l_open)(dev, tp);
			if (error) {
				(void)(*linesw[tp->t_line].l_open)(dev, tp);
				splx(s);
				return (error);
			}
			tp->t_line = t;
			splx(s);
		}
		break;
	}

	/* prevent more opens on channel */
	case TIOCEXCL:
		tp->t_state |= TS_XCLUDE;
		break;

	case TIOCNXCL:
		tp->t_state &= ~TS_XCLUDE;
		break;

#ifdef COMPAT_43
	/* wkt */
	case TIOCHPCL:
		tp->t_cflag |= HUPCL;
		break;
#endif

	case TIOCFLUSH: {
		register int flags = *(int *)data;

		if (flags == 0)
			flags = FREAD|FWRITE;
		else
			flags &= FREAD|FWRITE;
		ttyflush(tp, flags);
		break;
	}

	case FIOASYNC:
		if (*(int *)data)
			tp->t_state |= TS_ASYNC;
		else
			tp->t_state &= ~TS_ASYNC;
		break;

	case FIONBIO:
		break;	/* XXX remove */

	/* return number of characters immediately available */
	case FIONREAD:
		*(off_t *)data = ttnread(tp);
		break;

	case TIOCOUTQ:
		*(int *)data = RB_LEN(&tp->t_out);
		break;

	case TIOCSTOP:
		s = spltty();
		if ((tp->t_state&TS_TTSTOP) == 0) {
			tp->t_state |= TS_TTSTOP;
			(*cdevsw[major(tp->t_dev)].d_stop)(tp, 0);
		}
		splx(s);
		break;

	case TIOCSTART:
		s = spltty();
		if ((tp->t_state&TS_TTSTOP) || (tp->t_lflag&FLUSHO)) {
			tp->t_state &= ~TS_TTSTOP;
			tp->t_lflag &= ~FLUSHO;
			ttstart(tp);
		}
		splx(s);
		break;

	/*
	 * Simulate typing of a character at the terminal.
	 */
	case TIOCSTI:
		if (p->p_ucred->cr_uid && (flag & FREAD) == 0)
			return (EPERM);
		if (p->p_ucred->cr_uid && !isctty(p, tp))
			return (EACCES);
		(*linesw[tp->t_line].l_rint)(*(u_char *)data, tp);
		break;

	case TIOCGETA: {
		struct termios *t = (struct termios *)data;

		bcopy(&tp->t_termios, t, sizeof(struct termios));
		break;
	}

	case TIOCSETA:
	case TIOCSETAW:
	case TIOCSETAF: {
		register struct termios *t = (struct termios *)data;

		s = spltty();
		if (com == TIOCSETAW || com == TIOCSETAF) {
			if (error = ttywait(tp)) {
				splx(s);
				return (error);
			}
			if (com == TIOCSETAF)
				ttyflush(tp, FREAD);
		}
		if ((t->c_cflag&CIGNORE) == 0) {
			/*
			 * set device hardware
			 */
			if (tp->t_param && (error = (*tp->t_param)(tp, t))) {
				splx(s);
				return (error);
			} else {
				if ((tp->t_state&TS_CARR_ON) == 0 &&
				    (tp->t_cflag&CLOCAL) &&
				    (t->c_cflag&CLOCAL) == 0) {
					tp->t_state &= ~TS_ISOPEN;
					tp->t_state |= TS_WOPEN;
					ttwakeup(tp);
				}
				tp->t_cflag = t->c_cflag;
				tp->t_ispeed = t->c_ispeed;
				tp->t_ospeed = t->c_ospeed;
			}
			ttsetwater(tp);
		}
		if (com != TIOCSETAF) {
			if ((t->c_lflag&ICANON) != (tp->t_lflag&ICANON))
				if (t->c_lflag&ICANON) {	
					tp->t_lflag |= PENDIN;
					ttwakeup(tp);
				}
				else {
					catb(&tp->t_raw, &tp->t_can);
					catb(&tp->t_can, &tp->t_raw);
				}
		}
		tp->t_iflag = t->c_iflag;
		tp->t_oflag = t->c_oflag;
		/*
		 * Make the EXTPROC bit read only.
		 */
		if (tp->t_lflag&EXTPROC)
			t->c_lflag |= EXTPROC;
		else
			t->c_lflag &= ~EXTPROC;
		tp->t_lflag = t->c_lflag;
		bcopy(t->c_cc, tp->t_cc, sizeof(t->c_cc));
		splx(s);
		break;
	}

	/*
	 * Give load average stats if requested (tcsh uses raw mode
	 * and directly sends the ioctl() to the tty driver)
	 */
	case TIOCSTAT:
		ttyinfo(tp);
		break;

	/*
	 * Set controlling terminal.
	 * Session ctty vnode pointer set in vnode layer.
	 */
	case TIOCSCTTY:
		if (!SESS_LEADER(p) || 
		   (p->p_session->s_ttyvp || tp->t_session) &&
		   (tp->t_session != p->p_session))
			return (EPERM);
		tp->t_session = p->p_session;
		tp->t_pgrp = p->p_pgrp;
		p->p_session->s_ttyp = tp;
		p->p_flag |= SCTTY;
		break;
		
	/*
	 * Set terminal process group.
	 */
	case TIOCSPGRP: {
		register struct pgrp *pgrp = pgfind(*(int *)data);

		if (!isctty(p, tp))
			return (ENOTTY);
		else if (pgrp == NULL || pgrp->pg_session != p->p_session)
			return (EPERM);
		tp->t_pgrp = pgrp;
		break;
	}

	case TIOCGPGRP:
		if (!isctty(p, tp))
			return (ENOTTY);
		*(int *)data = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		break;

	case TIOCSWINSZ:
		if (bcmp((caddr_t)&tp->t_winsize, data,
		    sizeof (struct winsize))) {
			tp->t_winsize = *(struct winsize *)data;
			pgsignal(tp->t_pgrp, SIGWINCH, 1);
		}
		break;

	case TIOCGWINSZ:
		*(struct winsize *)data = tp->t_winsize;
		break;

	case TIOCCONS:
		if (*(int *)data) {
			if (constty && constty != tp &&
			    (constty->t_state & (TS_CARR_ON|TS_ISOPEN)) ==
			    (TS_CARR_ON|TS_ISOPEN))
				return (EBUSY);
#ifndef	UCONSOLE
			if (error = suser(p->p_ucred, &p->p_acflag))
				return (error);
#endif
			constty = tp;
		} else if (tp == constty)
			constty = NULL;
		break;

	case TIOCDRAIN:
		if (error = ttywait(tp))
			return (error);
		break;

	default:
#ifdef COMPAT_43
		return (ttcompat(tp, com, data, flag));
#else
		return (-1);
#endif
	}
	return (0);
}

ttnread(tp)
	struct tty *tp;
{
	int nread = 0;

	if (tp->t_lflag & PENDIN)
		ttypend(tp);
	nread = RB_LEN(&tp->t_can);
	if ((tp->t_lflag & ICANON) == 0)
		nread += RB_LEN(&tp->t_raw);
	return (nread);
}

ttselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	register struct tty *tp = &cdevsw[major(dev)].d_ttys[minor(dev)];
	int nread;
	int s = spltty();
	struct proc *selp;

	switch (rw) {

	case FREAD:
		nread = ttnread(tp);
		if (nread > 0 || 
		   ((tp->t_cflag&CLOCAL) == 0 && (tp->t_state&TS_CARR_ON) == 0))
			goto win;
		if (tp->t_rsel && (selp = pfind(tp->t_rsel)) && selp->p_wchan == (caddr_t)&selwait)
			tp->t_state |= TS_RCOLL;
		else
			tp->t_rsel = p->p_pid;
		break;

	case FWRITE:
		if (RB_LEN(&tp->t_out) <= tp->t_lowat)
			goto win;
		if (tp->t_wsel && (selp = pfind(tp->t_wsel)) && selp->p_wchan == (caddr_t)&selwait)
			tp->t_state |= TS_WCOLL;
		else
			tp->t_wsel = p->p_pid;
		break;
	}
	splx(s);
	return (0);
win:
	splx(s);
	return (1);
}

/*
 * Initial open of tty, or (re)entry to standard tty line discipline.
 */
ttyopen(dev, tp)
	dev_t dev;
	register struct tty *tp;
{

	tp->t_dev = dev;

	tp->t_state &= ~TS_WOPEN;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_ISOPEN;
		initrb(&tp->t_raw);
		initrb(&tp->t_can);
		initrb(&tp->t_out);
		bzero((caddr_t)&tp->t_winsize, sizeof(tp->t_winsize));
	}
	return (0);
}

/*
 * "close" a line discipline
 */
ttylclose(tp, flag)
	struct tty *tp;
	int flag;
{

	if (flag&IO_NDELAY)
		ttyflush(tp, FREAD|FWRITE);
	else
		ttywflush(tp);
}

/*
 * Handle close() on a tty line: flush and set to initial state,
 * bumping generation number so that pending read/write calls
 * can detect recycling of the tty.
 */
ttyclose(tp)
	register struct tty *tp;
{
	if (constty == tp)
		constty = NULL;
	ttyflush(tp, FREAD|FWRITE);
	tp->t_session = NULL;
	tp->t_pgrp = NULL;
/*
 * XXX - do we need to send cc[VSTART] or do a ttstart() here in some cases?
 * (TS_TBLOCK and TS_RTSBLOCK are being cleared.)
 */
	tp->t_state = 0;
	tp->t_gen++;
	return (0);
}

/*
 * Handle modem control transition on a tty.
 * Flag indicates new state of carrier.
 * Returns 0 if the line should be turned off, otherwise 1.
 */
ttymodem(tp, flag)
	register struct tty *tp;
{

	if ((tp->t_state&TS_WOPEN) == 0 && (tp->t_lflag&MDMBUF)) {
		/*
		 * MDMBUF: do flow control according to carrier flag
		 */
		if (flag) {
			tp->t_state &= ~TS_TTSTOP;
			ttstart(tp);
		} else if ((tp->t_state&TS_TTSTOP) == 0) {
			tp->t_state |= TS_TTSTOP;
			(*cdevsw[major(tp->t_dev)].d_stop)(tp, 0);
		}
	} else if (flag == 0) {
		/*
		 * Lost carrier.
		 */
		tp->t_state &= ~TS_CARR_ON;
		if (tp->t_state&TS_ISOPEN && (tp->t_cflag&CLOCAL) == 0) {
			if (tp->t_session && tp->t_session->s_leader)
				psignal(tp->t_session->s_leader, SIGHUP);
			ttyflush(tp, FREAD|FWRITE);
			return (0);
		}
	} else {
		/*
		 * Carrier now on.
		 */
		tp->t_state |= TS_CARR_ON;
		ttwakeup(tp);
	}
	return (1);
}

/*
 * Default modem control routine (for other line disciplines).
 * Return argument flag, to turn off device on carrier drop.
 */
nullmodem(tp, flag)
	register struct tty *tp;
	int flag;
{
	
	if (flag)
		tp->t_state |= TS_CARR_ON;
	else {
		tp->t_state &= ~TS_CARR_ON;
		if ((tp->t_cflag&CLOCAL) == 0) {
			if (tp->t_session && tp->t_session->s_leader)
				psignal(tp->t_session->s_leader, SIGHUP);
			return (0);
		}
	}
	return (1);
}

/*
 * reinput pending characters after state switch
 * call at spltty().
 */
ttypend(tp)
	register struct tty *tp;
{
	register c;
	char *hd, *tl;

	tp->t_lflag &= ~PENDIN;
	tp->t_state |= TS_TYPEN;
	hd = tp->t_raw.rb_hd;
	tl = tp->t_raw.rb_tl;
	flushq(&tp->t_raw);
	while (hd != tl) {
		ttyinput(*hd, tp);
		hd = RB_SUCC(&tp->t_raw, hd);
	}
	tp->t_state &= ~TS_TYPEN;
}

/*
 * Process input of a single character received on a tty.
 */
ttyinput(c, tp)
	register c;
	register struct tty *tp;
{
	register int iflag = tp->t_iflag;
	register int lflag = tp->t_lflag;
	register u_char *cc = tp->t_cc;
	int i, err;

	/*
	 * If input is pending take it first.
	 */
	if (lflag&PENDIN)
		ttypend(tp);
	/*
	 * Gather stats.
	 */
	tk_nin++;
	if (lflag&ICANON) {
		tk_cancc++;
		tp->t_cancc++;
	} else {
		tk_rawcc++;
		tp->t_rawcc++;
	}
	/*
	 * Handle exceptional conditions (break, parity, framing).
	 */
	if (err = (c&TTY_ERRORMASK)) {
		c &= ~TTY_ERRORMASK;
		if (err&TTY_FE && !c) {		/* break */
			if (iflag&IGNBRK)
				goto endcase;
			else if (iflag&BRKINT && lflag&ISIG && 
				(cc[VINTR] != _POSIX_VDISABLE))
				c = cc[VINTR];
			else if (iflag&PARMRK)
				goto parmrk;
		} else if ((err&TTY_PE && iflag&INPCK) || err&TTY_FE) {
			if (iflag&IGNPAR)
				goto endcase;
			else if (iflag&PARMRK) {
parmrk:
				putc(0377|TTY_QUOTE, &tp->t_raw);
				putc(0|TTY_QUOTE, &tp->t_raw);
				putc(c|TTY_QUOTE, &tp->t_raw);
				goto endcase;
			} else
				c = 0;
		}
	}
	/*
	 * In tandem mode, check high water mark.
	 */
	if (iflag&IXOFF)
		ttyblock(tp);
	if ((tp->t_state&TS_TYPEN) == 0 && (iflag&ISTRIP))
		c &= ~0x80;
	if ((tp->t_lflag&EXTPROC) == 0) {
		/*
		 * Check for literal nexting very first
		 */
		if (tp->t_state&TS_LNCH) {
			c |= TTY_QUOTE;
			tp->t_state &= ~TS_LNCH;
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
		if (lflag&IEXTEN) {
			if (CCEQ(cc[VLNEXT], c)) {
				if (lflag&ECHO) {
					if (lflag&ECHOE)
						ttyoutstr("^\b", tp);
					else
						ttyecho(c, tp);
				}
				tp->t_state |= TS_LNCH;
				goto endcase;
			}
			if (CCEQ(cc[VDISCARD], c)) {
				if (lflag&FLUSHO)
					tp->t_lflag &= ~FLUSHO;
				else {
					ttyflush(tp, FWRITE);
					ttyecho(c, tp);
					if (RB_LEN(&tp->t_raw) + RB_LEN(&tp->t_can))
						ttyretype(tp);
					tp->t_lflag |= FLUSHO;
				}
				goto startoutput;
			}
		}
		/*
		 * Signals.
		 */
		if (lflag&ISIG) {
			if (CCEQ(cc[VINTR], c) || CCEQ(cc[VQUIT], c)) {
				if ((lflag&NOFLSH) == 0)
					ttyflush(tp, FREAD|FWRITE);
				ttyecho(c, tp);
				pgsignal(tp->t_pgrp,
				    CCEQ(cc[VINTR], c) ? SIGINT : SIGQUIT, 1);
				goto endcase;
			}
			if (CCEQ(cc[VSUSP], c)) {
				if ((lflag&NOFLSH) == 0)
					ttyflush(tp, FREAD);
				ttyecho(c, tp);
				pgsignal(tp->t_pgrp, SIGTSTP, 1);
				goto endcase;
			}
		}
		/*
		 * Handle start/stop characters.
		 */
		if (iflag&IXON) {
			if (CCEQ(cc[VSTOP], c)) {
				if ((tp->t_state&TS_TTSTOP) == 0) {
					tp->t_state |= TS_TTSTOP;
					(*cdevsw[major(tp->t_dev)].d_stop)(tp,
					   0);
					return;
				}
				if (!CCEQ(cc[VSTART], c))
					return;
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
			if (iflag&IGNCR)
				goto endcase;
			else if (iflag&ICRNL)
				c = '\n';
		} else if (c == '\n' && iflag&INLCR)
			c = '\r';
	}
	if ((tp->t_lflag&EXTPROC) == 0 && lflag&ICANON) {
		/*
		 * From here on down canonical mode character
		 * processing takes place.
		 */
		/*
		 * erase (^H / ^?)
		 */
		if (CCEQ(cc[VERASE], c)) {
			if (RB_LEN(&tp->t_raw))
				ttyrub(unputc(&tp->t_raw), tp);
			goto endcase;
		}
		/*
		 * kill (^U)
		 */
		if (CCEQ(cc[VKILL], c)) {
			if (lflag&ECHOKE && RB_LEN(&tp->t_raw) == tp->t_rocount &&
			    (lflag&ECHOPRT) == 0) {
				while (RB_LEN(&tp->t_raw))
					ttyrub(unputc(&tp->t_raw), tp);
			} else {
				ttyecho(c, tp);
				if (lflag&ECHOK || lflag&ECHOKE)
					ttyecho('\n', tp);
				while (getc(&tp->t_raw) > 0)
					;
				tp->t_rocount = 0;
			}
			tp->t_state &= ~TS_LOCAL;
			goto endcase;
		}
		/*
		 * word erase (^W)
		 */
		if (CCEQ(cc[VWERASE], c)) {	
			int ctype;
			int alt = lflag&ALTWERASE;

			/* 
			 * erase whitespace 
			 */
			while ((c = unputc(&tp->t_raw)) == ' ' || c == '\t')
				ttyrub(c, tp);
			if (c == -1)
				goto endcase;
			/*
			 * erase last char of word and remember the
			 * next chars type (for ALTWERASE)
			 */
			ttyrub(c, tp);
			c = unputc(&tp->t_raw);
			if (c == -1)
				goto endcase;
			ctype = ISALPHA(c);
			/*
			 * erase rest of word
			 */
			do {
				ttyrub(c, tp);
				c = unputc(&tp->t_raw);
				if (c == -1)
					goto endcase;
			} while (c != ' ' && c != '\t' && 
				(alt == 0 || ISALPHA(c) == ctype));
			(void) putc(c, &tp->t_raw);
			goto endcase;
		}
		/*
		 * reprint line (^R)
		 */
		if (CCEQ(cc[VREPRINT], c)) {
			ttyretype(tp);
			goto endcase;
		}
		/*
		 * ^T - kernel info and generate SIGINFO
		 */
		if (CCEQ(cc[VSTATUS], c)) {
			pgsignal(tp->t_pgrp, SIGINFO, 1);
			if ((lflag&NOKERNINFO) == 0)
				ttyinfo(tp);
			goto endcase;
		}
	}
	/*
	 * Check for input buffer overflow
	 */
	if (RB_LEN(&tp->t_raw)+RB_LEN(&tp->t_can) >= TTYHOG) {
		if (iflag&IMAXBEL) {
			if (RB_LEN(&tp->t_out) < tp->t_hiwat)
				(void) ttyoutput(CTRL('g'), tp);
		} else
			ttyflush(tp, FREAD | FWRITE);
		goto endcase;
	}
	/*
	 * Put data char in q for user and
	 * wakeup on seeing a line delimiter.
	 */
	if (putc(c, &tp->t_raw) >= 0) {
		if ((lflag&ICANON) == 0) {
			ttwakeup(tp);
			ttyecho(c, tp);
			goto endcase;
		}
		if (ttbreakc(c)) {
			tp->t_rocount = 0;
			catb(&tp->t_raw, &tp->t_can);
			ttwakeup(tp);
		} else if (tp->t_rocount++ == 0)
			tp->t_rocol = tp->t_col;
		if (tp->t_state&TS_ERASE) {
			/*
			 * end of prterase \.../
			 */
			tp->t_state &= ~TS_ERASE;
			(void) ttyoutput('/', tp);
		}
		i = tp->t_col;
		ttyecho(c, tp);
		if (CCEQ(cc[VEOF], c) && lflag&ECHO) {
			/*
			 * Place the cursor over the '^' of the ^D.
			 */
			i = MIN(2, tp->t_col - i);
			while (i > 0) {
				(void) ttyoutput('\b', tp);
				i--;
			}
		}
	}
endcase:
	/*
	 * IXANY means allow any character to restart output.
	 */
	if ((tp->t_state&TS_TTSTOP) && (iflag&IXANY) == 0 && 
	    cc[VSTART] != cc[VSTOP])
		return;
restartoutput:
	tp->t_state &= ~TS_TTSTOP;
	tp->t_lflag &= ~FLUSHO;
startoutput:
	ttstart(tp);
}

/*
 * Output a single character on a tty, doing output processing
 * as needed (expanding tabs, newline processing, etc.).
 * Returns < 0 if putc succeeds, otherwise returns char to resend.
 * Must be recursive.
 */
ttyoutput(c, tp)
	register c;
	register struct tty *tp;
{
	register int col;
	register long oflag = tp->t_oflag;
	
	if ((oflag&OPOST) == 0) {
		if (tp->t_lflag&FLUSHO) 
			return (-1);
		if (putc(c, &tp->t_out))
			return (c);
		tk_nout++;
		tp->t_outcc++;
		return (-1);
	}
	c &= TTY_CHARMASK;
	/*
	 * Do tab expansion if OXTABS is set.
	 * Special case if we have external processing, we don't
	 * do the tab expansion because we'll probably get it
	 * wrong.  If tab expansion needs to be done, let it
	 * happen externally.
	 */
	if (c == '\t' && oflag&OXTABS && (tp->t_lflag&EXTPROC) == 0) {
		register int s;

		c = 8 - (tp->t_col&7);
		if ((tp->t_lflag&FLUSHO) == 0) {
			int i;

			s = spltty();		/* don't interrupt tabs */
#ifdef was
			c -= b_to_q("        ", c, &tp->t_outq);
#else
			i = min (c, RB_CONTIGPUT(&tp->t_out));
			bcopy("        ", tp->t_out.rb_tl, i);
			tp->t_out.rb_tl =
				RB_ROLLOVER(&tp->t_out, tp->t_out.rb_tl+i);
			i = min (c-i, RB_CONTIGPUT(&tp->t_out));

			/* off end and still have space? */
			if (i) {
				bcopy("        ", tp->t_out.rb_tl, i);
				tp->t_out.rb_tl =
				   RB_ROLLOVER(&tp->t_out, tp->t_out.rb_tl+i);
			}
#endif
			tk_nout += c;
			tp->t_outcc += c;
			splx(s);
		}
		tp->t_col += c;
		return (c ? -1 : '\t');
	}
	if (c == CEOT && oflag&ONOEOT)
		return (-1);
	tk_nout++;
	tp->t_outcc++;
	/*
	 * Newline translation: if ONLCR is set,
	 * translate newline into "\r\n".
	 */
	if (c == '\n' && (tp->t_oflag&ONLCR) && ttyoutput('\r', tp) >= 0)
		return (c);
	if ((tp->t_lflag&FLUSHO) == 0 && putc(c, &tp->t_out))
		return (c);

	col = tp->t_col;
	switch (CCLASS(c)) {

	case ORDINARY:
		col++;

	case CONTROL:
		break;

	case BACKSPACE:
		if (col > 0)
			col--;
		break;

	case NEWLINE:
		col = 0;
		break;

	case TAB:
		col = (col + 8) &~ 0x7;
		break;

	case RETURN:
		col = 0;
	}
	tp->t_col = col;
	return (-1);
}

/*
 * Process a read call on a tty device.
 */
ttread(tp, uio, flag)
	register struct tty *tp;
	struct uio *uio;
{
	register struct ringb *qp;
	register int c;
	register long lflag;
	register u_char *cc = tp->t_cc;
	register struct proc *p = curproc;
	int s, first, error = 0;

loop:
	lflag = tp->t_lflag;
	s = spltty();
	/*
	 * take pending input first 
	 */
	if (lflag&PENDIN)
		ttypend(tp);
	splx(s);

	/*
	 * Hang process if it's in the background.
	 */
	if (isbackground(p, tp)) {
		if ((p->p_sigignore & sigmask(SIGTTIN)) ||
		   (p->p_sigmask & sigmask(SIGTTIN)) ||
		    p->p_flag&SPPWAIT || p->p_pgrp->pg_jobc == 0)
			return (EIO);
		pgsignal(p->p_pgrp, SIGTTIN, 1);
		if (error = ttysleep(tp, (caddr_t)&lbolt, TTIPRI | PCATCH, 
		    ttybg, 0)) 
			return (error);
		goto loop;
	}

	/*
	 * If canonical, use the canonical queue,
	 * else use the raw queue.
	 */
	qp = lflag&ICANON ? &tp->t_can : &tp->t_raw;

	/*
	 * If there is no input, sleep on rawq
	 * awaiting hardware receipt and notification.
	 * If we have data, we don't need to check for carrier.
	 */
	s = spltty();
	if (RB_LEN(qp) <= 0) {
		int carrier;

		carrier = (tp->t_state&TS_CARR_ON) || (tp->t_cflag&CLOCAL);
		if (!carrier && tp->t_state&TS_ISOPEN) {
			splx(s);
			return (0);	/* EOF */
		}
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		error = ttysleep(tp, (caddr_t)&tp->t_raw, TTIPRI | PCATCH,
		    carrier ? ttyin : ttopen, 0);
		splx(s);
		if (error)
			return (error);
		goto loop;
	}
	splx(s);

	/*
	 * Input present, check for input mapping and processing.
	 */
	first = 1;
	while ((c = getc(qp)) >= 0) {
		/*
		 * delayed suspend (^Y)
		 */
		if (CCEQ(cc[VDSUSP], c) && lflag&ISIG) {
			pgsignal(tp->t_pgrp, SIGTSTP, 1);
			if (first) {
				if (error = ttysleep(tp, (caddr_t)&lbolt,
				    TTIPRI | PCATCH, ttybg, 0))
					break;
				goto loop;
			}
			break;
		}
		/*
		 * Interpret EOF only in canonical mode.
		 */
		if (CCEQ(cc[VEOF], c) && lflag&ICANON)
			break;
		/*
		 * Give user character.
		 */
 		error = ureadc(c, uio);
		if (error)
			break;
 		if (uio->uio_resid == 0)
			break;
		/*
		 * In canonical mode check for a "break character"
		 * marking the end of a "line of input".
		 */
		if (lflag&ICANON && ttbreakc(c))
			break;
		first = 0;
	}
	/*
	 * Look to unblock output now that (presumably)
	 * the input queue has gone down.
	 */
#if 0
	if (tp->t_state&TS_TBLOCK && RB_LEN(&tp->t_raw) < TTYHOG/5) {
		if (cc[VSTART] != _POSIX_VDISABLE &&
		    putc(cc[VSTART], &tp->t_out) == 0) {
			tp->t_state &= ~TS_TBLOCK;
			ttstart(tp);
		}
	}
#else
#define	TS_RTSBLOCK	TS_TBLOCK	/* XXX */
#define	RB_I_LOW_WATER	((RBSZ - 2 * 256) * 7 / 8)	/* XXX */
	if (tp->t_state&TS_RTSBLOCK && RB_LEN(&tp->t_raw) <= RB_I_LOW_WATER) {
		tp->t_state &= ~TS_RTSBLOCK;
		ttstart(tp);
	}
#endif
	return (error);
}

/*
 * Check the output queue on tp for space for a kernel message
 * (from uprintf/tprintf).  Allow some space over the normal
 * hiwater mark so we don't lose messages due to normal flow
 * control, but don't let the tty run amok.
 * Sleeps here are not interruptible, but we return prematurely
 * if new signals come in.
 */
ttycheckoutq(tp, wait)
	register struct tty *tp;
	int wait;
{
	int hiwat, s, oldsig;
	extern int wakeup();

	hiwat = tp->t_hiwat;
	s = spltty();
	if (curproc)
		oldsig = curproc->p_sig;
	else
		oldsig = 0;
	if (RB_LEN(&tp->t_out) > hiwat + 200)
		while (RB_LEN(&tp->t_out) > hiwat) {
			ttstart(tp);
			if (wait == 0 || (curproc && curproc->p_sig != oldsig)) {
				splx(s);
				return (0);
			}
			timeout(wakeup, (caddr_t)&tp->t_out, hz);
			tp->t_state |= TS_ASLEEP;
			sleep((caddr_t)&tp->t_out, PZERO - 1);
		}
	splx(s);
	return (1);
}

/*
 * Process a write call on a tty device.
 */
ttwrite(tp, uio, flag)
	register struct tty *tp;
	register struct uio *uio;
{
	register char *cp;
	register int cc = 0, ce;
	register struct proc *p = curproc;
	int i, hiwat, cnt, error, s;
	char obuf[OBUFSIZ];

	hiwat = tp->t_hiwat;
	cnt = uio->uio_resid;
	error = 0;
loop:
	s = spltty();
	if ((tp->t_state&TS_CARR_ON) == 0 && (tp->t_cflag&CLOCAL) == 0) {
		if (tp->t_state&TS_ISOPEN) {
			splx(s);
			return (EIO);
		} else if (flag & IO_NDELAY) {
			splx(s);
			error = EWOULDBLOCK;
			goto out;
		} else {
			/*
			 * sleep awaiting carrier
			 */
			error = ttysleep(tp, (caddr_t)&tp->t_raw, 
					TTIPRI | PCATCH,ttopen, 0);
			splx(s);
			if (error)
				goto out;
			goto loop;
		}
	}
	splx(s);
	/*
	 * Hang the process if it's in the background.
	 */
	if (isbackground(p, tp) && 
	    tp->t_lflag&TOSTOP && (p->p_flag&SPPWAIT) == 0 &&
	    (p->p_sigignore & sigmask(SIGTTOU)) == 0 &&
	    (p->p_sigmask & sigmask(SIGTTOU)) == 0 &&
	     p->p_pgrp->pg_jobc) {
		pgsignal(p->p_pgrp, SIGTTOU, 1);
		if (error = ttysleep(tp, (caddr_t)&lbolt, TTIPRI | PCATCH, 
		    ttybg, 0))
			goto out;
		goto loop;
	}
	/*
	 * Process the user's data in at most OBUFSIZ
	 * chunks.  Perform any output translation.
	 * Keep track of high water mark, sleep on overflow
	 * awaiting device aid in acquiring new space.
	 */
	while (uio->uio_resid > 0 || cc > 0) {
		if (tp->t_lflag&FLUSHO) {
			uio->uio_resid = 0;
			return (0);
		}
		if (RB_LEN(&tp->t_out) > hiwat)
			goto ovhiwat;
		/*
		 * Grab a hunk of data from the user,
		 * unless we have some leftover from last time.
		 */
		if (cc == 0) {
			cc = min(uio->uio_resid, OBUFSIZ);
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
		 * bits in partab), call ttyoutput.  After processing
		 * a hunk of data, look for FLUSHO so ^O's will take effect
		 * immediately.
		 */
		while (cc > 0) {
			if ((tp->t_oflag&OPOST) == 0)
				ce = cc;
			else {
				ce = cc - scanc((unsigned)cc, (u_char *)cp,
				   (u_char *)partab, CCLASSMASK);
				/*
				 * If ce is zero, then we're processing
				 * a special character through ttyoutput.
				 */
				if (ce == 0) {
					tp->t_rocount = 0;
					if (ttyoutput(*cp, tp) >= 0) {
					    /* no c-lists, wait a bit */
					    ttstart(tp);
printf("\nttysleep - no c-lists\n");	/* XXX */
					    if (error = ttysleep(tp, 
						(caddr_t)&lbolt,
						 TTOPRI | PCATCH, ttybuf, 0))
						    break;
					    goto loop;
					}
					cp++, cc--;
					if ((tp->t_lflag&FLUSHO) ||
					    RB_LEN(&tp->t_out) > hiwat)
						goto ovhiwat;
					continue;
				}
			}
			/*
			 * A bunch of normal characters have been found,
			 * transfer them en masse to the output queue and
			 * continue processing at the top of the loop.
			 * If there are any further characters in this
			 * <= OBUFSIZ chunk, the first should be a character
			 * requiring special handling by ttyoutput.
			 */
			tp->t_rocount = 0;
#ifdef was
			i = b_to_q(cp, ce, &tp->t_outq);
			ce -= i;
#else
			i = ce;
			ce = min (ce, RB_CONTIGPUT(&tp->t_out));
			bcopy(cp, tp->t_out.rb_tl, ce);
			tp->t_out.rb_tl = RB_ROLLOVER(&tp->t_out,
				tp->t_out.rb_tl + ce);
			i -= ce;
			if (i > 0) {
				int ii;

				ii = min (i, RB_CONTIGPUT(&tp->t_out));
				bcopy(cp + ce, tp->t_out.rb_tl, ii);
				tp->t_out.rb_tl = RB_ROLLOVER(&tp->t_out,
					tp->t_out.rb_tl + ii);
				i -= ii;
				ce += ii;
			}
#endif
			tp->t_col += ce;
			cp += ce, cc -= ce, tk_nout += ce;
			tp->t_outcc += ce;
			if (i > 0) {
				ttstart(tp);
				if (RB_CONTIGPUT(&tp->t_out) > 0)
					goto loop;	/* synchronous/fast */
				/* out of space, wait a bit */
				tp->t_state |= TS_ASLEEP;
				if (error = ttysleep(tp, (caddr_t)&tp->t_out,
					    TTOPRI | PCATCH, ttybuf, 0))
					break;
				goto loop;
			}
			if (tp->t_lflag&FLUSHO || RB_LEN(&tp->t_out) > hiwat)
				break;
		}
		ttstart(tp);
	}
out:
	/*
	 * If cc is nonzero, we leave the uio structure inconsistent,
	 * as the offset and iov pointers have moved forward,
	 * but it doesn't matter (the call will either return short
	 * or restart with a new uio).
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
	if (RB_LEN(&tp->t_out) <= hiwat) {
		splx(s);
		goto loop;
	}
	if (flag & IO_NDELAY) {
		splx(s);
		uio->uio_resid += cc;
		if (uio->uio_resid == cnt)
			return (EWOULDBLOCK);
		return (0);
	}
	tp->t_state |= TS_ASLEEP;
	error = ttysleep(tp, (caddr_t)&tp->t_out, TTOPRI | PCATCH, ttyout, 0);
	splx(s);
	if (error)
		goto out;
	goto loop;
}

/*
 * Rubout one character from the rawq of tp
 * as cleanly as possible.
 */
ttyrub(c, tp)
	register c;
	register struct tty *tp;
{
	char *cp;
	register int savecol;
	int s;

	if ((tp->t_lflag&ECHO) == 0 || (tp->t_lflag&EXTPROC))
		return;
	tp->t_lflag &= ~FLUSHO;	
	if (tp->t_lflag&ECHOE) {
		if (tp->t_rocount == 0) {
			/*
			 * Screwed by ttwrite; retype
			 */
			ttyretype(tp);
			return;
		}
		if (c == ('\t'|TTY_QUOTE) || c == ('\n'|TTY_QUOTE))
			ttyrubo(tp, 2);
		else switch (CCLASS(c &= TTY_CHARMASK)) {

		case ORDINARY:
			ttyrubo(tp, 1);
			break;

		case VTAB:
		case BACKSPACE:
		case CONTROL:
		case RETURN:
		case NEWLINE:
			if (tp->t_lflag&ECHOCTL)
				ttyrubo(tp, 2);
			break;

		case TAB: {
			int c;

			if (tp->t_rocount < RB_LEN(&tp->t_raw)) {
				ttyretype(tp);
				return;
			}
			s = spltty();
			savecol = tp->t_col;
			tp->t_state |= TS_CNTTB;
			tp->t_lflag |= FLUSHO;
			tp->t_col = tp->t_rocol;
			cp = tp->t_raw.rb_hd;
			for (c = nextc(&cp, &tp->t_raw); c ;
				c = nextc(&cp, &tp->t_raw))
				ttyecho(c, tp);
			tp->t_lflag &= ~FLUSHO;
			tp->t_state &= ~TS_CNTTB;
			splx(s);
			/*
			 * savecol will now be length of the tab
			 */
			savecol -= tp->t_col;
			tp->t_col += savecol;
			if (savecol > 8)
				savecol = 8;		/* overflow screw */
			while (--savecol >= 0)
				(void) ttyoutput('\b', tp);
			break;
		}

		default:
			/* XXX */
			printf("ttyrub: would panic c = %d, val = %d\n",
				c, CCLASS(c));
			/*panic("ttyrub");*/
		}
	} else if (tp->t_lflag&ECHOPRT) {
		if ((tp->t_state&TS_ERASE) == 0) {
			(void) ttyoutput('\\', tp);
			tp->t_state |= TS_ERASE;
		}
		ttyecho(c, tp);
	} else
		ttyecho(tp->t_cc[VERASE], tp);
	tp->t_rocount--;
}

/*
 * Crt back over cnt chars perhaps
 * erasing them.
 */
ttyrubo(tp, cnt)
	register struct tty *tp;
	int cnt;
{

	while (--cnt >= 0)
		ttyoutstr("\b \b", tp);
}

/*
 * Reprint the rawq line.
 * We assume c_cc has already been checked.
 */
ttyretype(tp)
	register struct tty *tp;
{
	char *cp;
	int s, c;

	if (tp->t_cc[VREPRINT] != _POSIX_VDISABLE)
		ttyecho(tp->t_cc[VREPRINT], tp);
	(void) ttyoutput('\n', tp);

	s = spltty();
	cp = tp->t_can.rb_hd;
	for (c = nextc(&cp, &tp->t_can); c ; c = nextc(&cp, &tp->t_can))
		ttyecho(c, tp);
	cp = tp->t_raw.rb_hd;
	for (c = nextc(&cp, &tp->t_raw); c ; c = nextc(&cp, &tp->t_raw))
		ttyecho(c, tp);
	tp->t_state &= ~TS_ERASE;
	splx(s);

	tp->t_rocount = RB_LEN(&tp->t_raw);
	tp->t_rocol = 0;
}

/*
 * Echo a typed character to the terminal.
 */
ttyecho(c, tp)
	register c;
	register struct tty *tp;
{
	if ((tp->t_state & TS_CNTTB) == 0)
		tp->t_lflag &= ~FLUSHO;
	if (tp->t_lflag & EXTPROC)
		return;
	if ((tp->t_lflag & ECHO) == 0) {
		if ((tp->t_lflag & ECHONL) == 0)
			return;
		else if  (c != '\n')
			return;
	}
	if (tp->t_lflag & ECHOCTL) {
		if ((c & TTY_CHARMASK) <= 037 && c != '\t' && c != '\n' ||
		    c == 0177) {
			(void) ttyoutput('^', tp);
			c &= TTY_CHARMASK;
			if (c == 0177)
				c = '?';
			else
				c += 'A' - 1;
		}
	}
	(void) ttyoutput(c, tp);
}

/*
 * send string cp to tp
 */
ttyoutstr(cp, tp)
	register char *cp;
	register struct tty *tp;
{
	register char c;

	while (c = *cp++)
		(void) ttyoutput(c, tp);
}

/*
 * Wake up any readers on a tty.
 */
ttwakeup(tp)
	register struct tty *tp;
{

	if (tp->t_rsel) {
		selwakeup(tp->t_rsel, tp->t_state&TS_RCOLL);
		tp->t_state &= ~TS_RCOLL;
		tp->t_rsel = 0;
	}
	if (tp->t_state & TS_ASYNC)
		pgsignal(tp->t_pgrp, SIGIO, 1); 
	wakeup((caddr_t)&tp->t_raw);
}

/*
 * Look up a code for a specified speed in a conversion table;
 * used by drivers to map software speed values to hardware parameters.
 */
ttspeedtab(speed, table)
	register struct speedtab *table;
{

	for ( ; table->sp_speed != -1; table++)
		if (table->sp_speed == speed)
			return (table->sp_code);
	return (-1);
}

/*
 * set tty hi and low water marks
 *
 * Try to arrange the dynamics so there's about one second
 * from hi to low water.
 * 
 */
ttsetwater(tp)
	struct tty *tp;
{
	register cps = tp->t_ospeed / 10;
	register x;

#define clamp(x, h, l) ((x)>h ? h : ((x)<l) ? l : (x))
	tp->t_lowat = x = clamp(cps/2, TTMAXLOWAT, TTMINLOWAT);
	x += cps;
	x = clamp(x, TTMAXHIWAT, TTMINHIWAT);
	tp->t_hiwat = roundup(x, CBSIZE);
#undef clamp
}

/*
 * Report on state of foreground process group.
 */
ttyinfo(tp)
	register struct tty *tp;
{
	register struct proc *p, *pick;
	struct timeval utime, stime;
	int tmp;

	if (ttycheckoutq(tp,0) == 0) 
		return;

	/* Print load average. */
	tmp = (averunnable[0] * 100 + FSCALE / 2) >> FSHIFT;
	ttyprintf(tp, "load: %d.%02d ", tmp / 100, tmp % 100);

	if (tp->t_session == NULL)
		ttyprintf(tp, "not a controlling terminal\n");
	else if (tp->t_pgrp == NULL)
		ttyprintf(tp, "no foreground process group\n");
	else if ((p = tp->t_pgrp->pg_mem) == NULL)
		ttyprintf(tp, "empty foreground process group\n");
	else {
		/* Pick interesting process. */
		for (pick = NULL; p != NULL; p = p->p_pgrpnxt)
			if (proc_compare(pick, p))
				pick = p;

		ttyprintf(tp, " cmd: %s %d [%s] ", pick->p_comm, pick->p_pid,
		    pick->p_stat == SRUN ? "running" :
		    pick->p_wmesg ? pick->p_wmesg : "iowait");

		/*
		 * Lock out clock if process is running; get user/system
		 * cpu time.
		 */
		if (curproc == pick)
			tmp = splclock();
		utime = pick->p_utime;
		stime = pick->p_stime;
		if (curproc == pick)
			splx(tmp);

		/* Print user time. */
		ttyprintf(tp, "%d.%02du ",
		    utime.tv_sec, (utime.tv_usec + 5000) / 10000);

		/* Print system time. */
		ttyprintf(tp, "%d.%02ds ",
		    stime.tv_sec, (stime.tv_usec + 5000) / 10000);

#define	pgtok(a)	(((a) * NBPG) / 1024)
		/* Print percentage cpu, resident set size. */
		tmp = pick->p_pctcpu * 10000 + FSCALE / 2 >> FSHIFT;
		ttyprintf(tp, "%d%% %dk\n",
		   tmp / 100, pgtok(pick->p_vmspace->vm_rssize));
	}
	tp->t_rocount = 0;	/* so pending input will be retyped if BS */
}

/*
 * Returns 1 if p2 is "better" than p1
 *
 * The algorithm for picking the "interesting" process is thus:
 *
 *	1) (Only foreground processes are eligable - implied)
 *	2) Runnable processes are favored over anything
 *	   else.  The runner with the highest cpu
 *	   utilization is picked (p_cpu).  Ties are
 *	   broken by picking the highest pid.
 *	3  Next, the sleeper with the shortest sleep
 *	   time is favored.  With ties, we pick out
 *	   just "short-term" sleepers (SSINTR == 0).
 *	   Further ties are broken by picking the highest
 *	   pid.
 *
 */
#define isrun(p)	(((p)->p_stat == SRUN) || ((p)->p_stat == SIDL))
#define TESTAB(a, b)    ((a)<<1 | (b))
#define ONLYA   2
#define ONLYB   1
#define BOTH    3

static int
proc_compare(p1, p2)
	register struct proc *p1, *p2;
{

	if (p1 == NULL)
		return (1);
	/*
	 * see if at least one of them is runnable
	 */
	switch (TESTAB(isrun(p1), isrun(p2))) {
	case ONLYA:
		return (0);
	case ONLYB:
		return (1);
	case BOTH:
		/*
		 * tie - favor one with highest recent cpu utilization
		 */
		if (p2->p_cpu > p1->p_cpu)
			return (1);
		if (p1->p_cpu > p2->p_cpu)
			return (0);
		return (p2->p_pid > p1->p_pid);	/* tie - return highest pid */
	}
	/*
 	 * weed out zombies
	 */
	switch (TESTAB(p1->p_stat == SZOMB, p2->p_stat == SZOMB)) {
	case ONLYA:
		return (1);
	case ONLYB:
		return (0);
	case BOTH:
		return (p2->p_pid > p1->p_pid); /* tie - return highest pid */
	}
	/* 
	 * pick the one with the smallest sleep time
	 */
	if (p2->p_slptime > p1->p_slptime)
		return (0);
	if (p1->p_slptime > p2->p_slptime)
		return (1);
	/*
	 * favor one sleeping in a non-interruptible sleep
	 */
	if (p1->p_flag&SSINTR && (p2->p_flag&SSINTR) == 0)
		return (1);
	if (p2->p_flag&SSINTR && (p1->p_flag&SSINTR) == 0)
		return (0);
	return (p2->p_pid > p1->p_pid);		/* tie - return highest pid */
}

/*
 * Output char to tty; console putchar style.
 */
tputchar(c, tp)
	int c;
	struct tty *tp;
{
	register s = spltty();

	if ((tp->t_state & (TS_CARR_ON|TS_ISOPEN)) == (TS_CARR_ON|TS_ISOPEN)) {
		if (c == '\n')
			(void) ttyoutput('\r', tp);
		(void) ttyoutput(c, tp);
		ttstart(tp);
		splx(s);
		return (0);
	}
	splx(s);
	return (-1);
}

/*
 * Sleep on chan, returning ERESTART if tty changed
 * while we napped and returning any errors (e.g. EINTR/ETIMEDOUT)
 * reported by tsleep.  If the tty is revoked, restarting a pending
 * call will redo validation done at the start of the call.
 */
ttysleep(tp, chan, pri, wmesg, timo)
	struct tty *tp;
	caddr_t chan;
	int pri;
	char *wmesg;
	int timo;
{
	int error;
	short gen = tp->t_gen;

	if (error = tsleep(chan, pri, wmesg, timo))
		return (error);
	if (tp->t_gen != gen)
		return (ERESTART);
	return (0);
}
