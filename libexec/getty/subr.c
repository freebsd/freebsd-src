/*
 * Copyright (c) 1983, 1993
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
/*static char sccsid[] = "from: @(#)subr.c	8.1 (Berkeley) 6/4/93";*/
static char rcsid[] = "$Id: subr.c,v 1.6 1996/05/05 19:01:11 joerg Exp $";
#endif /* not lint */

/*
 * Melbourne getty.
 */
#define COMPAT_43
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#ifdef DEBUG
#include <stdio.h>
#endif

#include "gettytab.h"
#include "pathnames.h"
#include "extern.h"


#ifdef COMPAT_43
static void	compatflags __P((long));
#endif

/*
 * Get a table entry.
 */
void
gettable(name, buf)
	const char *name;
	char *buf;
{
	register struct gettystrs *sp;
	register struct gettynums *np;
	register struct gettyflags *fp;
	long n;
	const char *dba[2];
	dba[0] = _PATH_GETTYTAB;
	dba[1] = 0;

	if (cgetent(&buf, (char**)dba, (char*)name) != 0)
		return;

	for (sp = gettystrs; sp->field; sp++)
		cgetstr(buf, (char*)sp->field, &sp->value);
	for (np = gettynums; np->field; np++) {
		if (cgetnum(buf, (char*)np->field, &n) == -1)
			np->set = 0;
		else {
			np->set = 1;
			np->value = n;
		}
	}
	for (fp = gettyflags; fp->field; fp++) {
		if (cgetcap(buf, (char*)fp->field, ':') == NULL)
			fp->set = 0;
		else {
			fp->set = 1;
			fp->value = 1 ^ fp->invrt;
		}
	}
#ifdef DEBUG
	printf("name=\"%s\", buf=\"%s\"\r\n", name, buf);
	for (sp = gettystrs; sp->field; sp++)
		printf("cgetstr: %s=%s\r\n", sp->field, sp->value);
	for (np = gettynums; np->field; np++)
		printf("cgetnum: %s=%d\r\n", np->field, np->value);
	for (fp = gettyflags; fp->field; fp++)
		printf("cgetflags: %s='%c' set='%c'\r\n", fp->field, 
		       fp->value + '0', fp->set + '0');
#endif /* DEBUG */
}

void
gendefaults()
{
	register struct gettystrs *sp;
	register struct gettynums *np;
	register struct gettyflags *fp;

	for (sp = gettystrs; sp->field; sp++)
		if (sp->value)
			sp->defalt = sp->value;
	for (np = gettynums; np->field; np++)
		if (np->set)
			np->defalt = np->value;
	for (fp = gettyflags; fp->field; fp++)
		if (fp->set)
			fp->defalt = fp->value;
		else
			fp->defalt = fp->invrt;
}

void
setdefaults()
{
	register struct gettystrs *sp;
	register struct gettynums *np;
	register struct gettyflags *fp;

	for (sp = gettystrs; sp->field; sp++)
		if (!sp->value)
			sp->value = sp->defalt;
	for (np = gettynums; np->field; np++)
		if (!np->set)
			np->value = np->defalt;
	for (fp = gettyflags; fp->field; fp++)
		if (!fp->set)
			fp->value = fp->defalt;
}

static char **
charnames[] = {
	&ER, &KL, &IN, &QU, &XN, &XF, &ET, &BK,
	&SU, &DS, &RP, &FL, &WE, &LN, 0
};

static char *
charvars[] = {
	&tmode.c_cc[VERASE], &tmode.c_cc[VKILL], &tmode.c_cc[VINTR],
	&tmode.c_cc[VQUIT], &tmode.c_cc[VSTART], &tmode.c_cc[VSTOP],
	&tmode.c_cc[VEOF], &tmode.c_cc[VEOL], &tmode.c_cc[VSUSP],
	&tmode.c_cc[VDSUSP], &tmode.c_cc[VREPRINT], &tmode.c_cc[VDISCARD],
	&tmode.c_cc[VWERASE], &tmode.c_cc[VLNEXT], 0
};

void
setchars()
{
	register int i;
	register const char *p;

	for (i = 0; charnames[i]; i++) {
		p = *charnames[i];
		if (p && *p)
			*charvars[i] = *p;
		else
			*charvars[i] = _POSIX_VDISABLE;
	}
}

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

void
setflags(n)
	int n;
{
	register tcflag_t iflag, oflag, cflag, lflag;

#ifdef COMPAT_43
	switch (n) {
	case 0:
		if (F0set) {
			compatflags(F0);
			return;
		}
		break;
	case 1:
		if (F1set) {
			compatflags(F1);
			return;
		}
		break;
	default:
		if (F2set) {
			compatflags(F2);
			return;
		}
		break;
	}
#endif

	switch (n) {
	case 0:
		if (C0set && I0set && L0set && O0set) {
			tmode.c_cflag = C0;
			tmode.c_iflag = I0;
			tmode.c_lflag = L0;
			tmode.c_oflag = O0;
			return;
		}
		break;
	case 1:
		if (C1set && I1set && L1set && O1set) {
			tmode.c_cflag = C1;
			tmode.c_iflag = I1;
			tmode.c_lflag = L1;
			tmode.c_oflag = O1;
			return;
		}
		break;
	default:
		if (C2set && I2set && L2set && O2set) {
			tmode.c_cflag = C2;
			tmode.c_iflag = I2;
			tmode.c_lflag = L2;
			tmode.c_oflag = O2;
			return;
		}
		break;
	}

	iflag = omode.c_iflag;
	oflag = omode.c_oflag;
	cflag = omode.c_cflag;
	lflag = omode.c_lflag;

	if (NP) {
		CLR(cflag, CSIZE|PARENB);
		SET(cflag, CS8);
		CLR(iflag, ISTRIP|INPCK|IGNPAR);
	} else if (AP || EP || OP) {
		CLR(cflag, CSIZE);
		SET(cflag, CS7|PARENB);
		SET(iflag, ISTRIP);
		if (OP && !EP) {
			SET(iflag, INPCK|IGNPAR);
			SET(cflag, PARODD);
			if (AP)
				CLR(iflag, INPCK);
		} else if (EP && !OP) {
			SET(iflag, INPCK|IGNPAR);
			CLR(cflag, PARODD);
			if (AP)
				CLR(iflag, INPCK);
		} else if (AP || (EP && OP)) {
			CLR(iflag, INPCK|IGNPAR);
			CLR(cflag, PARODD);
		}
	} /* else, leave as is */

#if 0
	if (UC)
		f |= LCASE;
#endif

	if (HC)
		SET(cflag, HUPCL);
	else
		CLR(cflag, HUPCL);

	if (MB)
		SET(cflag, MDMBUF);
	else
		CLR(cflag, MDMBUF);

	if (HW)
		SET(cflag, CRTSCTS);
	else
		CLR(cflag, CRTSCTS);

	if (NL) {
		SET(iflag, ICRNL);
		SET(oflag, ONLCR|OPOST);
	} else {
		CLR(iflag, ICRNL);
		CLR(oflag, ONLCR);
	}

	if (!HT)
		SET(oflag, OXTABS|OPOST);
	else
		CLR(oflag, OXTABS);

#ifdef XXX_DELAY
	SET(f, delaybits());
#endif

	if (n == 1) {		/* read mode flags */
		if (RW) {
			iflag = 0;
			CLR(oflag, OPOST);
			CLR(cflag, CSIZE|PARENB);
			SET(cflag, CS8);
			lflag = 0;
		} else {
			CLR(lflag, ICANON);
		}
		goto out;
	}

	if (n == 0)
		goto out;

#if 0
	if (CB)
		SET(f, CRTBS);
#endif

	if (CE)
		SET(lflag, ECHOE);
	else
		CLR(lflag, ECHOE);

	if (CK)
		SET(lflag, ECHOKE);
	else
		CLR(lflag, ECHOKE);

	if (PE)
		SET(lflag, ECHOPRT);
	else
		CLR(lflag, ECHOPRT);

	if (EC)
		SET(lflag, ECHO);
	else
		CLR(lflag, ECHO);

	if (XC)
		SET(lflag, ECHOCTL);
	else
		CLR(lflag, ECHOCTL);

	if (DX)
		SET(lflag, IXANY);
	else
		CLR(lflag, IXANY);

out:
	tmode.c_iflag = iflag;
	tmode.c_oflag = oflag;
	tmode.c_cflag = cflag;
	tmode.c_lflag = lflag;
}

#ifdef COMPAT_43
/*
 * Old TTY => termios, snatched from <sys/kern/tty_compat.c>
 */
void
compatflags(flags)
register long flags;
{
	register tcflag_t iflag, oflag, cflag, lflag;

	iflag = BRKINT|ICRNL|IMAXBEL|IXON|IXANY;
	oflag = OPOST|ONLCR|OXTABS;
	cflag = CREAD;
	lflag = ICANON|ISIG|IEXTEN;

	if (ISSET(flags, TANDEM))
		SET(iflag, IXOFF);
	else
		CLR(iflag, IXOFF);
	if (ISSET(flags, ECHO))
		SET(lflag, ECHO);
	else
		CLR(lflag, ECHO);
	if (ISSET(flags, CRMOD)) {
		SET(iflag, ICRNL);
		SET(oflag, ONLCR);
	} else {
		CLR(iflag, ICRNL);
		CLR(oflag, ONLCR);
	}
	if (ISSET(flags, XTABS))
		SET(oflag, OXTABS);
	else
		CLR(oflag, OXTABS);


	if (ISSET(flags, RAW)) {
		iflag &= IXOFF;
		CLR(lflag, ISIG|ICANON|IEXTEN);
		CLR(cflag, PARENB);
	} else {
		SET(iflag, BRKINT|IXON|IMAXBEL);
		SET(lflag, ISIG|IEXTEN);
		if (ISSET(flags, CBREAK))
			CLR(lflag, ICANON);
		else
			SET(lflag, ICANON);
		switch (ISSET(flags, ANYP)) {
		case 0:
			CLR(cflag, PARENB);
			break;
		case ANYP:
			SET(cflag, PARENB);
			CLR(iflag, INPCK);
			break;
		case EVENP:
			SET(cflag, PARENB);
			SET(iflag, INPCK);
			CLR(cflag, PARODD);
			break;
		case ODDP:
			SET(cflag, PARENB);
			SET(iflag, INPCK);
			SET(cflag, PARODD);
			break;
		}
	}

	/* Nothing we can do with CRTBS. */
	if (ISSET(flags, PRTERA))
		SET(lflag, ECHOPRT);
	else
		CLR(lflag, ECHOPRT);
	if (ISSET(flags, CRTERA))
		SET(lflag, ECHOE);
	else
		CLR(lflag, ECHOE);
	/* Nothing we can do with TILDE. */
	if (ISSET(flags, MDMBUF))
		SET(cflag, MDMBUF);
	else
		CLR(cflag, MDMBUF);
	if (ISSET(flags, NOHANG))
		CLR(cflag, HUPCL);
	else
		SET(cflag, HUPCL);
	if (ISSET(flags, CRTKIL))
		SET(lflag, ECHOKE);
	else
		CLR(lflag, ECHOKE);
	if (ISSET(flags, CTLECH))
		SET(lflag, ECHOCTL);
	else
		CLR(lflag, ECHOCTL);
	if (!ISSET(flags, DECCTQ))
		SET(iflag, IXANY);
	else
		CLR(iflag, IXANY);
	CLR(lflag, TOSTOP|FLUSHO|PENDIN|NOFLSH);
	SET(lflag, ISSET(flags, TOSTOP|FLUSHO|PENDIN|NOFLSH));

	if (ISSET(flags, RAW|LITOUT|PASS8)) {
		CLR(cflag, CSIZE);
		SET(cflag, CS8);
		if (!ISSET(flags, RAW|PASS8))
			SET(iflag, ISTRIP);
		else
			CLR(iflag, ISTRIP);
		if (!ISSET(flags, RAW|LITOUT))
			SET(oflag, OPOST);
		else
			CLR(oflag, OPOST);
	} else {
		CLR(cflag, CSIZE);
		SET(cflag, CS7);
		SET(iflag, ISTRIP);
		SET(oflag, OPOST);
	}

	tmode.c_iflag = iflag;
	tmode.c_oflag = oflag;
	tmode.c_cflag = cflag;
	tmode.c_lflag = lflag;
}
#endif

#ifdef XXX_DELAY
struct delayval {
	unsigned	delay;		/* delay in ms */
	int		bits;
};

/*
 * below are random guesses, I can't be bothered checking
 */

struct delayval	crdelay[] = {
	{ 1,		CR1 },
	{ 2,		CR2 },
	{ 3,		CR3 },
	{ 83,		CR1 },
	{ 166,		CR2 },
	{ 0,		CR3 },
};

struct delayval nldelay[] = {
	{ 1,		NL1 },		/* special, calculated */
	{ 2,		NL2 },
	{ 3,		NL3 },
	{ 100,		NL2 },
	{ 0,		NL3 },
};

struct delayval	bsdelay[] = {
	{ 1,		BS1 },
	{ 0,		0 },
};

struct delayval	ffdelay[] = {
	{ 1,		FF1 },
	{ 1750,		FF1 },
	{ 0,		FF1 },
};

struct delayval	tbdelay[] = {
	{ 1,		TAB1 },
	{ 2,		TAB2 },
	{ 3,		XTABS },	/* this is expand tabs */
	{ 100,		TAB1 },
	{ 0,		TAB2 },
};

int
delaybits()
{
	register int f;

	f  = adelay(CD, crdelay);
	f |= adelay(ND, nldelay);
	f |= adelay(FD, ffdelay);
	f |= adelay(TD, tbdelay);
	f |= adelay(BD, bsdelay);
	return (f);
}

int
adelay(ms, dp)
	register ms;
	register struct delayval *dp;
{
	if (ms == 0)
		return (0);
	while (dp->delay && ms > dp->delay)
		dp++;
	return (dp->bits);
}
#endif

char	editedhost[MAXHOSTNAMELEN];

void
edithost(pat)
	register const char *pat;
{
	register const char *host = HN;
	register char *res = editedhost;

	if (!pat)
		pat = "";
	while (*pat) {
		switch (*pat) {

		case '#':
			if (*host)
				host++;
			break;

		case '@':
			if (*host)
				*res++ = *host++;
			break;

		default:
			*res++ = *pat;
			break;

		}
		if (res == &editedhost[sizeof editedhost - 1]) {
			*res = '\0';
			return;
		}
		pat++;
	}
	if (*host)
		strncpy(res, host, sizeof editedhost - (res - editedhost) - 1);
	else
		*res = '\0';
	editedhost[sizeof editedhost - 1] = '\0';
}

static struct speedtab {
	int	speed;
	int	uxname;
} speedtab[] = {
	{ 50,	B50 },
	{ 75,	B75 },
	{ 110,	B110 },
	{ 134,	B134 },
	{ 150,	B150 },
	{ 200,	B200 },
	{ 300,	B300 },
	{ 600,	B600 },
	{ 1200,	B1200 },
	{ 1800,	B1800 },
	{ 2400,	B2400 },
	{ 4800,	B4800 },
	{ 9600,	B9600 },
	{ 19200, EXTA },
	{ 19,	EXTA },		/* for people who say 19.2K */
	{ 38400, EXTB },
	{ 38,	EXTB },
	{ 7200,	EXTB },		/* alternative */
	{ 57600, B57600 },
	{ 115200, B115200 },
	{ 0 }
};

int
speed(val)
	int val;
{
	register struct speedtab *sp;

	if (val <= B115200)
		return (val);

	for (sp = speedtab; sp->speed; sp++)
		if (sp->speed == val)
			return (sp->uxname);

	return (B300);		/* default in impossible cases */
}

void
makeenv(env)
	char *env[];
{
	static char termbuf[128] = "TERM=";
	register char *p, *q;
	register char **ep;

	ep = env;
	if (TT && *TT) {
		strcat(termbuf, TT);
		*ep++ = termbuf;
	}
	if ((p = EV)) {
		q = p;
		while ((q = strchr(q, ','))) {
			*q++ = '\0';
			*ep++ = p;
			p = q;
		}
		if (*p)
			*ep++ = p;
	}
	*ep = (char *)0;
}

/*
 * This speed select mechanism is written for the Develcon DATASWITCH.
 * The Develcon sends a string of the form "B{speed}\n" at a predefined
 * baud rate. This string indicates the user's actual speed.
 * The routine below returns the terminal type mapped from derived speed.
 */
struct	portselect {
	const char	*ps_baud;
	const char	*ps_type;
} portspeeds[] = {
	{ "B110",	"std.110" },
	{ "B134",	"std.134" },
	{ "B150",	"std.150" },
	{ "B300",	"std.300" },
	{ "B600",	"std.600" },
	{ "B1200",	"std.1200" },
	{ "B2400",	"std.2400" },
	{ "B4800",	"std.4800" },
	{ "B9600",	"std.9600" },
	{ "B19200",	"std.19200" },
	{ 0 }
};

const char *
portselector()
{
	char c, baud[20];
	const char *type = "default";
	register struct portselect *ps;
	int len;

	alarm(5*60);
	for (len = 0; len < sizeof (baud) - 1; len++) {
		if (read(STDIN_FILENO, &c, 1) <= 0)
			break;
		c &= 0177;
		if (c == '\n' || c == '\r')
			break;
		if (c == 'B')
			len = 0;	/* in case of leading garbage */
		baud[len] = c;
	}
	baud[len] = '\0';
	for (ps = portspeeds; ps->ps_baud; ps++)
		if (strcmp(ps->ps_baud, baud) == 0) {
			type = ps->ps_type;
			break;
		}
	sleep(2);	/* wait for connection to complete */
	return (type);
}

/*
 * This auto-baud speed select mechanism is written for the Micom 600
 * portselector. Selection is done by looking at how the character '\r'
 * is garbled at the different speeds.
 */
#include <sys/time.h>

const char *
autobaud()
{
	int rfds;
	struct timeval timeout;
	char c;
	const char *type = "9600-baud";

	(void)tcflush(0, TCIOFLUSH);
	rfds = 1 << 0;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if (select(32, (fd_set *)&rfds, (fd_set *)NULL,
	    (fd_set *)NULL, &timeout) <= 0)
		return (type);
	if (read(STDIN_FILENO, &c, sizeof(char)) != sizeof(char))
		return (type);
	timeout.tv_sec = 0;
	timeout.tv_usec = 20;
	(void) select(32, (fd_set *)NULL, (fd_set *)NULL,
	    (fd_set *)NULL, &timeout);
	(void)tcflush(0, TCIOFLUSH);
	switch (c & 0377) {

	case 0200:		/* 300-baud */
		type = "300-baud";
		break;

	case 0346:		/* 1200-baud */
		type = "1200-baud";
		break;

	case  015:		/* 2400-baud */
	case 0215:
		type = "2400-baud";
		break;

	default:		/* 4800-baud */
		type = "4800-baud";
		break;

	case 0377:		/* 9600-baud */
		type = "9600-baud";
		break;
	}
	return (type);
}
