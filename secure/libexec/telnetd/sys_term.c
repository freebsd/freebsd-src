/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef lint
static char sccsid[] = "@(#)sys_term.c	8.2 (Berkeley) 12/15/93";
#endif /* not lint */

#include "telnetd.h"
#include "pathnames.h"

#if	defined(AUTHENTICATION)
#include <libtelnet/auth.h>
#endif

#if defined(CRAY) || defined(__hpux)
# define PARENT_DOES_UTMP
#endif

#ifdef	NEWINIT
#include <initreq.h>
int	utmp_len = MAXHOSTNAMELEN;	/* sizeof(init_request.host) */
#else	/* NEWINIT*/
# ifdef	UTMPX
# include <utmpx.h>
struct	utmpx wtmp;
# else
# include <utmp.h>
struct	utmp wtmp;
# endif /* UTMPX */

int	utmp_len = sizeof(wtmp.ut_host);
# ifndef PARENT_DOES_UTMP
char	wtmpf[]	= "/usr/adm/wtmp";
char	utmpf[] = "/etc/utmp";
# else /* PARENT_DOES_UTMP */
char	wtmpf[]	= "/etc/wtmp";
# endif /* PARENT_DOES_UTMP */

# ifdef CRAY
#include <tmpdir.h>
#include <sys/wait.h>
#  if defined(_SC_CRAY_SECURE_SYS) && !defined(SCM_SECURITY)
   /*
    * UNICOS 6.0/6.1 do not have SCM_SECURITY defined, so we can
    * use it to tell us to turn off all the socket security code,
    * since that is only used in UNICOS 7.0 and later.
    */
#   undef _SC_CRAY_SECURE_SYS
#  endif

#  if defined(_SC_CRAY_SECURE_SYS)
#include <sys/sysv.h>
#include <sys/secstat.h>
extern int secflag;
extern struct sysv sysv;
#  endif /* _SC_CRAY_SECURE_SYS */
# endif	/* CRAY */
#endif	/* NEWINIT */

#ifdef	STREAMSPTY
#include <sac.h>
#include <sys/stropts.h>
#endif

#define SCPYN(a, b)	(void) strncpy(a, b, sizeof(a))
#define SCMPN(a, b)	strncmp(a, b, sizeof(a))

#ifdef	STREAMS
#include <sys/stream.h>
#endif
#ifdef __hpux
#include <sys/resource.h>
#include <sys/proc.h>
#endif
#include <sys/tty.h>
#ifdef	t_erase
#undef	t_erase
#undef	t_kill
#undef	t_intrc
#undef	t_quitc
#undef	t_startc
#undef	t_stopc
#undef	t_eofc
#undef	t_brkc
#undef	t_suspc
#undef	t_dsuspc
#undef	t_rprntc
#undef	t_flushc
#undef	t_werasc
#undef	t_lnextc
#endif

#if defined(UNICOS5) && defined(CRAY2) && !defined(EXTPROC)
# define EXTPROC 0400
#endif

#ifndef	USE_TERMIO
struct termbuf {
	struct sgttyb sg;
	struct tchars tc;
	struct ltchars ltc;
	int state;
	int lflags;
} termbuf, termbuf2;
# define	cfsetospeed(tp, val)	(tp)->sg.sg_ospeed = (val)
# define	cfsetispeed(tp, val)	(tp)->sg.sg_ispeed = (val)
# define	cfgetospeed(tp)		(tp)->sg.sg_ospeed
# define	cfgetispeed(tp)		(tp)->sg.sg_ispeed
#else	/* USE_TERMIO */
# ifdef	SYSV_TERMIO
#	define termios termio
# endif
# ifndef	TCSANOW
#  ifdef TCSETS
#   define	TCSANOW		TCSETS
#   define	TCSADRAIN	TCSETSW
#   define	tcgetattr(f, t)	ioctl(f, TCGETS, (char *)t)
#  else
#   ifdef TCSETA
#    define	TCSANOW		TCSETA
#    define	TCSADRAIN	TCSETAW
#    define	tcgetattr(f, t)	ioctl(f, TCGETA, (char *)t)
#   else
#    define	TCSANOW		TIOCSETA
#    define	TCSADRAIN	TIOCSETAW
#    define	tcgetattr(f, t)	ioctl(f, TIOCGETA, (char *)t)
#   endif
#  endif
#  define	tcsetattr(f, a, t)	ioctl(f, a, t)
#  define	cfsetospeed(tp, val)	(tp)->c_cflag &= ~CBAUD; \
					(tp)->c_cflag |= (val)
#  define	cfgetospeed(tp)		((tp)->c_cflag & CBAUD)
#  ifdef CIBAUD
#   define	cfsetispeed(tp, val)	(tp)->c_cflag &= ~CIBAUD; \
					(tp)->c_cflag |= ((val)<<IBSHIFT)
#   define	cfgetispeed(tp)		(((tp)->c_cflag & CIBAUD)>>IBSHIFT)
#  else
#   define	cfsetispeed(tp, val)	(tp)->c_cflag &= ~CBAUD; \
					(tp)->c_cflag |= (val)
#   define	cfgetispeed(tp)		((tp)->c_cflag & CBAUD)
#  endif
# endif /* TCSANOW */
struct termios termbuf, termbuf2;	/* pty control structure */
# ifdef  STREAMSPTY
int ttyfd = -1;
# endif
#endif	/* USE_TERMIO */

/*
 * init_termbuf()
 * copy_termbuf(cp)
 * set_termbuf()
 *
 * These three routines are used to get and set the "termbuf" structure
 * to and from the kernel.  init_termbuf() gets the current settings.
 * copy_termbuf() hands in a new "termbuf" to write to the kernel, and
 * set_termbuf() writes the structure into the kernel.
 */

	void
init_termbuf()
{
#ifndef	USE_TERMIO
	(void) ioctl(pty, TIOCGETP, (char *)&termbuf.sg);
	(void) ioctl(pty, TIOCGETC, (char *)&termbuf.tc);
	(void) ioctl(pty, TIOCGLTC, (char *)&termbuf.ltc);
# ifdef	TIOCGSTATE
	(void) ioctl(pty, TIOCGSTATE, (char *)&termbuf.state);
# endif
#else
# ifdef  STREAMSPTY
	(void) tcgetattr(ttyfd, &termbuf);
# else
	(void) tcgetattr(pty, &termbuf);
# endif
#endif
	termbuf2 = termbuf;
}

#if	defined(LINEMODE) && defined(TIOCPKT_IOCTL)
	void
copy_termbuf(cp, len)
	char *cp;
	int len;
{
	if (len > sizeof(termbuf))
		len = sizeof(termbuf);
	bcopy(cp, (char *)&termbuf, len);
	termbuf2 = termbuf;
}
#endif	/* defined(LINEMODE) && defined(TIOCPKT_IOCTL) */

	void
set_termbuf()
{
	/*
	 * Only make the necessary changes.
	 */
#ifndef	USE_TERMIO
	if (bcmp((char *)&termbuf.sg, (char *)&termbuf2.sg, sizeof(termbuf.sg)))
		(void) ioctl(pty, TIOCSETN, (char *)&termbuf.sg);
	if (bcmp((char *)&termbuf.tc, (char *)&termbuf2.tc, sizeof(termbuf.tc)))
		(void) ioctl(pty, TIOCSETC, (char *)&termbuf.tc);
	if (bcmp((char *)&termbuf.ltc, (char *)&termbuf2.ltc,
							sizeof(termbuf.ltc)))
		(void) ioctl(pty, TIOCSLTC, (char *)&termbuf.ltc);
	if (termbuf.lflags != termbuf2.lflags)
		(void) ioctl(pty, TIOCLSET, (char *)&termbuf.lflags);
#else	/* USE_TERMIO */
	if (bcmp((char *)&termbuf, (char *)&termbuf2, sizeof(termbuf)))
# ifdef  STREAMSPTY
		(void) tcsetattr(ttyfd, TCSANOW, &termbuf);
# else
		(void) tcsetattr(pty, TCSANOW, &termbuf);
# endif
# if	defined(CRAY2) && defined(UNICOS5)
	needtermstat = 1;
# endif
#endif	/* USE_TERMIO */
}


/*
 * spcset(func, valp, valpp)
 *
 * This function takes various special characters (func), and
 * sets *valp to the current value of that character, and
 * *valpp to point to where in the "termbuf" structure that
 * value is kept.
 *
 * It returns the SLC_ level of support for this function.
 */

#ifndef	USE_TERMIO
	int
spcset(func, valp, valpp)
	int func;
	cc_t *valp;
	cc_t **valpp;
{
	switch(func) {
	case SLC_EOF:
		*valp = termbuf.tc.t_eofc;
		*valpp = (cc_t *)&termbuf.tc.t_eofc;
		return(SLC_VARIABLE);
	case SLC_EC:
		*valp = termbuf.sg.sg_erase;
		*valpp = (cc_t *)&termbuf.sg.sg_erase;
		return(SLC_VARIABLE);
	case SLC_EL:
		*valp = termbuf.sg.sg_kill;
		*valpp = (cc_t *)&termbuf.sg.sg_kill;
		return(SLC_VARIABLE);
	case SLC_IP:
		*valp = termbuf.tc.t_intrc;
		*valpp = (cc_t *)&termbuf.tc.t_intrc;
		return(SLC_VARIABLE|SLC_FLUSHIN|SLC_FLUSHOUT);
	case SLC_ABORT:
		*valp = termbuf.tc.t_quitc;
		*valpp = (cc_t *)&termbuf.tc.t_quitc;
		return(SLC_VARIABLE|SLC_FLUSHIN|SLC_FLUSHOUT);
	case SLC_XON:
		*valp = termbuf.tc.t_startc;
		*valpp = (cc_t *)&termbuf.tc.t_startc;
		return(SLC_VARIABLE);
	case SLC_XOFF:
		*valp = termbuf.tc.t_stopc;
		*valpp = (cc_t *)&termbuf.tc.t_stopc;
		return(SLC_VARIABLE);
	case SLC_AO:
		*valp = termbuf.ltc.t_flushc;
		*valpp = (cc_t *)&termbuf.ltc.t_flushc;
		return(SLC_VARIABLE);
	case SLC_SUSP:
		*valp = termbuf.ltc.t_suspc;
		*valpp = (cc_t *)&termbuf.ltc.t_suspc;
		return(SLC_VARIABLE);
	case SLC_EW:
		*valp = termbuf.ltc.t_werasc;
		*valpp = (cc_t *)&termbuf.ltc.t_werasc;
		return(SLC_VARIABLE);
	case SLC_RP:
		*valp = termbuf.ltc.t_rprntc;
		*valpp = (cc_t *)&termbuf.ltc.t_rprntc;
		return(SLC_VARIABLE);
	case SLC_LNEXT:
		*valp = termbuf.ltc.t_lnextc;
		*valpp = (cc_t *)&termbuf.ltc.t_lnextc;
		return(SLC_VARIABLE);
	case SLC_FORW1:
		*valp = termbuf.tc.t_brkc;
		*valpp = (cc_t *)&termbuf.ltc.t_lnextc;
		return(SLC_VARIABLE);
	case SLC_BRK:
	case SLC_SYNCH:
	case SLC_AYT:
	case SLC_EOR:
		*valp = (cc_t)0;
		*valpp = (cc_t *)0;
		return(SLC_DEFAULT);
	default:
		*valp = (cc_t)0;
		*valpp = (cc_t *)0;
		return(SLC_NOSUPPORT);
	}
}

#else	/* USE_TERMIO */

	int
spcset(func, valp, valpp)
	int func;
	cc_t *valp;
	cc_t **valpp;
{

#define	setval(a, b)	*valp = termbuf.c_cc[a]; \
			*valpp = &termbuf.c_cc[a]; \
			return(b);
#define	defval(a) *valp = ((cc_t)a); *valpp = (cc_t *)0; return(SLC_DEFAULT);

	switch(func) {
	case SLC_EOF:
		setval(VEOF, SLC_VARIABLE);
	case SLC_EC:
		setval(VERASE, SLC_VARIABLE);
	case SLC_EL:
		setval(VKILL, SLC_VARIABLE);
	case SLC_IP:
		setval(VINTR, SLC_VARIABLE|SLC_FLUSHIN|SLC_FLUSHOUT);
	case SLC_ABORT:
		setval(VQUIT, SLC_VARIABLE|SLC_FLUSHIN|SLC_FLUSHOUT);
	case SLC_XON:
#ifdef	VSTART
		setval(VSTART, SLC_VARIABLE);
#else
		defval(0x13);
#endif
	case SLC_XOFF:
#ifdef	VSTOP
		setval(VSTOP, SLC_VARIABLE);
#else
		defval(0x11);
#endif
	case SLC_EW:
#ifdef	VWERASE
		setval(VWERASE, SLC_VARIABLE);
#else
		defval(0);
#endif
	case SLC_RP:
#ifdef	VREPRINT
		setval(VREPRINT, SLC_VARIABLE);
#else
		defval(0);
#endif
	case SLC_LNEXT:
#ifdef	VLNEXT
		setval(VLNEXT, SLC_VARIABLE);
#else
		defval(0);
#endif
	case SLC_AO:
#if	!defined(VDISCARD) && defined(VFLUSHO)
# define VDISCARD VFLUSHO
#endif
#ifdef	VDISCARD
		setval(VDISCARD, SLC_VARIABLE|SLC_FLUSHOUT);
#else
		defval(0);
#endif
	case SLC_SUSP:
#ifdef	VSUSP
		setval(VSUSP, SLC_VARIABLE|SLC_FLUSHIN);
#else
		defval(0);
#endif
#ifdef	VEOL
	case SLC_FORW1:
		setval(VEOL, SLC_VARIABLE);
#endif
#ifdef	VEOL2
	case SLC_FORW2:
		setval(VEOL2, SLC_VARIABLE);
#endif
	case SLC_AYT:
#ifdef	VSTATUS
		setval(VSTATUS, SLC_VARIABLE);
#else
		defval(0);
#endif

	case SLC_BRK:
	case SLC_SYNCH:
	case SLC_EOR:
		defval(0);

	default:
		*valp = 0;
		*valpp = 0;
		return(SLC_NOSUPPORT);
	}
}
#endif	/* USE_TERMIO */

#ifdef CRAY
/*
 * getnpty()
 *
 * Return the number of pty's configured into the system.
 */
	int
getnpty()
{
#ifdef _SC_CRAY_NPTY
	int numptys;

	if ((numptys = sysconf(_SC_CRAY_NPTY)) != -1)
		return numptys;
	else
#endif /* _SC_CRAY_NPTY */
		return 128;
}
#endif /* CRAY */

#ifndef	convex
/*
 * getpty()
 *
 * Allocate a pty.  As a side effect, the external character
 * array "line" contains the name of the slave side.
 *
 * Returns the file descriptor of the opened pty.
 */
#ifndef	__GNUC__
char *line = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
#else
static char Xline[] = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
char *line = Xline;
#endif
#ifdef	CRAY
char *myline = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
#endif	/* CRAY */

	int
getpty(ptynum)
int *ptynum;
{
	register int p;
#ifdef	STREAMSPTY
	int t;
	char *ptsname();

	p = open("/dev/ptmx", 2);
	if (p > 0) {
		grantpt(p);
		unlockpt(p);
		strcpy(line, ptsname(p));
		return(p);
	}

#else	/* ! STREAMSPTY */
#ifndef CRAY
	register char *cp, *p1, *p2;
	register int i;
#if defined(sun) && defined(TIOCGPGRP) && BSD < 199207
	int dummy;
#endif

#ifndef	__hpux
	(void) sprintf(line, "/dev/ptyXX");
	p1 = &line[8];
	p2 = &line[9];
#else
	(void) sprintf(line, "/dev/ptym/ptyXX");
	p1 = &line[13];
	p2 = &line[14];
#endif

	for (cp = "pqrstuvwxyzPQRST"; *cp; cp++) {
		struct stat stb;

		*p1 = *cp;
		*p2 = '0';
		/*
		 * This stat() check is just to keep us from
		 * looping through all 256 combinations if there
		 * aren't that many ptys available.
		 */
		if (stat(line, &stb) < 0)
			break;
		for (i = 0; i < 16; i++) {
			*p2 = "0123456789abcdef"[i];
			p = open(line, 2);
			if (p > 0) {
#ifndef	__hpux
				line[5] = 't';
#else
				for (p1 = &line[8]; *p1; p1++)
					*p1 = *(p1+1);
				line[9] = 't';
#endif
				chown(line, 0, 0);
				chmod(line, 0600);
#if defined(sun) && defined(TIOCGPGRP) && BSD < 199207
				if (ioctl(p, TIOCGPGRP, &dummy) == 0
				    || errno != EIO) {
					chmod(line, 0666);
					close(p);
					line[5] = 'p';
				} else
#endif /* defined(sun) && defined(TIOCGPGRP) && BSD < 199207 */
					return(p);
			}
		}
	}
#else	/* CRAY */
	extern lowpty, highpty;
	struct stat sb;

	for (*ptynum = lowpty; *ptynum <= highpty; (*ptynum)++) {
		(void) sprintf(myline, "/dev/pty/%03d", *ptynum);
		p = open(myline, 2);
		if (p < 0)
			continue;
		(void) sprintf(line, "/dev/ttyp%03d", *ptynum);
		/*
		 * Here are some shenanigans to make sure that there
		 * are no listeners lurking on the line.
		 */
		if(stat(line, &sb) < 0) {
			(void) close(p);
			continue;
		}
		if(sb.st_uid || sb.st_gid || sb.st_mode != 0600) {
			chown(line, 0, 0);
			chmod(line, 0600);
			(void)close(p);
			p = open(myline, 2);
			if (p < 0)
				continue;
		}
		/*
		 * Now it should be safe...check for accessability.
		 */
		if (access(line, 6) == 0)
			return(p);
		else {
			/* no tty side to pty so skip it */
			(void) close(p);
		}
	}
#endif	/* CRAY */
#endif	/* STREAMSPTY */
	return(-1);
}
#endif	/* convex */

#ifdef	LINEMODE
/*
 * tty_flowmode()	Find out if flow control is enabled or disabled.
 * tty_linemode()	Find out if linemode (external processing) is enabled.
 * tty_setlinemod(on)	Turn on/off linemode.
 * tty_isecho()		Find out if echoing is turned on.
 * tty_setecho(on)	Enable/disable character echoing.
 * tty_israw()		Find out if terminal is in RAW mode.
 * tty_binaryin(on)	Turn on/off BINARY on input.
 * tty_binaryout(on)	Turn on/off BINARY on output.
 * tty_isediting()	Find out if line editing is enabled.
 * tty_istrapsig()	Find out if signal trapping is enabled.
 * tty_setedit(on)	Turn on/off line editing.
 * tty_setsig(on)	Turn on/off signal trapping.
 * tty_issofttab()	Find out if tab expansion is enabled.
 * tty_setsofttab(on)	Turn on/off soft tab expansion.
 * tty_islitecho()	Find out if typed control chars are echoed literally
 * tty_setlitecho()	Turn on/off literal echo of control chars
 * tty_tspeed(val)	Set transmit speed to val.
 * tty_rspeed(val)	Set receive speed to val.
 */

#ifdef convex
static int linestate;
#endif

	int
tty_linemode()
{
#ifndef convex
#ifndef	USE_TERMIO
	return(termbuf.state & TS_EXTPROC);
#else
	return(termbuf.c_lflag & EXTPROC);
#endif
#else
	return(linestate);
#endif
}

	void
tty_setlinemode(on)
	int on;
{
#ifdef	TIOCEXT
# ifndef convex
	set_termbuf();
# else
	linestate = on;
# endif
	(void) ioctl(pty, TIOCEXT, (char *)&on);
# ifndef convex
	init_termbuf();
# endif
#else	/* !TIOCEXT */
# ifdef	EXTPROC
	if (on)
		termbuf.c_lflag |= EXTPROC;
	else
		termbuf.c_lflag &= ~EXTPROC;
# endif
#endif	/* TIOCEXT */
}
#endif	/* LINEMODE */

	int
tty_isecho()
{
#ifndef USE_TERMIO
	return (termbuf.sg.sg_flags & ECHO);
#else
	return (termbuf.c_lflag & ECHO);
#endif
}

	int
tty_flowmode()
{
#ifndef USE_TERMIO
	return(((termbuf.tc.t_startc) > 0 && (termbuf.tc.t_stopc) > 0) ? 1 : 0);
#else
	return((termbuf.c_iflag & IXON) ? 1 : 0);
#endif
}

	int
tty_restartany()
{
#ifndef USE_TERMIO
# ifdef	DECCTQ
	return((termbuf.lflags & DECCTQ) ? 0 : 1);
# else
	return(-1);
# endif
#else
	return((termbuf.c_iflag & IXANY) ? 1 : 0);
#endif
}

	void
tty_setecho(on)
	int on;
{
#ifndef	USE_TERMIO
	if (on)
		termbuf.sg.sg_flags |= ECHO|CRMOD;
	else
		termbuf.sg.sg_flags &= ~(ECHO|CRMOD);
#else
	if (on)
		termbuf.c_lflag |= ECHO;
	else
		termbuf.c_lflag &= ~ECHO;
#endif
}

	int
tty_israw()
{
#ifndef USE_TERMIO
	return(termbuf.sg.sg_flags & RAW);
#else
	return(!(termbuf.c_lflag & ICANON));
#endif
}

#if	defined (AUTHENTICATION) && defined(NO_LOGIN_F) && defined(LOGIN_R)
	int
tty_setraw(on)
{
#  ifndef USE_TERMIO
	if (on)
		termbuf.sg.sg_flags |= RAW;
	else
		termbuf.sg.sg_flags &= ~RAW;
#  else
	if (on)
		termbuf.c_lflag &= ~ICANON;
	else
		termbuf.c_lflag |= ICANON;
#  endif
}
#endif

	void
tty_binaryin(on)
	int on;
{
#ifndef	USE_TERMIO
	if (on)
		termbuf.lflags |= LPASS8;
	else
		termbuf.lflags &= ~LPASS8;
#else
	if (on) {
		termbuf.c_iflag &= ~ISTRIP;
	} else {
		termbuf.c_iflag |= ISTRIP;
	}
#endif
}

	void
tty_binaryout(on)
	int on;
{
#ifndef	USE_TERMIO
	if (on)
		termbuf.lflags |= LLITOUT;
	else
		termbuf.lflags &= ~LLITOUT;
#else
	if (on) {
		termbuf.c_cflag &= ~(CSIZE|PARENB);
		termbuf.c_cflag |= CS8;
		termbuf.c_oflag &= ~OPOST;
	} else {
		termbuf.c_cflag &= ~CSIZE;
		termbuf.c_cflag |= CS7|PARENB;
		termbuf.c_oflag |= OPOST;
	}
#endif
}

	int
tty_isbinaryin()
{
#ifndef	USE_TERMIO
	return(termbuf.lflags & LPASS8);
#else
	return(!(termbuf.c_iflag & ISTRIP));
#endif
}

	int
tty_isbinaryout()
{
#ifndef	USE_TERMIO
	return(termbuf.lflags & LLITOUT);
#else
	return(!(termbuf.c_oflag&OPOST));
#endif
}

#ifdef	LINEMODE
	int
tty_isediting()
{
#ifndef USE_TERMIO
	return(!(termbuf.sg.sg_flags & (CBREAK|RAW)));
#else
	return(termbuf.c_lflag & ICANON);
#endif
}

	int
tty_istrapsig()
{
#ifndef USE_TERMIO
	return(!(termbuf.sg.sg_flags&RAW));
#else
	return(termbuf.c_lflag & ISIG);
#endif
}

	void
tty_setedit(on)
	int on;
{
#ifndef USE_TERMIO
	if (on)
		termbuf.sg.sg_flags &= ~CBREAK;
	else
		termbuf.sg.sg_flags |= CBREAK;
#else
	if (on)
		termbuf.c_lflag |= ICANON;
	else
		termbuf.c_lflag &= ~ICANON;
#endif
}

	void
tty_setsig(on)
	int on;
{
#ifndef	USE_TERMIO
	if (on)
		;
#else
	if (on)
		termbuf.c_lflag |= ISIG;
	else
		termbuf.c_lflag &= ~ISIG;
#endif
}
#endif	/* LINEMODE */

	int
tty_issofttab()
{
#ifndef	USE_TERMIO
	return (termbuf.sg.sg_flags & XTABS);
#else
# ifdef	OXTABS
	return (termbuf.c_oflag & OXTABS);
# endif
# ifdef	TABDLY
	return ((termbuf.c_oflag & TABDLY) == TAB3);
# endif
#endif
}

	void
tty_setsofttab(on)
	int on;
{
#ifndef	USE_TERMIO
	if (on)
		termbuf.sg.sg_flags |= XTABS;
	else
		termbuf.sg.sg_flags &= ~XTABS;
#else
	if (on) {
# ifdef	OXTABS
		termbuf.c_oflag |= OXTABS;
# endif
# ifdef	TABDLY
		termbuf.c_oflag &= ~TABDLY;
		termbuf.c_oflag |= TAB3;
# endif
	} else {
# ifdef	OXTABS
		termbuf.c_oflag &= ~OXTABS;
# endif
# ifdef	TABDLY
		termbuf.c_oflag &= ~TABDLY;
		termbuf.c_oflag |= TAB0;
# endif
	}
#endif
}

	int
tty_islitecho()
{
#ifndef	USE_TERMIO
	return (!(termbuf.lflags & LCTLECH));
#else
# ifdef	ECHOCTL
	return (!(termbuf.c_lflag & ECHOCTL));
# endif
# ifdef	TCTLECH
	return (!(termbuf.c_lflag & TCTLECH));
# endif
# if	!defined(ECHOCTL) && !defined(TCTLECH)
	return (0);	/* assumes ctl chars are echoed '^x' */
# endif
#endif
}

	void
tty_setlitecho(on)
	int on;
{
#ifndef	USE_TERMIO
	if (on)
		termbuf.lflags &= ~LCTLECH;
	else
		termbuf.lflags |= LCTLECH;
#else
# ifdef	ECHOCTL
	if (on)
		termbuf.c_lflag &= ~ECHOCTL;
	else
		termbuf.c_lflag |= ECHOCTL;
# endif
# ifdef	TCTLECH
	if (on)
		termbuf.c_lflag &= ~TCTLECH;
	else
		termbuf.c_lflag |= TCTLECH;
# endif
#endif
}

	int
tty_iscrnl()
{
#ifndef	USE_TERMIO
	return (termbuf.sg.sg_flags & CRMOD);
#else
	return (termbuf.c_iflag & ICRNL);
#endif
}

/*
 * A table of available terminal speeds
 */
struct termspeeds {
	int	speed;
	int	value;
} termspeeds[] = {
	{ 0,     B0 },    { 50,    B50 },   { 75,    B75 },
	{ 110,   B110 },  { 134,   B134 },  { 150,   B150 },
	{ 200,   B200 },  { 300,   B300 },  { 600,   B600 },
	{ 1200,  B1200 }, { 1800,  B1800 }, { 2400,  B2400 },
	{ 4800,  B4800 }, { 9600,  B9600 }, { 19200, B9600 },
	{ 38400, B9600 }, { -1,    B9600 }
};

	void
tty_tspeed(val)
	int val;
{
	register struct termspeeds *tp;

	for (tp = termspeeds; (tp->speed != -1) && (val > tp->speed); tp++)
		;
	cfsetospeed(&termbuf, tp->value);
}

	void
tty_rspeed(val)
	int val;
{
	register struct termspeeds *tp;

	for (tp = termspeeds; (tp->speed != -1) && (val > tp->speed); tp++)
		;
	cfsetispeed(&termbuf, tp->value);
}

#if	defined(CRAY2) && defined(UNICOS5)
	int
tty_isnewmap()
{
	return((termbuf.c_oflag & OPOST) && (termbuf.c_oflag & ONLCR) &&
			!(termbuf.c_oflag & ONLRET));
}
#endif

#ifdef PARENT_DOES_UTMP
# ifndef NEWINIT
extern	struct utmp wtmp;
extern char wtmpf[];
# else	/* NEWINIT */
int	gotalarm;

	/* ARGSUSED */
	void
nologinproc(sig)
	int sig;
{
	gotalarm++;
}
# endif	/* NEWINIT */
#endif /* PARENT_DOES_UTMP */

#ifndef	NEWINIT
# ifdef PARENT_DOES_UTMP
extern void utmp_sig_init P((void));
extern void utmp_sig_reset P((void));
extern void utmp_sig_wait P((void));
extern void utmp_sig_notify P((int));
# endif /* PARENT_DOES_UTMP */
#endif

/*
 * getptyslave()
 *
 * Open the slave side of the pty, and do any initialization
 * that is necessary.  The return value is a file descriptor
 * for the slave side.
 */
	int
getptyslave()
{
	register int t = -1;

#if	!defined(CRAY) || !defined(NEWINIT)
# ifdef	LINEMODE
	int waslm;
# endif
# ifdef	TIOCGWINSZ
	struct winsize ws;
	extern int def_row, def_col;
# endif
	extern int def_tspeed, def_rspeed;
	/*
	 * Opening the slave side may cause initilization of the
	 * kernel tty structure.  We need remember the state of
	 * 	if linemode was turned on
	 *	terminal window size
	 *	terminal speed
	 * so that we can re-set them if we need to.
	 */
# ifdef	LINEMODE
	waslm = tty_linemode();
# endif


	/*
	 * Make sure that we don't have a controlling tty, and
	 * that we are the session (process group) leader.
	 */
# ifdef	TIOCNOTTY
	t = open(_PATH_TTY, O_RDWR);
	if (t >= 0) {
		(void) ioctl(t, TIOCNOTTY, (char *)0);
		(void) close(t);
	}
# endif


# ifdef PARENT_DOES_UTMP
	/*
	 * Wait for our parent to get the utmp stuff to get done.
	 */
	utmp_sig_wait();
# endif

	t = cleanopen(line);
	if (t < 0)
		fatalperror(net, line);

#ifdef  STREAMSPTY
#ifdef	USE_TERMIO
	ttyfd = t;
#endif
	if (ioctl(t, I_PUSH, "ptem") < 0) 
		fatal(net, "I_PUSH ptem");
	if (ioctl(t, I_PUSH, "ldterm") < 0)
		fatal(net, "I_PUSH ldterm");
	if (ioctl(t, I_PUSH, "ttcompat") < 0)
		fatal(net, "I_PUSH ttcompat");
	if (ioctl(pty, I_PUSH, "pckt") < 0)
		fatal(net, "I_PUSH pckt");
#endif

	/*
	 * set up the tty modes as we like them to be.
	 */
	init_termbuf();
# ifdef	TIOCGWINSZ
	if (def_row || def_col) {
		bzero((char *)&ws, sizeof(ws));
		ws.ws_col = def_col;
		ws.ws_row = def_row;
		(void)ioctl(t, TIOCSWINSZ, (char *)&ws);
	}
# endif

	/*
	 * Settings for sgtty based systems
	 */
# ifndef	USE_TERMIO
	termbuf.sg.sg_flags |= CRMOD|ANYP|ECHO|XTABS;
# endif	/* USE_TERMIO */

	/*
	 * Settings for UNICOS (and HPUX)
	 */
# if defined(CRAY) || defined(__hpux)
	termbuf.c_oflag = OPOST|ONLCR|TAB3;
	termbuf.c_iflag = IGNPAR|ISTRIP|ICRNL|IXON;
	termbuf.c_lflag = ISIG|ICANON|ECHO|ECHOE|ECHOK;
	termbuf.c_cflag = EXTB|HUPCL|CS8;
# endif

	/*
	 * Settings for all other termios/termio based
	 * systems, other than 4.4BSD.  In 4.4BSD the
	 * kernel does the initial terminal setup.
	 */
# if defined(USE_TERMIO) && !(defined(CRAY) || defined(__hpux)) && (BSD <= 43)
#  ifndef	OXTABS
#   define OXTABS	0
#  endif
	termbuf.c_lflag |= ECHO;
	termbuf.c_oflag |= ONLCR|OXTABS;
	termbuf.c_iflag |= ICRNL;
	termbuf.c_iflag &= ~IXOFF;
# endif /* defined(USE_TERMIO) && !defined(CRAY) && (BSD <= 43) */
	tty_rspeed((def_rspeed > 0) ? def_rspeed : 9600);
	tty_tspeed((def_tspeed > 0) ? def_tspeed : 9600);
# ifdef	LINEMODE
	if (waslm)
		tty_setlinemode(1);
# endif	/* LINEMODE */

	/*
	 * Set the tty modes, and make this our controlling tty.
	 */
	set_termbuf();
	if (login_tty(t) == -1)
		fatalperror(net, "login_tty");
#endif	/* !defined(CRAY) || !defined(NEWINIT) */
	if (net > 2)
		(void) close(net);
#if	defined(AUTHENTICATION) && defined(NO_LOGIN_F) && defined(LOGIN_R)
	/*
	 * Leave the pty open so that we can write out the rlogin
	 * protocol for /bin/login, if the authentication works.
	 */
#else
	if (pty > 2) {
		(void) close(pty);
		pty = -1;
	}
#endif
}

#if	!defined(CRAY) || !defined(NEWINIT)
#ifndef	O_NOCTTY
#define	O_NOCTTY	0
#endif
/*
 * Open the specified slave side of the pty,
 * making sure that we have a clean tty.
 */
	int
cleanopen(line)
	char *line;
{
	register int t;
#if	defined(_SC_CRAY_SECURE_SYS)
	struct secstat secbuf;
#endif	/* _SC_CRAY_SECURE_SYS */

#ifndef STREAMSPTY
	/*
	 * Make sure that other people can't open the
	 * slave side of the connection.
	 */
	(void) chown(line, 0, 0);
	(void) chmod(line, 0600);
#endif

# if !defined(CRAY) && (BSD > 43)
	(void) revoke(line);
# endif
#if	defined(_SC_CRAY_SECURE_SYS)
	if (secflag) {
		if (secstat(line, &secbuf) < 0)
			return(-1);
		if (setulvl(secbuf.st_slevel) < 0)
			return(-1);
		if (setucmp(secbuf.st_compart) < 0)
			return(-1);
	}
#endif	/* _SC_CRAY_SECURE_SYS */

	t = open(line, O_RDWR|O_NOCTTY);

#if	defined(_SC_CRAY_SECURE_SYS)
	if (secflag) {
		if (setulvl(sysv.sy_minlvl) < 0)
			return(-1);
		if (setucmp(0) < 0)
			return(-1);
	}
#endif	/* _SC_CRAY_SECURE_SYS */

	if (t < 0)
		return(-1);

	/*
	 * Hangup anybody else using this ttyp, then reopen it for
	 * ourselves.
	 */
# if !(defined(CRAY) || defined(__hpux)) && (BSD <= 43) && !defined(STREAMSPTY)
	(void) signal(SIGHUP, SIG_IGN);
	vhangup();
	(void) signal(SIGHUP, SIG_DFL);
	t = open(line, O_RDWR|O_NOCTTY);
	if (t < 0)
		return(-1);
# endif
# if	defined(CRAY) && defined(TCVHUP)
	{
		register int i;
		(void) signal(SIGHUP, SIG_IGN);
		(void) ioctl(t, TCVHUP, (char *)0);
		(void) signal(SIGHUP, SIG_DFL);
		setpgrp();

#if		defined(_SC_CRAY_SECURE_SYS)
		if (secflag) {
			if (secstat(line, &secbuf) < 0)
				return(-1);
			if (setulvl(secbuf.st_slevel) < 0)
				return(-1);
			if (setucmp(secbuf.st_compart) < 0)
				return(-1);
		}
#endif		/* _SC_CRAY_SECURE_SYS */

		i = open(line, O_RDWR);

#if		defined(_SC_CRAY_SECURE_SYS)
		if (secflag) {
			if (setulvl(sysv.sy_minlvl) < 0)
				return(-1);
			if (setucmp(0) < 0)
				return(-1);
		}
#endif		/* _SC_CRAY_SECURE_SYS */

		if (i < 0)
			return(-1);
		(void) close(t);
		t = i;
	}
# endif	/* defined(CRAY) && defined(TCVHUP) */
	return(t);
}
#endif	/* !defined(CRAY) || !defined(NEWINIT) */

#if BSD <= 43

	int
login_tty(t)
	int t;
{
	if (setsid() < 0) {
#ifdef ultrix
		/*
		 * The setsid() may have failed because we
		 * already have a pgrp == pid.  Zero out
		 * our pgrp and try again...
		 */
		if ((setpgrp(0, 0) < 0) || (setsid() < 0))
#endif
			fatalperror(net, "setsid()");
	}
# ifdef	TIOCSCTTY
	if (ioctl(t, TIOCSCTTY, (char *)0) < 0)
		fatalperror(net, "ioctl(sctty)");
#  if defined(CRAY)
	/*
	 * Close the hard fd to /dev/ttypXXX, and re-open through
	 * the indirect /dev/tty interface.
	 */
	close(t);
	if ((t = open("/dev/tty", O_RDWR)) < 0)
		fatalperror(net, "open(/dev/tty)");
#  endif
# else
	/*
	 * We get our controlling tty assigned as a side-effect
	 * of opening up a tty device.  But on BSD based systems,
	 * this only happens if our process group is zero.  The
	 * setsid() call above may have set our pgrp, so clear
	 * it out before opening the tty...
	 */
	(void) setpgrp(0, 0);
	close(open(line, O_RDWR));
# endif
	if (t != 0)
		(void) dup2(t, 0);
	if (t != 1)
		(void) dup2(t, 1);
	if (t != 2)
		(void) dup2(t, 2);
	if (t > 2)
		close(t);
	return(0);
}
#endif	/* BSD <= 43 */

#ifdef	NEWINIT
char *gen_id = "fe";
#endif

/*
 * startslave(host)
 *
 * Given a hostname, do whatever
 * is necessary to startup the login process on the slave side of the pty.
 */

/* ARGSUSED */
	void
startslave(host, autologin, autoname)
	char *host;
	int autologin;
	char *autoname;
{
	register int i;
	long time();
	char name[256];
#ifdef	NEWINIT
	extern char *ptyip;
	struct init_request request;
	void nologinproc();
	register int n;
#endif	/* NEWINIT */

#if	defined(AUTHENTICATION)
	if (!autoname || !autoname[0])
		autologin = 0;

	if (autologin < auth_level) {
		fatal(net, "Authorization failed");
		exit(1);
	}
#endif

#ifndef	NEWINIT
# ifdef	PARENT_DOES_UTMP
	utmp_sig_init();
# endif	/* PARENT_DOES_UTMP */

	if ((i = fork()) < 0)
		fatalperror(net, "fork");
	if (i) {
# ifdef PARENT_DOES_UTMP
		/*
		 * Cray parent will create utmp entry for child and send
		 * signal to child to tell when done.  Child waits for signal
		 * before doing anything important.
		 */
		register int pid = i;
		void sigjob P((int));

		setpgrp();
		utmp_sig_reset();		/* reset handler to default */
		/*
		 * Create utmp entry for child
		 */
		(void) time(&wtmp.ut_time);
		wtmp.ut_type = LOGIN_PROCESS;
		wtmp.ut_pid = pid;
		SCPYN(wtmp.ut_user, "LOGIN");
		SCPYN(wtmp.ut_host, host);
		SCPYN(wtmp.ut_line, line + sizeof("/dev/") - 1);
#ifndef	__hpux
		SCPYN(wtmp.ut_id, wtmp.ut_line+3);
#else
		SCPYN(wtmp.ut_id, wtmp.ut_line+7);
#endif
		pututline(&wtmp);
		endutent();
		if ((i = open(wtmpf, O_WRONLY|O_APPEND)) >= 0) {
			(void) write(i, (char *)&wtmp, sizeof(struct utmp));
			(void) close(i);
		}
#ifdef	CRAY
		(void) signal(WJSIGNAL, sigjob);
#endif
		utmp_sig_notify(pid);
# endif	/* PARENT_DOES_UTMP */
	} else {
		getptyslave(autologin);
		start_login(host, autologin, autoname);
		/*NOTREACHED*/
	}
#else	/* NEWINIT */

	/*
	 * Init will start up login process if we ask nicely.  We only wait
	 * for it to start up and begin normal telnet operation.
	 */
	if ((i = open(INIT_FIFO, O_WRONLY)) < 0) {
		char tbuf[128];
		(void) sprintf(tbuf, "Can't open %s\n", INIT_FIFO);
		fatalperror(net, tbuf);
	}
	memset((char *)&request, 0, sizeof(request));
	request.magic = INIT_MAGIC;
	SCPYN(request.gen_id, gen_id);
	SCPYN(request.tty_id, &line[8]);
	SCPYN(request.host, host);
	SCPYN(request.term_type, terminaltype ? terminaltype : "network");
#if	!defined(UNICOS5)
	request.signal = SIGCLD;
	request.pid = getpid();
#endif
#ifdef BFTPDAEMON
	/*
	 * Are we working as the bftp daemon?
	 */
	if (bftpd) {
		SCPYN(request.exec_name, BFTPPATH);
	}
#endif /* BFTPDAEMON */
	if (write(i, (char *)&request, sizeof(request)) < 0) {
		char tbuf[128];
		(void) sprintf(tbuf, "Can't write to %s\n", INIT_FIFO);
		fatalperror(net, tbuf);
	}
	(void) close(i);
	(void) signal(SIGALRM, nologinproc);
	for (i = 0; ; i++) {
		char tbuf[128];
		alarm(15);
		n = read(pty, ptyip, BUFSIZ);
		if (i == 3 || n >= 0 || !gotalarm)
			break;
		gotalarm = 0;
		sprintf(tbuf, "telnetd: waiting for /etc/init to start login process on %s\r\n", line);
		(void) write(net, tbuf, strlen(tbuf));
	}
	if (n < 0 && gotalarm)
		fatal(net, "/etc/init didn't start login process");
	pcc += n;
	alarm(0);
	(void) signal(SIGALRM, SIG_DFL);

	return;
#endif	/* NEWINIT */
}

char	*envinit[3];
extern char **environ;

	void
init_env()
{
	extern char *getenv();
	char **envp;

	envp = envinit;
	if (*envp = getenv("TZ"))
		*envp++ -= 3;
#if	defined(CRAY) || defined(__hpux)
	else
		*envp++ = "TZ=GMT0";
#endif
	*envp = 0;
	environ = envinit;
}

#ifndef	NEWINIT

/*
 * start_login(host)
 *
 * Assuming that we are now running as a child processes, this
 * function will turn us into the login process.
 */

	void
start_login(host, autologin, name)
	char *host;
	int autologin;
	char *name;
{
	register char *cp;
	register char **argv;
	char **addarg();
	extern char *getenv();
#ifdef	UTMPX
	register int pid = getpid();
	struct utmpx utmpx;
#endif
#ifdef SOLARIS
	char *term;
	char termbuf[64];
#endif

#ifdef	UTMPX
	/*
	 * Create utmp entry for child
	 */

	bzero(&utmpx, sizeof(utmpx));
	SCPYN(utmpx.ut_user, ".telnet");
	SCPYN(utmpx.ut_line, line + sizeof("/dev/") - 1);
	utmpx.ut_pid = pid;
	utmpx.ut_id[0] = 't';
	utmpx.ut_id[1] = 'n';
	utmpx.ut_id[2] = SC_WILDC;
	utmpx.ut_id[3] = SC_WILDC;
	utmpx.ut_type = LOGIN_PROCESS;
	(void) time(&utmpx.ut_tv.tv_sec);
	if (makeutx(&utmpx) == NULL)
		fatal(net, "makeutx failed");
#endif

	/*
	 * -h : pass on name of host.
	 *		WARNING:  -h is accepted by login if and only if
	 *			getuid() == 0.
	 * -p : don't clobber the environment (so terminal type stays set).
	 *
	 * -f : force this login, he has already been authenticated
	 */
	argv = addarg(0, "login");

#if	!defined(NO_LOGIN_H)

# if	defined (AUTHENTICATION) && defined(NO_LOGIN_F) && defined(LOGIN_R)
	/*
	 * Don't add the "-h host" option if we are going
	 * to be adding the "-r host" option down below...
	 */
	if ((auth_level < 0) || (autologin != AUTH_VALID))
# endif
	{
		argv = addarg(argv, "-h");
		argv = addarg(argv, host);
#ifdef	SOLARIS
		/*
		 * SVR4 version of -h takes TERM= as second arg, or -
		 */
		term = getenv("TERM");
		if (term == NULL || term[0] == 0) {
			term = "-";
		} else {
			strcpy(termbuf, "TERM=");
			strncat(termbuf, term, sizeof(termbuf) - 6);
			term = termbuf;
		}
		argv = addarg(argv, term);
#endif
	}
#endif
#if	!defined(NO_LOGIN_P)
	argv = addarg(argv, "-p");
#endif
#ifdef	BFTPDAEMON
	/*
	 * Are we working as the bftp daemon?  If so, then ask login
	 * to start bftp instead of shell.
	 */
	if (bftpd) {
		argv = addarg(argv, "-e");
		argv = addarg(argv, BFTPPATH);
	} else 
#endif
#if	defined (SecurID)
	/*
	 * don't worry about the -f that might get sent.
	 * A -s is supposed to override it anyhow.
	 */
	if (require_SecurID)
		argv = addarg(argv, "-s");
#endif
#if	defined (AUTHENTICATION)
	if (auth_level >= 0 && autologin == AUTH_VALID) {
# if	!defined(NO_LOGIN_F)
		argv = addarg(argv, "-f");
		argv = addarg(argv, name);
# else
#  if defined(LOGIN_R)
		/*
		 * We don't have support for "login -f", but we
		 * can fool /bin/login into thinking that we are
		 * rlogind, and allow us to log in without a
		 * password.  The rlogin protocol expects
		 *	local-user\0remote-user\0term/speed\0
		 */

		if (pty > 2) {
			register char *cp;
			char speed[128];
			int isecho, israw, xpty, len;
			extern int def_rspeed;
#  ifndef LOGIN_HOST
			/*
			 * Tell login that we are coming from "localhost".
			 * If we passed in the real host name, then the
			 * user would have to allow .rhost access from
			 * every machine that they want authenticated
			 * access to work from, which sort of defeats
			 * the purpose of an authenticated login...
			 * So, we tell login that the session is coming
			 * from "localhost", and the user will only have
			 * to have "localhost" in their .rhost file.
			 */
#			define LOGIN_HOST "localhost"
#  endif
			argv = addarg(argv, "-r");
			argv = addarg(argv, LOGIN_HOST);

			xpty = pty;
# ifndef  STREAMSPTY
			pty = 0;
# else
			ttyfd = 0;
# endif
			init_termbuf();
			isecho = tty_isecho();
			israw = tty_israw();
			if (isecho || !israw) {
				tty_setecho(0);		/* Turn off echo */
				tty_setraw(1);		/* Turn on raw */
				set_termbuf();
			}
			len = strlen(name)+1;
			write(xpty, name, len);
			write(xpty, name, len);
			sprintf(speed, "%s/%d", (cp = getenv("TERM")) ? cp : "",
				(def_rspeed > 0) ? def_rspeed : 9600);
			len = strlen(speed)+1;
			write(xpty, speed, len);

			if (isecho || !israw) {
				init_termbuf();
				tty_setecho(isecho);
				tty_setraw(israw);
				set_termbuf();
				if (!israw) {
					/*
					 * Write a newline to ensure
					 * that login will be able to
					 * read the line...
					 */
					write(xpty, "\n", 1);
				}
			}
			pty = xpty;
		}
#  else
		argv = addarg(argv, name);
#  endif
# endif
	} else
#endif
	if (getenv("USER")) {
		argv = addarg(argv, getenv("USER"));
#if	defined(LOGIN_ARGS) && defined(NO_LOGIN_P)
		{
			register char **cpp;
			for (cpp = environ; *cpp; cpp++)
				argv = addarg(argv, *cpp);
		}
#endif
		/*
		 * Assume that login will set the USER variable
		 * correctly.  For SysV systems, this means that
		 * USER will no longer be set, just LOGNAME by
		 * login.  (The problem is that if the auto-login
		 * fails, and the user then specifies a different
		 * account name, he can get logged in with both
		 * LOGNAME and USER in his environment, but the
		 * USER value will be wrong.
		 */
		unsetenv("USER");
	}
#if	defined(AUTHENTICATION) && defined(NO_LOGIN_F) && defined(LOGIN_R)
	if (pty > 2)
		close(pty);
#endif
	closelog();
	execv(_PATH_LOGIN, argv);

	syslog(LOG_ERR, "%s: %m\n", _PATH_LOGIN);
	fatalperror(net, _PATH_LOGIN);
	/*NOTREACHED*/
}

	char **
addarg(argv, val)
	register char **argv;
	register char *val;
{
	register char **cpp;

	if (argv == NULL) {
		/*
		 * 10 entries, a leading length, and a null
		 */
		argv = (char **)malloc(sizeof(*argv) * 12);
		if (argv == NULL)
			return(NULL);
		*argv++ = (char *)10;
		*argv = (char *)0;
	}
	for (cpp = argv; *cpp; cpp++)
		;
	if (cpp == &argv[(int)argv[-1]]) {
		--argv;
		*argv = (char *)((int)(*argv) + 10);
		argv = (char **)realloc(argv, (int)(*argv) + 2);
		if (argv == NULL)
			return(NULL);
		argv++;
		cpp = &argv[(int)argv[-1] - 10];
	}
	*cpp++ = val;
	*cpp = 0;
	return(argv);
}
#endif	/* NEWINIT */

/*
 * cleanup()
 *
 * This is the routine to call when we are all through, to
 * clean up anything that needs to be cleaned up.
 */
	/* ARGSUSED */
	void
cleanup(sig)
	int sig;
{
#ifndef	PARENT_DOES_UTMP
# if (BSD > 43) || defined(convex)
	char *p;

	p = line + sizeof("/dev/") - 1;
	if (logout(p))
		logwtmp(p, "", "");
	(void)chmod(line, 0666);
	(void)chown(line, 0, 0);
	*p = 'p';
	(void)chmod(line, 0666);
	(void)chown(line, 0, 0);
	(void) shutdown(net, 2);
	exit(1);
# else
	void rmut();

	rmut();
	vhangup();	/* XXX */
	(void) shutdown(net, 2);
	exit(1);
# endif
#else	/* PARENT_DOES_UTMP */
# ifdef	NEWINIT
	(void) shutdown(net, 2);
	exit(1);
# else	/* NEWINIT */
#  ifdef CRAY
	static int incleanup = 0;
	register int t;

	/*
	 * 1: Pick up the zombie, if we are being called
	 *    as the signal handler.
	 * 2: If we are a nested cleanup(), return.
	 * 3: Try to clean up TMPDIR.
	 * 4: Fill in utmp with shutdown of process.
	 * 5: Close down the network and pty connections.
	 * 6: Finish up the TMPDIR cleanup, if needed.
	 */
	if (sig == SIGCHLD)
		while (waitpid(-1, 0, WNOHANG) > 0)
			;	/* VOID */
	t = sigblock(sigmask(SIGCHLD));
	if (incleanup) {
		sigsetmask(t);
		return;
	}
	incleanup = 1;
	sigsetmask(t);
	if (secflag) {
		/*
		 *	We need to set ourselves back to a null
		 *	label to clean up.
		 */

		setulvl(sysv.sy_minlvl);
		setucmp((long)0);
	}

	t = cleantmp(&wtmp);
	setutent();	/* just to make sure */
#  endif /* CRAY */
	rmut(line);
	close(pty);
	(void) shutdown(net, 2);
#  ifdef CRAY
	if (t == 0)
		cleantmp(&wtmp);
#  endif /* CRAY */
	exit(1);
# endif	/* NEWINT */
#endif	/* PARENT_DOES_UTMP */
}

#if defined(PARENT_DOES_UTMP) && !defined(NEWINIT)
/*
 * _utmp_sig_rcv
 * utmp_sig_init
 * utmp_sig_wait
 *	These three functions are used to coordinate the handling of
 *	the utmp file between the server and the soon-to-be-login shell.
 *	The server actually creates the utmp structure, the child calls
 *	utmp_sig_wait(), until the server calls utmp_sig_notify() and
 *	signals the future-login shell to proceed.
 */
static int caught=0;		/* NZ when signal intercepted */
static void (*func)();		/* address of previous handler */

	void
_utmp_sig_rcv(sig)
	int sig;
{
	caught = 1;
	(void) signal(SIGUSR1, func);
}

	void
utmp_sig_init()
{
	/*
	 * register signal handler for UTMP creation
	 */
	if ((int)(func = signal(SIGUSR1, _utmp_sig_rcv)) == -1)
		fatalperror(net, "telnetd/signal");
}

	void
utmp_sig_reset()
{
	(void) signal(SIGUSR1, func);	/* reset handler to default */
}

# ifdef __hpux
# define sigoff() /* do nothing */
# define sigon() /* do nothing */
# endif

	void
utmp_sig_wait()
{
	/*
	 * Wait for parent to write our utmp entry.
	 */
	sigoff();
	while (caught == 0) {
		pause();	/* wait until we get a signal (sigon) */
		sigoff();	/* turn off signals while we check caught */
	}
	sigon();		/* turn on signals again */
}

	void
utmp_sig_notify(pid)
{
	kill(pid, SIGUSR1);
}

# ifdef CRAY
static int gotsigjob = 0;

	/*ARGSUSED*/
	void
sigjob(sig)
	int sig;
{
	register int jid;
	register struct jobtemp *jp;

	while ((jid = waitjob(NULL)) != -1) {
		if (jid == 0) {
			return;
		}
		gotsigjob++;
		jobend(jid, NULL, NULL);
	}
}

/*
 * Clean up the TMPDIR that login created.
 * The first time this is called we pick up the info
 * from the utmp.  If the job has already gone away,
 * then we'll clean up and be done.  If not, then
 * when this is called the second time it will wait
 * for the signal that the job is done.
 */
	int
cleantmp(wtp)
	register struct utmp *wtp;
{
	struct utmp *utp;
	static int first = 1;
	register int mask, omask, ret;
	extern struct utmp *getutid P((const struct utmp *_Id));


	mask = sigmask(WJSIGNAL);

	if (first == 0) {
		omask = sigblock(mask);
		while (gotsigjob == 0)
			sigpause(omask);
		return(1);
	}
	first = 0;
	setutent();	/* just to make sure */

	utp = getutid(wtp);
	if (utp == 0) {
		syslog(LOG_ERR, "Can't get /etc/utmp entry to clean TMPDIR");
		return(-1);
	}
	/*
	 * Nothing to clean up if the user shell was never started.
	 */
	if (utp->ut_type != USER_PROCESS || utp->ut_jid == 0)
		return(1);

	/*
	 * Block the WJSIGNAL while we are in jobend().
	 */
	omask = sigblock(mask);
	ret = jobend(utp->ut_jid, utp->ut_tpath, utp->ut_user);
	sigsetmask(omask);
	return(ret);
}

	int
jobend(jid, path, user)
	register int jid;
	register char *path;
	register char *user;
{
	static int saved_jid = 0;
	static char saved_path[sizeof(wtmp.ut_tpath)+1];
	static char saved_user[sizeof(wtmp.ut_user)+1];

	if (path) {
		strncpy(saved_path, path, sizeof(wtmp.ut_tpath));
		strncpy(saved_user, user, sizeof(wtmp.ut_user));
		saved_path[sizeof(saved_path)] = '\0';
		saved_user[sizeof(saved_user)] = '\0';
	}
	if (saved_jid == 0) {
		saved_jid = jid;
		return(0);
	}
	cleantmpdir(jid, saved_path, saved_user);
	return(1);
}

/*
 * Fork a child process to clean up the TMPDIR
 */
cleantmpdir(jid, tpath, user)
	register int jid;
	register char *tpath;
	register char *user;
{
	switch(fork()) {
	case -1:
		syslog(LOG_ERR, "TMPDIR cleanup(%s): fork() failed: %m\n",
							tpath);
		break;
	case 0:
		execl(CLEANTMPCMD, CLEANTMPCMD, user, tpath, 0);
		syslog(LOG_ERR, "TMPDIR cleanup(%s): execl(%s) failed: %m\n",
							tpath, CLEANTMPCMD);
		exit(1);
	default:
		/*
		 * Forget about child.  We will exit, and
		 * /etc/init will pick it up.
		 */
		break;
	}
}
# endif /* CRAY */
#endif	/* defined(PARENT_DOES_UTMP) && !defined(NEWINIT) */

/*
 * rmut()
 *
 * This is the function called by cleanup() to
 * remove the utmp entry for this person.
 */

#ifdef	UTMPX
	void
rmut()
{
	register f;
	int found = 0;
	struct utmp *u, *utmp;
	int nutmp;
	struct stat statbf;

	struct utmpx *utxp, utmpx;

	/*
	 * This updates the utmpx and utmp entries and make a wtmp/x entry
	 */

	SCPYN(utmpx.ut_line, line + sizeof("/dev/") - 1);
	utxp = getutxline(&utmpx);
	if (utxp) {
		utxp->ut_type = DEAD_PROCESS;
		utxp->ut_exit.e_termination = 0;
		utxp->ut_exit.e_exit = 0;
		(void) time(&utmpx.ut_tv.tv_sec);
		utmpx.ut_tv.tv_usec = 0;
		modutx(utxp);
	}
	endutxent();
}  /* end of rmut */
#endif

#if	!defined(UTMPX) && !(defined(CRAY) || defined(__hpux)) && BSD <= 43
	void
rmut()
{
	register f;
	int found = 0;
	struct utmp *u, *utmp;
	int nutmp;
	struct stat statbf;

	f = open(utmpf, O_RDWR);
	if (f >= 0) {
		(void) fstat(f, &statbf);
		utmp = (struct utmp *)malloc((unsigned)statbf.st_size);
		if (!utmp)
			syslog(LOG_ERR, "utmp malloc failed");
		if (statbf.st_size && utmp) {
			nutmp = read(f, (char *)utmp, (int)statbf.st_size);
			nutmp /= sizeof(struct utmp);
		
			for (u = utmp ; u < &utmp[nutmp] ; u++) {
				if (SCMPN(u->ut_line, line+5) ||
				    u->ut_name[0]==0)
					continue;
				(void) lseek(f, ((long)u)-((long)utmp), L_SET);
				SCPYN(u->ut_name, "");
				SCPYN(u->ut_host, "");
				(void) time(&u->ut_time);
				(void) write(f, (char *)u, sizeof(wtmp));
				found++;
			}
		}
		(void) close(f);
	}
	if (found) {
		f = open(wtmpf, O_WRONLY|O_APPEND);
		if (f >= 0) {
			SCPYN(wtmp.ut_line, line+5);
			SCPYN(wtmp.ut_name, "");
			SCPYN(wtmp.ut_host, "");
			(void) time(&wtmp.ut_time);
			(void) write(f, (char *)&wtmp, sizeof(wtmp));
			(void) close(f);
		}
	}
	(void) chmod(line, 0666);
	(void) chown(line, 0, 0);
	line[strlen("/dev/")] = 'p';
	(void) chmod(line, 0666);
	(void) chown(line, 0, 0);
}  /* end of rmut */
#endif	/* CRAY */

#ifdef __hpux
rmut (line)
char *line;
{
	struct utmp utmp;
	struct utmp *utptr;
	int fd;			/* for /etc/wtmp */

	utmp.ut_type = USER_PROCESS;
	(void) strncpy(utmp.ut_id, line+12, sizeof(utmp.ut_id));
	(void) setutent();
	utptr = getutid(&utmp);
	/* write it out only if it exists */
	if (utptr) {
		utptr->ut_type = DEAD_PROCESS;
		utptr->ut_time = time((long *) 0);
		(void) pututline(utptr);
		/* set wtmp entry if wtmp file exists */
		if ((fd = open(wtmpf, O_WRONLY | O_APPEND)) >= 0) {
			(void) write(fd, utptr, sizeof(utmp));
			(void) close(fd);
		}
	}
	(void) endutent();

	(void) chmod(line, 0666);
	(void) chown(line, 0, 0);
	line[14] = line[13];
	line[13] = line[12];
	line[8] = 'm';
	line[9] = '/';
	line[10] = 'p';
	line[11] = 't';
	line[12] = 'y';
	(void) chmod(line, 0666);
	(void) chown(line, 0, 0);
}
#endif
