/*	$NetBSD: ibcs2_ioctl.c,v 1.6 1995/03/14 15:12:28 scottb Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1994, 1995 Scott Bartram
 * All rights reserved.
 *
 * based on compat/sunos/sun_ioctl.c
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/capsicum.h>
#include <sys/consio.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/kbio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/tty.h>

#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_socksys.h>
#include <i386/ibcs2/ibcs2_stropts.h>
#include <i386/ibcs2/ibcs2_proto.h>
#include <i386/ibcs2/ibcs2_termios.h>
#include <i386/ibcs2/ibcs2_util.h>
#include <i386/ibcs2/ibcs2_ioctl.h>

static void stios2btios(struct ibcs2_termios *, struct termios *);
static void btios2stios(struct termios *, struct ibcs2_termios *);
static void stios2stio(struct ibcs2_termios *, struct ibcs2_termio *);
static void stio2stios(struct ibcs2_termio *, struct ibcs2_termios *);

/*
 * iBCS2 ioctl calls.
 */

struct speedtab {
	int sp_speed;			/* Speed. */
	int sp_code;			/* Code. */
};

static struct speedtab sptab[] = {
	{ 0, 0 },
	{ 50, 1 },
	{ 75, 2 },
	{ 110, 3 },
	{ 134, 4 },
	{ 135, 4 },
	{ 150, 5 },
	{ 200, 6 },
	{ 300, 7 },
	{ 600, 8 },
	{ 1200, 9 },
	{ 1800, 10 },
	{ 2400, 11 },
	{ 4800, 12 },
	{ 9600, 13 },
	{ 19200, 14 },
	{ 38400, 15 },
	{ -1, -1 }
};

static u_long s2btab[] = { 
	0,
	50,
	75,
	110,
	134,
	150,
	200,
	300,
	600,
	1200,
	1800,
	2400,
	4800,
	9600,
	19200,
	38400,
};

static int
ttspeedtab(int speed, struct speedtab *table)
{

	for ( ; table->sp_speed != -1; table++)
		if (table->sp_speed == speed)
			return (table->sp_code);
	return (-1);
}

static void
stios2btios(st, bt)
	struct ibcs2_termios *st;
	struct termios *bt;
{
	register u_long l, r;

	l = st->c_iflag;	r = 0;
	if (l & IBCS2_IGNBRK)	r |= IGNBRK;
	if (l & IBCS2_BRKINT)	r |= BRKINT;
	if (l & IBCS2_IGNPAR)	r |= IGNPAR;
	if (l & IBCS2_PARMRK)	r |= PARMRK;
	if (l & IBCS2_INPCK)	r |= INPCK;
	if (l & IBCS2_ISTRIP)	r |= ISTRIP;
	if (l & IBCS2_INLCR)	r |= INLCR;
	if (l & IBCS2_IGNCR)	r |= IGNCR;
	if (l & IBCS2_ICRNL)	r |= ICRNL;
	if (l & IBCS2_IXON)	r |= IXON;
	if (l & IBCS2_IXANY)	r |= IXANY;
	if (l & IBCS2_IXOFF)	r |= IXOFF;
	if (l & IBCS2_IMAXBEL)	r |= IMAXBEL;
	bt->c_iflag = r;

	l = st->c_oflag;	r = 0;
	if (l & IBCS2_OPOST)	r |= OPOST;
	if (l & IBCS2_ONLCR)	r |= ONLCR;
	if (l & IBCS2_TAB3)	r |= TAB3;
	bt->c_oflag = r;

	l = st->c_cflag;	r = 0;
	switch (l & IBCS2_CSIZE) {
	case IBCS2_CS5:		r |= CS5; break;
	case IBCS2_CS6:		r |= CS6; break;
	case IBCS2_CS7:		r |= CS7; break;
	case IBCS2_CS8:		r |= CS8; break;
	}
	if (l & IBCS2_CSTOPB)	r |= CSTOPB;
	if (l & IBCS2_CREAD)	r |= CREAD;
	if (l & IBCS2_PARENB)	r |= PARENB;
	if (l & IBCS2_PARODD)	r |= PARODD;
	if (l & IBCS2_HUPCL)	r |= HUPCL;
	if (l & IBCS2_CLOCAL)	r |= CLOCAL;
	bt->c_cflag = r;

	bt->c_ispeed = bt->c_ospeed = s2btab[l & 0x0000000f];

	l = st->c_lflag;	r = 0;
	if (l & IBCS2_ISIG)	r |= ISIG;
	if (l & IBCS2_ICANON)	r |= ICANON;
	if (l & IBCS2_ECHO)	r |= ECHO;
	if (l & IBCS2_ECHOE)	r |= ECHOE;
	if (l & IBCS2_ECHOK)	r |= ECHOK;
	if (l & IBCS2_ECHONL)	r |= ECHONL;
	if (l & IBCS2_NOFLSH)	r |= NOFLSH;
	if (l & IBCS2_TOSTOP)	r |= TOSTOP;
	bt->c_lflag = r;

	bt->c_cc[VINTR]	=
	    st->c_cc[IBCS2_VINTR]  ? st->c_cc[IBCS2_VINTR]  : _POSIX_VDISABLE;
	bt->c_cc[VQUIT] =
	    st->c_cc[IBCS2_VQUIT]  ? st->c_cc[IBCS2_VQUIT]  : _POSIX_VDISABLE;
	bt->c_cc[VERASE] =
	    st->c_cc[IBCS2_VERASE] ? st->c_cc[IBCS2_VERASE] : _POSIX_VDISABLE;
	bt->c_cc[VKILL] =
	    st->c_cc[IBCS2_VKILL]  ? st->c_cc[IBCS2_VKILL]  : _POSIX_VDISABLE;
	if (bt->c_lflag & ICANON) {
		bt->c_cc[VEOF] =
		    st->c_cc[IBCS2_VEOF] ? st->c_cc[IBCS2_VEOF] : _POSIX_VDISABLE;
		bt->c_cc[VEOL] =
		    st->c_cc[IBCS2_VEOL] ? st->c_cc[IBCS2_VEOL] : _POSIX_VDISABLE;
	} else {
		bt->c_cc[VMIN]  = st->c_cc[IBCS2_VMIN];
		bt->c_cc[VTIME] = st->c_cc[IBCS2_VTIME];
	}
	bt->c_cc[VEOL2] =
	    st->c_cc[IBCS2_VEOL2]  ? st->c_cc[IBCS2_VEOL2]  : _POSIX_VDISABLE;
#if 0
	bt->c_cc[VSWTCH] =
	    st->c_cc[IBCS2_VSWTCH] ? st->c_cc[IBCS2_VSWTCH] : _POSIX_VDISABLE;
#endif
	bt->c_cc[VSTART] =
	    st->c_cc[IBCS2_VSTART] ? st->c_cc[IBCS2_VSTART] : _POSIX_VDISABLE;
	bt->c_cc[VSTOP] =
	    st->c_cc[IBCS2_VSTOP]  ? st->c_cc[IBCS2_VSTOP]  : _POSIX_VDISABLE;
	bt->c_cc[VSUSP] =
	    st->c_cc[IBCS2_VSUSP]  ? st->c_cc[IBCS2_VSUSP]  : _POSIX_VDISABLE;
	bt->c_cc[VDSUSP]   = _POSIX_VDISABLE;
	bt->c_cc[VREPRINT] = _POSIX_VDISABLE;
	bt->c_cc[VDISCARD] = _POSIX_VDISABLE;
	bt->c_cc[VWERASE]  = _POSIX_VDISABLE;
	bt->c_cc[VLNEXT]   = _POSIX_VDISABLE;
	bt->c_cc[VSTATUS]  = _POSIX_VDISABLE;
}

static void
btios2stios(bt, st)
	struct termios *bt;
	struct ibcs2_termios *st;
{
	register u_long l, r;

	l = bt->c_iflag;	r = 0;
	if (l & IGNBRK)		r |= IBCS2_IGNBRK;
	if (l & BRKINT)		r |= IBCS2_BRKINT;
	if (l & IGNPAR)		r |= IBCS2_IGNPAR;
	if (l & PARMRK)		r |= IBCS2_PARMRK;
	if (l & INPCK)		r |= IBCS2_INPCK;
	if (l & ISTRIP)		r |= IBCS2_ISTRIP;
	if (l & INLCR)		r |= IBCS2_INLCR;
	if (l & IGNCR)		r |= IBCS2_IGNCR;
	if (l & ICRNL)		r |= IBCS2_ICRNL;
	if (l & IXON)		r |= IBCS2_IXON;
	if (l & IXANY)		r |= IBCS2_IXANY;
	if (l & IXOFF)		r |= IBCS2_IXOFF;
	if (l & IMAXBEL)	r |= IBCS2_IMAXBEL;
	st->c_iflag = r;

	l = bt->c_oflag;	r = 0;
	if (l & OPOST)		r |= IBCS2_OPOST;
	if (l & ONLCR)		r |= IBCS2_ONLCR;
	if (l & TAB3)		r |= IBCS2_TAB3;
	st->c_oflag = r;

	l = bt->c_cflag;	r = 0;
	switch (l & CSIZE) {
	case CS5:		r |= IBCS2_CS5; break;
	case CS6:		r |= IBCS2_CS6; break;
	case CS7:		r |= IBCS2_CS7; break;
	case CS8:		r |= IBCS2_CS8; break;
	}
	if (l & CSTOPB)		r |= IBCS2_CSTOPB;
	if (l & CREAD)		r |= IBCS2_CREAD;
	if (l & PARENB)		r |= IBCS2_PARENB;
	if (l & PARODD)		r |= IBCS2_PARODD;
	if (l & HUPCL)		r |= IBCS2_HUPCL;
	if (l & CLOCAL)		r |= IBCS2_CLOCAL;
	st->c_cflag = r;

	l = bt->c_lflag;	r = 0;
	if (l & ISIG)		r |= IBCS2_ISIG;
	if (l & ICANON)		r |= IBCS2_ICANON;
	if (l & ECHO)		r |= IBCS2_ECHO;
	if (l & ECHOE)		r |= IBCS2_ECHOE;
	if (l & ECHOK)		r |= IBCS2_ECHOK;
	if (l & ECHONL)		r |= IBCS2_ECHONL;
	if (l & NOFLSH)		r |= IBCS2_NOFLSH;
	if (l & TOSTOP)		r |= IBCS2_TOSTOP;
	st->c_lflag = r;

	l = ttspeedtab(bt->c_ospeed, sptab);
	if ((int)l >= 0)
		st->c_cflag |= l;

	st->c_cc[IBCS2_VINTR] =
	    bt->c_cc[VINTR]  != _POSIX_VDISABLE ? bt->c_cc[VINTR]  : 0;
	st->c_cc[IBCS2_VQUIT] =
	    bt->c_cc[VQUIT]  != _POSIX_VDISABLE ? bt->c_cc[VQUIT]  : 0;
	st->c_cc[IBCS2_VERASE] =
	    bt->c_cc[VERASE] != _POSIX_VDISABLE ? bt->c_cc[VERASE] : 0;
	st->c_cc[IBCS2_VKILL] =
	    bt->c_cc[VKILL]  != _POSIX_VDISABLE ? bt->c_cc[VKILL]  : 0;
	if (bt->c_lflag & ICANON) {
		st->c_cc[IBCS2_VEOF] =
		    bt->c_cc[VEOF] != _POSIX_VDISABLE ? bt->c_cc[VEOF] : 0;
		st->c_cc[IBCS2_VEOL] =
		    bt->c_cc[VEOL] != _POSIX_VDISABLE ? bt->c_cc[VEOL] : 0;
	} else {
		st->c_cc[IBCS2_VMIN]  = bt->c_cc[VMIN];
		st->c_cc[IBCS2_VTIME] = bt->c_cc[VTIME];
	}
	st->c_cc[IBCS2_VEOL2] =
	    bt->c_cc[VEOL2]  != _POSIX_VDISABLE ? bt->c_cc[VEOL2]  : 0;
	st->c_cc[IBCS2_VSWTCH] =
	    0;
	st->c_cc[IBCS2_VSUSP] =
	    bt->c_cc[VSUSP]  != _POSIX_VDISABLE ? bt->c_cc[VSUSP]  : 0;
	st->c_cc[IBCS2_VSTART] =
	    bt->c_cc[VSTART] != _POSIX_VDISABLE ? bt->c_cc[VSTART] : 0;
	st->c_cc[IBCS2_VSTOP] =
	    bt->c_cc[VSTOP]  != _POSIX_VDISABLE ? bt->c_cc[VSTOP]  : 0;

	st->c_line = 0;
}

static void
stios2stio(ts, t)
	struct ibcs2_termios *ts;
	struct ibcs2_termio *t;
{
	t->c_iflag = ts->c_iflag;
	t->c_oflag = ts->c_oflag;
	t->c_cflag = ts->c_cflag;
	t->c_lflag = ts->c_lflag;
	t->c_line  = ts->c_line;
	bcopy(ts->c_cc, t->c_cc, IBCS2_NCC);
}

static void
stio2stios(t, ts)
	struct ibcs2_termio *t;
	struct ibcs2_termios *ts;
{
	ts->c_iflag = t->c_iflag;
	ts->c_oflag = t->c_oflag;
	ts->c_cflag = t->c_cflag;
	ts->c_lflag = t->c_lflag;
	ts->c_line  = t->c_line;
	bcopy(t->c_cc, ts->c_cc, IBCS2_NCC);
}

int
ibcs2_ioctl(td, uap)
	struct thread *td;
	struct ibcs2_ioctl_args *uap;
{
	struct proc *p = td->td_proc;
	cap_rights_t rights;
	struct file *fp;
	int error;

	error = fget(td, uap->fd, cap_rights_init(&rights, CAP_IOCTL), &fp);
	if (error != 0) {
		DPRINTF(("ibcs2_ioctl(%d): bad fd %d ", p->p_pid,
			 uap->fd));
		return EBADF;
	}

	if ((fp->f_flag & (FREAD|FWRITE)) == 0) {
		fdrop(fp, td);
		DPRINTF(("ibcs2_ioctl(%d): bad fp flag ", p->p_pid));
		return EBADF;
	}

	switch (uap->cmd) {
	case IBCS2_TCGETA:
	case IBCS2_XCGETA:
	case IBCS2_OXCGETA:
	    {
		struct termios bts;
		struct ibcs2_termios sts;
		struct ibcs2_termio st;
	
		if ((error = fo_ioctl(fp, TIOCGETA, (caddr_t)&bts,
		    td->td_ucred, td)) != 0)
			break;
	
		btios2stios (&bts, &sts);
		if (uap->cmd == IBCS2_TCGETA) {
			stios2stio (&sts, &st);
			error = copyout((caddr_t)&st, uap->data,
					sizeof (st));
#ifdef DEBUG_IBCS2
			if (error)
				DPRINTF(("ibcs2_ioctl(%d): copyout failed ",
					 p->p_pid));
#endif
			break;
		} else {
			error = copyout((caddr_t)&sts, uap->data,
					sizeof (sts));
			break;
		}
		/*NOTREACHED*/
	    }

	case IBCS2_TCSETA:
	case IBCS2_TCSETAW:
	case IBCS2_TCSETAF:
	    {
		struct termios bts;
		struct ibcs2_termios sts;
		struct ibcs2_termio st;

		if ((error = copyin(uap->data, (caddr_t)&st,
				    sizeof(st))) != 0) {
			DPRINTF(("ibcs2_ioctl(%d): TCSET copyin failed ",
				 p->p_pid));
			break;
		}

		/* get full BSD termios so we don't lose information */
		if ((error = fo_ioctl(fp, TIOCGETA, (caddr_t)&bts,
		    td->td_ucred, td)) != 0) {
			DPRINTF(("ibcs2_ioctl(%d): TCSET ctl failed fd %d ",
				 p->p_pid, uap->fd));
			break;
		}

		/*
		 * convert to iBCS2 termios, copy in information from
		 * termio, and convert back, then set new values.
		 */
		btios2stios(&bts, &sts);
		stio2stios(&st, &sts);
		stios2btios(&sts, &bts);

		error = fo_ioctl(fp, uap->cmd - IBCS2_TCSETA + TIOCSETA,
			      (caddr_t)&bts, td->td_ucred, td);
		break;
	    }

	case IBCS2_XCSETA:
	case IBCS2_XCSETAW:
	case IBCS2_XCSETAF:
	    {
		struct termios bts;
		struct ibcs2_termios sts;

		if ((error = copyin(uap->data, (caddr_t)&sts,
				    sizeof (sts))) != 0)
			break;
		stios2btios (&sts, &bts);
		error = fo_ioctl(fp, uap->cmd - IBCS2_XCSETA + TIOCSETA,
			      (caddr_t)&bts, td->td_ucred, td);
		break;
	    }

	case IBCS2_OXCSETA:
	case IBCS2_OXCSETAW:
	case IBCS2_OXCSETAF:
	    {
		struct termios bts;
		struct ibcs2_termios sts;

		if ((error = copyin(uap->data, (caddr_t)&sts,
				    sizeof (sts))) != 0)
			break;
		stios2btios (&sts, &bts);
		error = fo_ioctl(fp, uap->cmd - IBCS2_OXCSETA + TIOCSETA,
			      (caddr_t)&bts, td->td_ucred, td);
		break;
	    }

	case IBCS2_TCSBRK:
		DPRINTF(("ibcs2_ioctl(%d): TCSBRK ", p->p_pid));
		error = ENOSYS;
		break;

	case IBCS2_TCXONC:
	    {
		switch ((int)uap->data) {
		case 0:
		case 1:
			DPRINTF(("ibcs2_ioctl(%d): TCXONC ", p->p_pid));
			error = ENOSYS;
			break;
		case 2:
			error = fo_ioctl(fp, TIOCSTOP, (caddr_t)0,
			    td->td_ucred, td);
			break;
		case 3:
			error = fo_ioctl(fp, TIOCSTART, (caddr_t)1,
			    td->td_ucred, td);
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	    }

	case IBCS2_TCFLSH:
	    {
		int arg;

		switch ((int)uap->data) {
		case 0:
			arg = FREAD;
			break;
		case 1:
			arg = FWRITE;
			break;
		case 2:
			arg = FREAD | FWRITE;
			break;
		default:
			fdrop(fp, td);
			return EINVAL;
		}
		error = fo_ioctl(fp, TIOCFLUSH, (caddr_t)&arg, td->td_ucred,
		    td);
		break;
	    }

	case IBCS2_TIOCGWINSZ:
		uap->cmd = TIOCGWINSZ;
		error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_TIOCSWINSZ:
		uap->cmd = TIOCSWINSZ;
		error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_TIOCGPGRP:
	    {
		pid_t	pg_id;

		PROC_LOCK(p);
		pg_id = p->p_pgrp->pg_id;
		PROC_UNLOCK(p);
		error = copyout((caddr_t)&pg_id, uap->data,
				sizeof(pg_id));
		break;
	    }

	case IBCS2_TIOCSPGRP:	/* XXX - is uap->data a pointer to pgid? */
	    {
		struct setpgid_args sa;

		sa.pid = 0;
		sa.pgid = (int)uap->data;
		error = sys_setpgid(td, &sa);
		break;
	    }

	case IBCS2_TCGETSC:	/* SCO console - get scancode flags */
		error = EINTR;  /* ENOSYS; */
		break;

	case IBCS2_TCSETSC:	/* SCO console - set scancode flags */
		error = 0;   /* ENOSYS; */
		break;

	case IBCS2_JWINSIZE:	/* Unix to Jerq I/O control */
	    {
	        struct ibcs2_jwinsize {
		  char bytex, bytey; 
		  short bitx, bity;
	        } ibcs2_jwinsize;

		PROC_LOCK(p);
		SESS_LOCK(p->p_session);
                ibcs2_jwinsize.bytex = 80;
	          /* p->p_session->s_ttyp->t_winsize.ws_col; XXX */
	        ibcs2_jwinsize.bytey = 25;
                  /* p->p_session->s_ttyp->t_winsize.ws_row; XXX */
	        ibcs2_jwinsize.bitx = 
		  p->p_session->s_ttyp->t_winsize.ws_xpixel;
	        ibcs2_jwinsize.bity =
		  p->p_session->s_ttyp->t_winsize.ws_ypixel;
		SESS_UNLOCK(p->p_session);
		PROC_UNLOCK(p);
	        error = copyout((caddr_t)&ibcs2_jwinsize, uap->data,
			       sizeof(ibcs2_jwinsize));
		break;
	     }

	/* keyboard and display ioctl's -- type 'K' */
	case IBCS2_KDGKBMODE:        /* get keyboard translation mode */
	        uap->cmd = KDGKBMODE;
/* printf("ioctl KDGKBMODE = %x\n", uap->cmd);*/
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDSKBMODE:        /* set keyboard translation mode */
	        uap->cmd = KDSKBMODE;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDMKTONE:        /* sound tone */
	        uap->cmd = KDMKTONE;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDGETMODE:        /* get text/graphics mode */  
	        uap->cmd = KDGETMODE;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDSETMODE:       /* set text/graphics mode */
	        uap->cmd = KDSETMODE;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDSBORDER:       /* set ega color border */
	        uap->cmd = KDSBORDER;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDGKBSTATE:
	        uap->cmd = KDGKBSTATE;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDSETRAD:
	        uap->cmd = KDSETRAD;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDENABIO:       /* enable direct I/O to ports */
	        uap->cmd = KDENABIO;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDDISABIO:       /* disable direct I/O to ports */
	        uap->cmd = KDDISABIO;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KIOCSOUND:       /* start sound generation */
	        uap->cmd = KIOCSOUND;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDGKBTYPE:       /* get keyboard type */
	        uap->cmd = KDGKBTYPE;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDGETLED:       /* get keyboard LED status */
	        uap->cmd = KDGETLED;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_KDSETLED:       /* set keyboard LED status */
	        uap->cmd = KDSETLED;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	    /* Xenix keyboard and display ioctl's from sys/kd.h -- type 'k' */
	case IBCS2_GETFKEY:      /* Get function key */
	        uap->cmd = GETFKEY;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_SETFKEY:      /* Set function key */
	        uap->cmd = SETFKEY;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_GIO_SCRNMAP:      /* Get screen output map table */
	        uap->cmd = GIO_SCRNMAP;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_PIO_SCRNMAP:      /* Set screen output map table */
	        uap->cmd = PIO_SCRNMAP;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_GIO_KEYMAP:      /* Get keyboard map table */
	        uap->cmd = OGIO_KEYMAP;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	case IBCS2_PIO_KEYMAP:      /* Set keyboard map table */
	        uap->cmd = OPIO_KEYMAP;
	        error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	    /* socksys */
	case IBCS2_SIOCSOCKSYS:
		error = ibcs2_socksys(td, (struct ibcs2_socksys_args *)uap);
		break;

	case IBCS2_FIONREAD:
	case IBCS2_I_NREAD:     /* STREAMS */
	        uap->cmd = FIONREAD;
		error = sys_ioctl(td, (struct ioctl_args *)uap);
		break;

	default:
		DPRINTF(("ibcs2_ioctl(%d): unknown cmd 0x%lx ",
			 td->proc->p_pid, uap->cmd));
		error = ENOSYS;
		break;
	}

	fdrop(fp, td);
	return error;
}
