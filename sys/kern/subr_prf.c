/*-
 * Copyright (c) 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)subr_prf.c	8.3 (Berkeley) 1/21/94
 * $Id: subr_prf.c,v 1.41 1997/02/22 09:39:17 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/tprintf.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <machine/cons.h>

/*
 * Note that stdarg.h and the ANSI style va_start macro is used for both
 * ANSI and traditional C compilers.
 */
#include <machine/stdarg.h>

#define TOCONS	0x01
#define TOTTY	0x02
#define TOLOG	0x04

struct	tty *constty;			/* pointer to console "window" tty */

static void (*v_putc)(int) = cnputc;	/* routine to putc on virtual console */
static void  logpri __P((int level));
static void  msglogchar(int c, void *dummyarg);
struct putchar_arg {int flags; struct tty *tty; };
static void  putchar __P((int ch, void *arg));
static char *ksprintn __P((u_long num, int base, int *len));

static int consintr = 1;		/* Ok to handle console interrupts? */

/*
 * Warn that a system table is full.
 */
void
tablefull(tab)
	const char *tab;
{

	log(LOG_ERR, "%s: table is full\n", tab);
}

/*
 * Uprintf prints to the controlling terminal for the current process.
 * It may block if the tty queue is overfull.  No message is printed if
 * the queue does not clear in a reasonable time.
 */
void
uprintf(const char *fmt, ...)
{
	struct proc *p = curproc;
	va_list ap;
	struct putchar_arg pca;

	if (p->p_flag & P_CONTROLT && p->p_session->s_ttyvp) {
		va_start(ap, fmt);
		pca.tty = p->p_session->s_ttyp;
		pca.flags = TOTTY;
		kvprintf(fmt, putchar, &pca, 10, ap);
		va_end(ap);
	}
}

tpr_t
tprintf_open(p)
	register struct proc *p;
{

	if (p->p_flag & P_CONTROLT && p->p_session->s_ttyvp) {
		SESSHOLD(p->p_session);
		return ((tpr_t) p->p_session);
	}
	return ((tpr_t) NULL);
}

void
tprintf_close(sess)
	tpr_t sess;
{

	if (sess)
		SESSRELE((struct session *) sess);
}

/*
 * tprintf prints on the controlling terminal associated
 * with the given session.
 */
void
tprintf(tpr_t tpr, const char *fmt, ...)
{
	register struct session *sess = (struct session *)tpr;
	struct tty *tp = NULL;
	int flags = TOLOG;
	va_list ap;
	struct putchar_arg pca;

	logpri(LOG_INFO);
	if (sess && sess->s_ttyvp && ttycheckoutq(sess->s_ttyp, 0)) {
		flags |= TOTTY;
		tp = sess->s_ttyp;
	}
	va_start(ap, fmt);
	pca.tty = tp;
	pca.flags = flags;
	kvprintf(fmt, putchar, &pca, 10, ap);
	va_end(ap);
	logwakeup();
}

/*
 * Ttyprintf displays a message on a tty; it should be used only by
 * the tty driver, or anything that knows the underlying tty will not
 * be revoke(2)'d away.  Other callers should use tprintf.
 */
void
ttyprintf(struct tty *tp, const char *fmt, ...)
{
	va_list ap;
	struct putchar_arg pca;
	va_start(ap, fmt);
	pca.tty = tp;
	pca.flags = TOTTY;
	kvprintf(fmt, putchar, &pca, 10, ap);
	va_end(ap);
}

extern	int log_open;

/*
 * Log writes to the log buffer, and guarantees not to sleep (so can be
 * called by interrupt routines).  If there is no process reading the
 * log yet, it writes to the console also.
 */
void
log(int level, const char *fmt, ...)
{
	register int s;
	va_list ap;

	s = splhigh();
	logpri(level);
	va_start(ap, fmt);

	kvprintf(fmt, msglogchar, NULL, 10, ap);
	va_end(ap);

	splx(s);
	if (!log_open) {
		struct putchar_arg pca;
		va_start(ap, fmt);
		pca.tty = NULL;
		pca.flags = TOCONS;
		kvprintf(fmt, putchar, &pca, 10, ap);
		va_end(ap);
	}
	logwakeup();
}

static void
logpri(level)
	int level;
{
	register char *p;

	msglogchar('<', NULL);
	for (p = ksprintn((u_long)level, 10, NULL); *p;)
		msglogchar(*p--, NULL);
	msglogchar('>', NULL);
}

int
addlog(const char *fmt, ...)
{
	register int s;
	va_list ap;
	int retval;

	s = splhigh();
	va_start(ap, fmt);
	retval = kvprintf(fmt, msglogchar, NULL, 10, ap);
	splx(s);
	va_end(ap);
	if (!log_open) {
		struct putchar_arg pca;
		va_start(ap, fmt);
		pca.tty = NULL;
		pca.flags = TOCONS;
		kvprintf(fmt, putchar, &pca, 10, ap);
		va_end(ap);
	}
	logwakeup();
	return (retval);
}

int
printf(const char *fmt, ...)
{
	va_list ap;
	register int savintr;
	struct putchar_arg pca;
	int retval;

	savintr = consintr;		/* disable interrupts */
	consintr = 0;
	va_start(ap, fmt);
	pca.tty = NULL;
	pca.flags = TOCONS | TOLOG;
	retval = kvprintf(fmt, putchar, &pca, 10, ap);
	va_end(ap);
	if (!panicstr)
		logwakeup();
	consintr = savintr;		/* reenable interrupts */
	return retval;
}

void
vprintf(const char *fmt, va_list ap)
{
	register int savintr;
	struct putchar_arg pca;

	savintr = consintr;		/* disable interrupts */
	consintr = 0;
	pca.tty = NULL;
	pca.flags = TOCONS | TOLOG;
	kvprintf(fmt, putchar, &pca, 10, ap);
	if (!panicstr)
		logwakeup();
	consintr = savintr;		/* reenable interrupts */
}

/*
 * Print a character on console or users terminal.  If destination is
 * the console then the last MSGBUFS characters are saved in msgbuf for
 * inspection later.
 */
static void
putchar(int c, void *arg)
{
	struct putchar_arg *ap = (struct putchar_arg*) arg;
	int flags = ap->flags;
	struct tty *tp = ap->tty;
	if (panicstr)
		constty = NULL;
	if ((flags & TOCONS) && tp == NULL && constty) {
		tp = constty;
		flags |= TOTTY;
	}
	if ((flags & TOTTY) && tp && tputchar(c, tp) < 0 &&
	    (flags & TOCONS) && tp == constty)
		constty = NULL;
	if ((flags & TOLOG))
		msglogchar(c, NULL);
	if ((flags & TOCONS) && constty == NULL && c != '\0')
		(*v_putc)(c);
}

/*
 * Scaled down version of sprintf(3).
 */
int
sprintf(char *buf, const char *cfmt, ...)
{
	int retval;
	va_list ap;

	va_start(ap, cfmt);
	retval = kvprintf(cfmt, NULL, (void *)buf, 10, ap);
	buf[retval] = '\0';
	va_end(ap);
	return retval;
}

/*
 * Put a number (base <= 16) in a buffer in reverse order; return an
 * optional length and a pointer to the NULL terminated (preceded?)
 * buffer.
 */
static char *
ksprintn(ul, base, lenp)
	register u_long ul;
	register int base, *lenp;
{					/* A long in base 8, plus NULL. */
	static char buf[sizeof(long) * NBBY / 3 + 2];
	register char *p;

	p = buf;
	do {
		*++p = hex2ascii(ul % base);
	} while (ul /= base);
	if (lenp)
		*lenp = p - buf;
	return (p);
}

/*
 * Scaled down version of printf(3).
 *
 * Two additional formats:
 *
 * The format %b is supported to decode error registers.
 * Its usage is:
 *
 *	printf("reg=%b\n", regval, "<base><arg>*");
 *
 * where <base> is the output base expressed as a control character, e.g.
 * \10 gives octal; \20 gives hex.  Each arg is a sequence of characters,
 * the first of which gives the bit number to be inspected (origin 1), and
 * the next characters (up to a control character, i.e. a character <= 32),
 * give the name of the register.  Thus:
 *
 *	kvprintf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 *
 * would produce output:
 *
 *	reg=3<BITTWO,BITONE>
 *
 * XXX:  %D  -- Hexdump, takes pointer and separator string:
 *		("%6D", ptr, ":")   -> XX:XX:XX:XX:XX:XX
 *		("%*D", len, ptr, " " -> XX XX XX XX ...
 */
int
kvprintf(char const *fmt, void (*func)(int, void*), void *arg, int radix, va_list ap)
{
#define PCHAR(c) {int cc=(c); if (func) (*func)(cc,arg); else *d++ = cc; retval++; }
	char *p, *q, *d;
	u_char *up;
	int ch, n;
	u_long ul;
	int base, lflag, tmp, width, ladjust, sharpflag, neg, sign, dot;
	int dwidth;
	char padc;
	int retval = 0;

	if (!func)
		d = (char *) arg;
	else
		d = NULL;

	if (fmt == NULL)
		fmt = "(fmt null)\n";

	if (radix < 2 || radix > 36)
		radix = 10;

	for (;;) {
		padc = ' ';
		width = 0;
		while ((ch = (u_char)*fmt++) != '%') {
			if (ch == '\0') 
				return retval;
			PCHAR(ch);
		}
		lflag = 0; ladjust = 0; sharpflag = 0; neg = 0;
		sign = 0; dot = 0; dwidth = 0;
reswitch:	switch (ch = (u_char)*fmt++) {
		case '.':
			dot = 1;
			goto reswitch;
		case '#':
			sharpflag = 1;
			goto reswitch;
		case '+':
			sign = 1;
			goto reswitch;
		case '-':
			ladjust = 1;
			goto reswitch;
		case '%':
			PCHAR(ch);
			break;
		case '*':
			if (!dot) {
				width = va_arg(ap, int);
				if (width < 0) {
					ladjust = !ladjust;
					width = -width;
				}
			} else {
				dwidth = va_arg(ap, int);
			}
			goto reswitch;
		case '0':
			if (!dot) {
				padc = '0';
				goto reswitch;
			}
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
				for (n = 0;; ++fmt) {
					n = n * 10 + ch - '0';
					ch = *fmt;
					if (ch < '0' || ch > '9')
						break;
				}
			if (dot)
				dwidth = n;
			else
				width = n;
			goto reswitch;
		case 'b':
			ul = va_arg(ap, int);
			p = va_arg(ap, char *);
			for (q = ksprintn(ul, *p++, NULL); *q;)
				PCHAR(*q--);

			if (!ul)
				break;

			for (tmp = 0; *p;) {
				n = *p++;
				if (ul & (1 << (n - 1))) {
					PCHAR(tmp ? ',' : '<');
					for (; (n = *p) > ' '; ++p)
						PCHAR(n);
					tmp = 1;
				} else
					for (; *p > ' '; ++p)
						continue;
			}
			if (tmp)
				PCHAR('>');
			break;
		case 'c':
			PCHAR(va_arg(ap, int));
			break;
		case 'D':
			up = va_arg(ap, u_char *);
			p = va_arg(ap, char *);
			if (!width)
				width = 16;
			while(width--) {
				PCHAR(hex2ascii(*up >> 4));
				PCHAR(hex2ascii(*up & 0x0f));
				up++;
				if (width)
					for (q=p;*q;q++)
						PCHAR(*q);
			}
			break;
		case 'd':
			ul = lflag ? va_arg(ap, long) : va_arg(ap, int);
			sign = 1;
			base = 10;
			goto number;
		case 'l':
			lflag = 1;
			goto reswitch;
		case 'n':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = radix;
			goto number;
		case 'o':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 8;
			goto number;
		case 'p':
			ul = (u_long)va_arg(ap, void *);
			base = 16;
			PCHAR('0');
			PCHAR('x');
			goto number;
		case 's':
			p = va_arg(ap, char *);
			if (p == NULL)
				p = "(null)";
			if (!dot)
				n = strlen (p);
			else
				for (n = 0; n < dwidth && p[n]; n++)
					continue;

			width -= n;

			if (!ladjust && width > 0)
				while (width--)
					PCHAR(padc);
			while (n--)
				PCHAR(*p++);
			if (ladjust && width > 0)
				while (width--)
					PCHAR(padc);
			break;
		case 'u':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 10;
			goto number;
		case 'x':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 16;
number:			if (sign && (long)ul < 0L) {
				neg = 1;
				ul = -(long)ul;
			}
			p = ksprintn(ul, base, &tmp);
			if (sharpflag && ul != 0) {
				if (base == 8)
					tmp++;
				else if (base == 16)
					tmp += 2;
			}
			if (neg)
				tmp++;

			if (!ladjust && width && (width -= tmp) > 0)
				while (width--)
					PCHAR(padc);
			if (neg)
				PCHAR('-');
			if (sharpflag && ul != 0) {
				if (base == 8) {
					PCHAR('0');
				} else if (base == 16) {
					PCHAR('0');
					PCHAR('x');
				}
			}

			while (*p)
				PCHAR(*p--);

			if (ladjust && width && (width -= tmp) > 0)
				while (width--)
					PCHAR(padc);

			break;
		default:
			PCHAR('%');
			if (lflag)
				PCHAR('l');
			PCHAR(ch);
			break;
		}
	}
#undef PCHAR
}

/*
 * Put character in log buffer.
 */
static void
msglogchar(int c, void *dummyarg)
{
	struct msgbuf *mbp;

	if (c != '\0' && c != '\r' && c != 0177 && msgbufmapped) {
		mbp = msgbufp;
		if (mbp->msg_magic != MSG_MAGIC ||
		    mbp->msg_bufx >= MSG_BSIZE ||
		    mbp->msg_bufr >= MSG_BSIZE) {
			bzero(mbp, sizeof(struct msgbuf));
			mbp->msg_magic = MSG_MAGIC;
		}
		mbp->msg_bufc[mbp->msg_bufx++] = c;
		if (mbp->msg_bufx >= MSG_BSIZE)
			mbp->msg_bufx = 0;
		/* If the buffer is full, keep the most recent data. */
		if (mbp->msg_bufr == mbp->msg_bufx) {
			if (++mbp->msg_bufr >= MSG_BSIZE)
				mbp->msg_bufr = 0;
		}
	}
}
