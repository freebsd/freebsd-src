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
 * $Id: subr_prf.c,v 1.18 1995/08/24 12:54:11 davidg Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/vnode.h>
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

#ifdef KADB
#include <machine/kdbparam.h>
#endif


#define TOCONS	0x01
#define TOTTY	0x02
#define TOLOG	0x04

struct	tty *constty;			/* pointer to console "window" tty */

static void (*v_putc)(int) = cnputc;	/* routine to putc on virtual console */

static void  logpri __P((int level));
static void  putchar __P((int ch, int flags, struct tty *tp));
static char *ksprintn __P((u_long num, int base, int *len));

static int consintr = 1;		/* Ok to handle console interrupts? */

/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.  It prints "panic: mesg",
 * and then reboots.  If we are called twice, then we avoid trying to sync
 * the disks as this often leads to recursive panics.
 */
#ifdef __GNUC__
__dead			/* panic() does not return */
#endif
void
#ifdef __STDC__
panic(const char *fmt, ...)
#else
panic(fmt, va_alist)
	char *fmt;
#endif
{
	int bootopt;
	va_list ap;

	bootopt = RB_AUTOBOOT | RB_DUMP;
	if (panicstr)
		bootopt |= RB_NOSYNC;
	else
		panicstr = fmt;

	va_start(ap, fmt);
	printf("panic: %r\n", fmt, ap);
	va_end(ap);

#ifdef KGDB
	kgdb_panic();
#endif
#ifdef KADB
	if (boothowto & RB_KDB)
		kdbpanic();
#endif
#ifdef DDB
	Debugger ("panic");
#endif
	boot(bootopt);
}

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
#ifdef __STDC__
uprintf(const char *fmt, ...)
#else
uprintf(fmt, va_alist)
	char *fmt;
#endif
{
	register struct proc *p = curproc;
	va_list ap;

	if (p->p_flag & P_CONTROLT && p->p_session->s_ttyvp) {
		va_start(ap, fmt);
		kprintf(fmt, TOTTY, p->p_session->s_ttyp, ap);
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
#ifdef __STDC__
tprintf(tpr_t tpr, const char *fmt, ...)
#else
tprintf(tpr, fmt, va_alist)
	tpr_t tpr;
	char *fmt;
#endif
{
	register struct session *sess = (struct session *)tpr;
	struct tty *tp = NULL;
	int flags = TOLOG;
	va_list ap;

	logpri(LOG_INFO);
	if (sess && sess->s_ttyvp && ttycheckoutq(sess->s_ttyp, 0)) {
		flags |= TOTTY;
		tp = sess->s_ttyp;
	}
	va_start(ap, fmt);
	kprintf(fmt, flags, tp, ap);
	va_end(ap);
	logwakeup();
}

/*
 * Ttyprintf displays a message on a tty; it should be used only by
 * the tty driver, or anything that knows the underlying tty will not
 * be revoke(2)'d away.  Other callers should use tprintf.
 */
void
#ifdef __STDC__
ttyprintf(struct tty *tp, const char *fmt, ...)
#else
ttyprintf(tp, fmt, va_alist)
	struct tty *tp;
	char *fmt;
#endif
{
	va_list ap;

	va_start(ap, fmt);
	kprintf(fmt, TOTTY, tp, ap);
	va_end(ap);
}

extern	int log_open;

/*
 * Log writes to the log buffer, and guarantees not to sleep (so can be
 * called by interrupt routines).  If there is no process reading the
 * log yet, it writes to the console also.
 */
void
#ifdef __STDC__
log(int level, const char *fmt, ...)
#else
log(level, fmt, va_alist)
	int level;
	char *fmt;
#endif
{
	register int s;
	va_list ap;

	s = splhigh();
	logpri(level);
	va_start(ap, fmt);
	kprintf(fmt, TOLOG, NULL, ap);
	splx(s);
	va_end(ap);
	if (!log_open) {
		va_start(ap, fmt);
		kprintf(fmt, TOCONS, NULL, ap);
		va_end(ap);
	}
	logwakeup();
}

static void
logpri(level)
	int level;
{
	register char *p;

	putchar('<', TOLOG, NULL);
	for (p = ksprintn((u_long)level, 10, NULL); *p;)
		putchar(*p--, TOLOG, NULL);
	putchar('>', TOLOG, NULL);
}

void
#ifdef __STDC__
addlog(const char *fmt, ...)
#else
addlog(fmt, va_alist)
	char *fmt;
#endif
{
	register int s;
	va_list ap;

	s = splhigh();
	va_start(ap, fmt);
	kprintf(fmt, TOLOG, NULL, ap);
	splx(s);
	va_end(ap);
	if (!log_open) {
		va_start(ap, fmt);
		kprintf(fmt, TOCONS, NULL, ap);
		va_end(ap);
	}
	logwakeup();
}

void
#ifdef __STDC__
printf(const char *fmt, ...)
#else
printf(fmt, va_alist)
	char *fmt;
#endif
{
	va_list ap;
	register int savintr;

	savintr = consintr;		/* disable interrupts */
	consintr = 0;
	va_start(ap, fmt);
	kprintf(fmt, TOCONS | TOLOG, NULL, ap);
	va_end(ap);
	if (!panicstr)
		logwakeup();
	consintr = savintr;		/* reenable interrupts */
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
 *	kprintf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 *
 * would produce output:
 *
 *	reg=3<BITTWO,BITONE>
 *
 * The format %r passes an additional format string and argument list
 * recursively.  Its usage is:
 *
 * fn(char *fmt, ...)
 * {
 *	va_list ap;
 *	va_start(ap, fmt);
 *	printf("prefix: %r: suffix\n", fmt, ap);
 *	va_end(ap);
 * }
 *
 * Space or zero padding and a field width are supported for the numeric
 * formats only.
 */
void
kprintf(fmt, flags, tp, ap)
	register const char *fmt;
	int flags;
	struct tty *tp;
	va_list ap;
{
	register char *p, *q;
	register int ch, n;
	u_long ul;
	int base, lflag, tmp, width;
	char padc;

	if (fmt == NULL)
		fmt = "(fmt null)\n";
	for (;;) {
		padc = ' ';
		width = 0;
		while ((ch = *(u_char *)fmt++) != '%') {
			if (ch == '\0')
				return;
			putchar(ch, flags, tp);
		}
		lflag = 0;
reswitch:	switch (ch = *(u_char *)fmt++) {
		case '0':
			padc = '0';
			goto reswitch;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			for (width = 0;; ++fmt) {
				width = width * 10 + ch - '0';
				ch = *fmt;
				if (ch < '0' || ch > '9')
					break;
			}
			goto reswitch;
		case 'l':
			lflag = 1;
			goto reswitch;
		case 'b':
			ul = va_arg(ap, int);
			p = va_arg(ap, char *);
			for (q = ksprintn(ul, *p++, NULL); *q;)
				putchar(*q--, flags, tp);

			if (!ul)
				break;

			for (tmp = 0; *p;) {
				n = *p++;
				if (ul & (1 << (n - 1))) {
					putchar(tmp ? ',' : '<', flags, tp);
					for (; (n = *p) > ' '; ++p)
						putchar(n, flags, tp);
					tmp = 1;
				} else
					for (; *p > ' '; ++p)
						continue;
			}
			if (tmp)
				putchar('>', flags, tp);
			break;
		case 'c':
			putchar(va_arg(ap, int), flags, tp);
			break;
		case 'r':
			p = va_arg(ap, char *);
			kprintf(p, flags, tp, va_arg(ap, va_list));
			break;
		case 's':
			p = va_arg(ap, char *);
			if (p == NULL)
				p = "(null)";
			while (*p)
				putchar(*p++, flags, tp);
			break;
		case 'd':
			ul = lflag ? va_arg(ap, long) : va_arg(ap, int);
			if ((long)ul < 0) {
				putchar('-', flags, tp);
				ul = -(long)ul;
			}
			base = 10;
			goto number;
		case 'o':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 8;
			goto number;
		case 'p':
			ul = (u_long)va_arg(ap, void *);
			base = 16;
			putchar('0', flags, tp);
			putchar('x', flags, tp);
			goto number;
		case 'u':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 10;
			goto number;
		case 'x':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 16;
number:			p = ksprintn(ul, base, &tmp);
			if (width && (width -= tmp) > 0)
				while (width--)
					putchar(padc, flags, tp);
			while (*p)
				putchar(*p--, flags, tp);
			break;
		default:
			putchar('%', flags, tp);
			if (lflag)
				putchar('l', flags, tp);
			/* FALLTHROUGH */
		case '%':
			putchar(ch, flags, tp);
		}
	}
}

/*
 * Print a character on console or users terminal.  If destination is
 * the console then the last MSGBUFS characters are saved in msgbuf for
 * inspection later.
 */
static void
putchar(c, flags, tp)
	register int c;
	int flags;
	struct tty *tp;
{
	register struct msgbuf *mbp;

	if (panicstr)
		constty = NULL;
	if ((flags & TOCONS) && tp == NULL && constty) {
		tp = constty;
		flags |= TOTTY;
	}
	if ((flags & TOTTY) && tp && tputchar(c, tp) < 0 &&
	    (flags & TOCONS) && tp == constty)
		constty = NULL;
	if ((flags & TOLOG) &&
	    c != '\0' && c != '\r' && c != 0177 && msgbufmapped) {
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
	if ((flags & TOCONS) && constty == NULL && c != '\0')
		(*v_putc)(c);
}

/*
 * Scaled down version of sprintf(3).
 */
#ifdef __STDC__
int
sprintf(char *buf, const char *cfmt, ...)
#else
int
sprintf(buf, cfmt, va_alist)
	char *buf, *cfmt;
#endif
{
	register const char *fmt = cfmt;
	register char *p, *bp;
	register int ch, base;
	u_long ul;
	int lflag;
	va_list ap;

	va_start(ap, cfmt);
	for (bp = buf; ; ) {
		while ((ch = *(u_char *)fmt++) != '%')
			if ((*bp++ = ch) == '\0')
				return ((bp - buf) - 1);

		lflag = 0;
reswitch:	switch (ch = *(u_char *)fmt++) {
		case 'l':
			lflag = 1;
			goto reswitch;
		case 'c':
			*bp++ = va_arg(ap, int);
			break;
		case 's':
			p = va_arg(ap, char *);
			while (*p)
				*bp++ = *p++;
			break;
		case 'd':
			ul = lflag ? va_arg(ap, long) : va_arg(ap, int);
			if ((long)ul < 0) {
				*bp++ = '-';
				ul = -(long)ul;
			}
			base = 10;
			goto number;
			break;
		case 'o':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 8;
			goto number;
			break;
		case 'p':
			ul = (u_long)va_arg(ap, void *);
			base = 16;
			*bp++ = '0';
			*bp++ = 'x';
			goto number;
		case 'u':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 10;
			goto number;
			break;
		case 'x':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 16;
number:			for (p = ksprintn(ul, base, NULL); *p;)
				*bp++ = *p--;
			break;
		default:
			*bp++ = '%';
			if (lflag)
				*bp++ = 'l';
			/* FALLTHROUGH */
		case '%':
			*bp++ = ch;
		}
	}
	va_end(ap);
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
		*++p = "0123456789abcdef"[ul % base];
	} while (ul /= base);
	if (lenp)
		*lenp = p - buf;
	return (p);
}
