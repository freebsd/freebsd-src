/*	$NetBSD: ibcs2_ioctl.c,v 1.6 1995/03/14 15:12:28 scottb Exp $	*/

/*
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/dir.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/tty.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/unistd.h>

#include <net/if.h>
#include <sys/syscallargs.h>

#include <compat/ibcs2/ibcs2_types.h>
#include <compat/ibcs2/ibcs2_signal.h>
#include <compat/ibcs2/ibcs2_socksys.h>
#include <compat/ibcs2/ibcs2_stropts.h>
#include <compat/ibcs2/ibcs2_syscallargs.h>
#include <compat/ibcs2/ibcs2_termios.h>
#include <compat/ibcs2/ibcs2_util.h>

/*
 * iBCS2 ioctl calls.
 */

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
	if (l & IBCS2_TAB3)	r |= OXTABS;
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

	bt->c_ispeed = bt->c_ospeed = s2btab[l & 0x0000000f];

	bt->c_cc[VINTR]	=
	    st->c_cc[IBCS2_VINTR]  ? st->c_cc[IBCS2_VINTR]  : _POSIX_VDISABLE;
	bt->c_cc[VQUIT] =
	    st->c_cc[IBCS2_VQUIT]  ? st->c_cc[IBCS2_VQUIT]  : _POSIX_VDISABLE;
	bt->c_cc[VERASE] =
	    st->c_cc[IBCS2_VERASE] ? st->c_cc[IBCS2_VERASE] : _POSIX_VDISABLE;
	bt->c_cc[VKILL] =
	    st->c_cc[IBCS2_VKILL]  ? st->c_cc[IBCS2_VKILL]  : _POSIX_VDISABLE;
	bt->c_cc[VEOF] =
	    st->c_cc[IBCS2_VEOF]   ? st->c_cc[IBCS2_VEOF]   : _POSIX_VDISABLE;
	bt->c_cc[VEOL] =
	    st->c_cc[IBCS2_VEOL]   ? st->c_cc[IBCS2_VEOL]   : _POSIX_VDISABLE;
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
	if (l & OXTABS)		r |= IBCS2_TAB3;
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
	if (l >= 0)
		st->c_cflag |= l;

	st->c_cc[IBCS2_VINTR] =
	    bt->c_cc[VINTR]  != _POSIX_VDISABLE ? bt->c_cc[VINTR]  : 0;
	st->c_cc[IBCS2_VQUIT] =
	    bt->c_cc[VQUIT]  != _POSIX_VDISABLE ? bt->c_cc[VQUIT]  : 0;
	st->c_cc[IBCS2_VERASE] =
	    bt->c_cc[VERASE] != _POSIX_VDISABLE ? bt->c_cc[VERASE] : 0;
	st->c_cc[IBCS2_VKILL] =
	    bt->c_cc[VKILL]  != _POSIX_VDISABLE ? bt->c_cc[VKILL]  : 0;
	st->c_cc[IBCS2_VEOF] =
	    bt->c_cc[VEOF]   != _POSIX_VDISABLE ? bt->c_cc[VEOF]   : 0;
	st->c_cc[IBCS2_VEOL] =
	    bt->c_cc[VEOL]   != _POSIX_VDISABLE ? bt->c_cc[VEOL]   : 0;
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
ibcs2_ioctl(p, uap, retval)
	struct proc *p;
	struct ibcs2_ioctl_args *uap;
	int *retval;
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	int (*ctl)();
	int error;

	if (SCARG(uap, fd) < 0 || SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL) {
		DPRINTF(("ibcs2_ioctl(%d): bad fd %d ", p->p_pid,
			 SCARG(uap, fd)));
		return EBADF;
	}

	if ((fp->f_flag & (FREAD|FWRITE)) == 0) {
		DPRINTF(("ibcs2_ioctl(%d): bad fp flag ", p->p_pid));
		return EBADF;
	}

	ctl = fp->f_ops->fo_ioctl;

	switch (SCARG(uap, cmd)) {
	case IBCS2_TCGETA:
	case IBCS2_XCGETA:
	case IBCS2_OXCGETA:
	    {
		struct termios bts;
		struct ibcs2_termios sts;
		struct ibcs2_termio st;
	
		if ((error = (*ctl)(fp, TIOCGETA, (caddr_t)&bts, p)) != 0)
			return error;
	
		btios2stios (&bts, &sts);
		if (SCARG(uap, cmd) == IBCS2_TCGETA) {
			stios2stio (&sts, &st);
			error = copyout((caddr_t)&st, SCARG(uap, data),
					sizeof (st));
			if (error)
				DPRINTF(("ibcs2_ioctl(%d): copyout failed ",
					 p->p_pid));
			return error;
		} else
			return copyout((caddr_t)&sts, SCARG(uap, data),
					sizeof (sts));
		/*NOTREACHED*/
	    }

	case IBCS2_TCSETA:
	case IBCS2_TCSETAW:
	case IBCS2_TCSETAF:
	    {
		struct termios bts;
		struct ibcs2_termios sts;
		struct ibcs2_termio st;

		if ((error = copyin(SCARG(uap, data), (caddr_t)&st,
				    sizeof(st))) != 0) {
			DPRINTF(("ibcs2_ioctl(%d): TCSET copyin failed ",
				 p->p_pid));
			return error;
		}

		/* get full BSD termios so we don't lose information */
		if ((error = (*ctl)(fp, TIOCGETA, (caddr_t)&bts, p)) != 0) {
			DPRINTF(("ibcs2_ioctl(%d): TCSET ctl failed fd %d ",
				 p->p_pid, SCARG(uap, fd)));
			return error;
		}

		/*
		 * convert to iBCS2 termios, copy in information from
		 * termio, and convert back, then set new values.
		 */
		btios2stios(&bts, &sts);
		stio2stios(&st, &sts);
		stios2btios(&sts, &bts);

		return (*ctl)(fp, SCARG(uap, cmd) - IBCS2_TCSETA + TIOCSETA,
			      (caddr_t)&bts, p);
	    }

	case IBCS2_XCSETA:
	case IBCS2_XCSETAW:
	case IBCS2_XCSETAF:
	    {
		struct termios bts;
		struct ibcs2_termios sts;

		if ((error = copyin(SCARG(uap, data), (caddr_t)&sts,
				    sizeof (sts))) != 0) {
			return error;
		}
		stios2btios (&sts, &bts);
		return (*ctl)(fp, SCARG(uap, cmd) - IBCS2_XCSETA + TIOCSETA,
			      (caddr_t)&bts, p);
	    }

	case IBCS2_OXCSETA:
	case IBCS2_OXCSETAW:
	case IBCS2_OXCSETAF:
	    {
		struct termios bts;
		struct ibcs2_termios sts;

		if ((error = copyin(SCARG(uap, data), (caddr_t)&sts,
				    sizeof (sts))) != 0) {
			return error;
		}
		stios2btios (&sts, &bts);
		return (*ctl)(fp, SCARG(uap, cmd) - IBCS2_OXCSETA + TIOCSETA,
			      (caddr_t)&bts, p);
	    }

	case IBCS2_TCSBRK:
		DPRINTF(("ibcs2_ioctl(%d): TCSBRK ", p->p_pid));
		return ENOSYS;

	case IBCS2_TCXONC:
		DPRINTF(("ibcs2_ioctl(%d): TCXONC ", p->p_pid));
		return ENOSYS;

	case IBCS2_TCFLSH:
		DPRINTF(("ibcs2_ioctl(%d): TCFLSH ", p->p_pid));
		return ENOSYS;

	case IBCS2_TIOCGWINSZ:
		SCARG(uap, cmd) = TIOCGWINSZ;
		return ioctl(p, uap, retval);

	case IBCS2_TIOCSWINSZ:
		SCARG(uap, cmd) = TIOCSWINSZ;
		return ioctl(p, uap, retval);

	case IBCS2_TIOCGPGRP:
		return copyout((caddr_t)&p->p_pgrp->pg_id, SCARG(uap, data),
				sizeof(p->p_pgrp->pg_id));

	case IBCS2_TIOCSPGRP:	/* XXX - is uap->data a pointer to pgid? */
	    {
		struct setpgid_args sa;

		SCARG(&sa, pid) = 0;
		SCARG(&sa, pgid) = (int)SCARG(uap, data);
		if (error = setpgid(p, &sa, retval))
			return error;
		return 0;
	    }

	case IBCS2_TCGETSC:	/* SCO console - get scancode flags */
		return ENOSYS;

	case IBCS2_TCSETSC:	/* SCO console - set scancode flags */
		return ENOSYS;

	case IBCS2_SIOCSOCKSYS:
		return ibcs2_socksys(p, uap, retval);

	case IBCS2_I_NREAD:     /* STREAMS */
	        SCARG(uap, cmd) = FIONREAD;
		return ioctl(p, uap, retval);

	default:
		DPRINTF(("ibcs2_ioctl(%d): unknown cmd 0x%lx ",
			 p->p_pid, SCARG(uap, cmd)));
		return ENOSYS;
	}
	return ENOSYS;
}

