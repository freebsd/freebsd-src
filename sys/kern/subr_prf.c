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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/msgbuf.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/syslog.h>
#include <sys/cons.h>
#include <sys/uio.h>
#include <sys/sysctl.h>

/*
 * Note that stdarg.h and the ANSI style va_start macro is used for both
 * ANSI and traditional C compilers.
 */
#include <machine/stdarg.h>

#define TOCONS	0x01
#define TOTTY	0x02
#define TOLOG	0x04

/* Max number conversion buffer length: a u_quad_t in base 2, plus NUL byte. */
#define MAXNBUF	(sizeof(quad_t) * NBBY + 1)

struct putchar_arg {
	int	flags;
	int	pri;
	struct	tty *tty;
};

struct snprintf_arg {
	char	*str;
	size_t	remain;
};

extern	int log_open;

struct	tty *constty;			/* pointer to console "window" tty */

static void (*v_putc)(int) = cnputc;	/* routine to putc on virtual console */
static void  msglogchar(int c, int pri);
static void  msgaddchar(int c, void *dummy);
static void  putchar __P((int ch, void *arg));
static char *ksprintn __P((char *nbuf, u_long num, int base, int *len));
static char *ksprintqn __P((char *nbuf, u_quad_t num, int base, int *len));
static void  snprintf_func __P((int ch, void *arg));

static int consintr = 1;		/* Ok to handle console interrupts? */
static int msgbufmapped;		/* Set when safe to use msgbuf */
int msgbuftrigger;

/*
 * Warn that a system table is full.
 */
void
tablefull(const char *tab)
{

	log(LOG_ERR, "%s: table is full\n", tab);
}

/*
 * Uprintf prints to the controlling terminal for the current process.
 * It may block if the tty queue is overfull.  No message is printed if
 * the queue does not clear in a reasonable time.
 */
int
uprintf(const char *fmt, ...)
{
	struct proc *p = curproc;
	va_list ap;
	struct putchar_arg pca;
	int retval = 0;

	if (p && p != PCPU_GET(idleproc) && p->p_flag & P_CONTROLT &&
	    p->p_session->s_ttyvp) {
		va_start(ap, fmt);
		pca.tty = p->p_session->s_ttyp;
		pca.flags = TOTTY;
		retval = kvprintf(fmt, putchar, &pca, 10, ap);
		va_end(ap);
	}
	return retval;
}

/*
 * tprintf prints on the controlling terminal associated
 * with the given session, possibly to the log as well.
 */
void
tprintf(struct proc *p, int pri, const char *fmt, ...)
{
	struct tty *tp = NULL;
	int flags = 0, shld = 0;
	va_list ap;
	struct putchar_arg pca;
	int retval;

	if (pri != -1)
		flags |= TOLOG;
	if (p && p->p_flag & P_CONTROLT && p->p_session->s_ttyvp) {
		SESSHOLD(p->p_session);
		shld++;
		if (ttycheckoutq(p->p_session->s_ttyp, 0)) {
			flags |= TOTTY;
			tp = p->p_session->s_ttyp;
		}
	}
	pca.pri = pri;
	pca.tty = tp;
	pca.flags = flags;
	va_start(ap, fmt);
	retval = kvprintf(fmt, putchar, &pca, 10, ap);
	va_end(ap);
	if (shld)
		SESSRELE(p->p_session);
	msgbuftrigger = 1;
}

/*
 * Ttyprintf displays a message on a tty; it should be used only by
 * the tty driver, or anything that knows the underlying tty will not
 * be revoke(2)'d away.  Other callers should use tprintf.
 */
int
ttyprintf(struct tty *tp, const char *fmt, ...)
{
	va_list ap;
	struct putchar_arg pca;
	int retval;

	va_start(ap, fmt);
	pca.tty = tp;
	pca.flags = TOTTY;
	retval = kvprintf(fmt, putchar, &pca, 10, ap);
	va_end(ap);
	return retval;
}

/*
 * Log writes to the log buffer, and guarantees not to sleep (so can be
 * called by interrupt routines).  If there is no process reading the
 * log yet, it writes to the console also.
 */
void
log(int level, const char *fmt, ...)
{
	va_list ap;
	int retval;
	struct putchar_arg pca;

	pca.tty = NULL;
	pca.pri = level;
	pca.flags = log_open ? TOLOG : TOCONS;

	va_start(ap, fmt);
	retval = kvprintf(fmt, putchar, &pca, 10, ap);
	va_end(ap);

	msgbuftrigger = 1;
}

#define CONSCHUNK 128

void
log_console(struct uio *uio)
{
	int c, i, error, iovlen, nl;
	struct uio muio;
	struct iovec *miov = NULL;
	char *consbuffer;
	int pri;

	pri = LOG_INFO | LOG_CONSOLE;
	muio = *uio;
	iovlen = uio->uio_iovcnt * sizeof (struct iovec);
	MALLOC(miov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
	MALLOC(consbuffer, char *, CONSCHUNK, M_TEMP, M_WAITOK);
	bcopy((caddr_t)muio.uio_iov, (caddr_t)miov, iovlen);
	muio.uio_iov = miov;
	uio = &muio;

	nl = 0;
	while (uio->uio_resid > 0) {
		c = imin(uio->uio_resid, CONSCHUNK);
		error = uiomove(consbuffer, c, uio);
		if (error != 0)
			return;
		for (i = 0; i < c; i++) {
			msglogchar(consbuffer[i], pri);
			if (consbuffer[i] == '\n')
				nl = 1;
			else
				nl = 0;
		}
	}
	if (!nl)
		msglogchar('\n', pri);
	msgbuftrigger = 1;
	FREE(miov, M_TEMP);
	FREE(consbuffer, M_TEMP);
	return;
}

int
printf(const char *fmt, ...)
{
	va_list ap;
	int savintr;
	struct putchar_arg pca;
	int retval;

	savintr = consintr;		/* disable interrupts */
	consintr = 0;
	va_start(ap, fmt);
	pca.tty = NULL;
	pca.flags = TOCONS | TOLOG;
	pca.pri = -1;
	retval = kvprintf(fmt, putchar, &pca, 10, ap);
	va_end(ap);
	if (!panicstr)
		msgbuftrigger = 1;
	consintr = savintr;		/* reenable interrupts */
	return retval;
}

int
vprintf(const char *fmt, va_list ap)
{
	int savintr;
	struct putchar_arg pca;
	int retval;

	savintr = consintr;		/* disable interrupts */
	consintr = 0;
	pca.tty = NULL;
	pca.flags = TOCONS | TOLOG;
	pca.pri = -1;
	retval = kvprintf(fmt, putchar, &pca, 10, ap);
	if (!panicstr)
		msgbuftrigger = 1;
	consintr = savintr;		/* reenable interrupts */
	return retval;
}

/*
 * Print a character on console or users terminal.  If destination is
 * the console then the last bunch of characters are saved in msgbuf for
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
		msglogchar(c, ap->pri);
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
 * Scaled down version of vsprintf(3).
 */
int
vsprintf(char *buf, const char *cfmt, va_list ap)
{
	int retval;

	retval = kvprintf(cfmt, NULL, (void *)buf, 10, ap);
	buf[retval] = '\0';
	return retval;
}

/*
 * Scaled down version of snprintf(3).
 */
int
snprintf(char *str, size_t size, const char *format, ...)
{
	int retval;
	va_list ap;

	va_start(ap, format);
	retval = vsnprintf(str, size, format, ap);
	va_end(ap);
	return(retval);
}

/*
 * Scaled down version of vsnprintf(3).
 */
int
vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	struct snprintf_arg info;
	int retval;

	info.str = str;
	info.remain = size;
	retval = kvprintf(format, snprintf_func, &info, 10, ap);
	if (info.remain >= 1)
		*info.str++ = '\0';
	return retval;
}

static void
snprintf_func(int ch, void *arg)
{
	struct snprintf_arg *const info = arg;

	if (info->remain >= 2) {
		*info->str++ = ch;
		info->remain--;
	}
}

/*
 * Put a NUL-terminated ASCII number (base <= 36) in a buffer in reverse
 * order; return an optional length and a pointer to the last character
 * written in the buffer (i.e., the first character of the string).
 * The buffer pointed to by `nbuf' must have length >= MAXNBUF.
 */
static char *
ksprintn(nbuf, ul, base, lenp)
	char *nbuf;
	u_long ul;
	int base, *lenp;
{
	char *p;

	p = nbuf;
	*p = '\0';
	do {
		*++p = hex2ascii(ul % base);
	} while (ul /= base);
	if (lenp)
		*lenp = p - nbuf;
	return (p);
}
/* ksprintn, but for a quad_t. */
static char *
ksprintqn(nbuf, uq, base, lenp)
	char *nbuf;
	u_quad_t uq;
	int base, *lenp;
{
	char *p;

	p = nbuf;
	*p = '\0';
	do {
		*++p = hex2ascii(uq % base);
	} while (uq /= base);
	if (lenp)
		*lenp = p - nbuf;
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
	char nbuf[MAXNBUF];
	char *p, *q, *d;
	u_char *up;
	int ch, n;
	u_long ul;
	u_quad_t uq;
	int base, lflag, qflag, tmp, width, ladjust, sharpflag, neg, sign, dot;
	int dwidth;
	char padc;
	int retval = 0;

	ul = 0;
	uq = 0;
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
		qflag = 0; lflag = 0; ladjust = 0; sharpflag = 0; neg = 0;
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
			for (q = ksprintn(nbuf, ul, *p++, NULL); *q;)
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
			if (qflag)
				uq = va_arg(ap, quad_t);
			else if (lflag)
				ul = va_arg(ap, long);
			else
				ul = va_arg(ap, int);
			sign = 1;
			base = 10;
			goto number;
		case 'l':
			if (lflag) {
				lflag = 0;
				qflag = 1;
			} else
				lflag = 1;
			goto reswitch;
		case 'o':
			if (qflag)
				uq = va_arg(ap, u_quad_t);
			else if (lflag)
				ul = va_arg(ap, u_long);
			else
				ul = va_arg(ap, u_int);
			base = 8;
			goto nosign;
		case 'p':
			ul = (uintptr_t)va_arg(ap, void *);
			base = 16;
			sharpflag = (width == 0);
			goto nosign;
		case 'q':
			qflag = 1;
			goto reswitch;
		case 'n':
		case 'r':
			if (qflag)
				uq = va_arg(ap, u_quad_t);
			else if (lflag)
				ul = va_arg(ap, u_long);
			else
				ul = sign ?
				    (u_long)va_arg(ap, int) : va_arg(ap, u_int);
			base = radix;
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
			if (qflag)
				uq = va_arg(ap, u_quad_t);
			else if (lflag)
				ul = va_arg(ap, u_long);
			else
				ul = va_arg(ap, u_int);
			base = 10;
			goto nosign;
		case 'x':
		case 'X':
			if (qflag)
				uq = va_arg(ap, u_quad_t);
			else if (lflag)
				ul = va_arg(ap, u_long);
			else
				ul = va_arg(ap, u_int);
			base = 16;
			goto nosign;
		case 'z':
			if (qflag)
				uq = va_arg(ap, u_quad_t);
			else if (lflag)
				ul = va_arg(ap, u_long);
			else
				ul = sign ?
				    (u_long)va_arg(ap, int) : va_arg(ap, u_int);
			base = 16;
			goto number;
nosign:			sign = 0;
number:			
			if (qflag) {
				if (sign && (quad_t)uq < 0) {
					neg = 1;
					uq = -(quad_t)uq;
				}
				p = ksprintqn(nbuf, uq, base, &tmp);
			} else {
				if (sign && (long)ul < 0) {
					neg = 1;
					ul = -(long)ul;
				}
				p = ksprintn(nbuf, ul, base, &tmp);
			}
			if (sharpflag && (qflag ? uq != 0 : ul != 0)) {
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
			if (sharpflag && (qflag ? uq != 0 : ul != 0)) {
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
 * Put character in log buffer with a particular priority.
 */
static void
msglogchar(int c, int pri)
{
	static int lastpri = -1;
	static int dangling;
	char nbuf[MAXNBUF];
	char *p;

	if (!msgbufmapped)
		return;
	if (c == '\0' || c == '\r')
		return;
	if (pri != -1 && pri != lastpri) {
		if (dangling) {
			msgaddchar('\n', NULL);
			dangling = 0;
		}
		msgaddchar('<', NULL);
		for (p = ksprintn(nbuf, (u_long)pri, 10, NULL); *p;)
			msgaddchar(*p--, NULL);
		msgaddchar('>', NULL);
		lastpri = pri;
	}
	msgaddchar(c, NULL);
	if (c == '\n') {
		dangling = 0;
		lastpri = -1;
	} else {
		dangling = 1;
	}
}

/*
 * Put char in log buffer
 */
static void
msgaddchar(int c, void *dummy)
{
	struct msgbuf *mbp;

	if (!msgbufmapped)
		return;
	mbp = msgbufp;
	mbp->msg_ptr[mbp->msg_bufx++] = c;
	if (mbp->msg_bufx >= mbp->msg_size)
		mbp->msg_bufx = 0;
	/* If the buffer is full, keep the most recent data. */
	if (mbp->msg_bufr == mbp->msg_bufx) {
		if (++mbp->msg_bufr >= mbp->msg_size)
			mbp->msg_bufr = 0;
	}
}

static void
msgbufcopy(struct msgbuf *oldp)
{
	int pos;

	pos = oldp->msg_bufr;
	while (pos != oldp->msg_bufx) {
		msglogchar(oldp->msg_ptr[pos], -1);
		if (++pos >= oldp->msg_size)
			pos = 0;
	}
}

void
msgbufinit(void *ptr, size_t size)
{
	char *cp;
	static struct msgbuf *oldp = NULL;

	cp = (char *)ptr;
	msgbufp = (struct msgbuf *) (cp + size - sizeof(*msgbufp));
	if (msgbufp->msg_magic != MSG_MAGIC || msgbufp->msg_ptr != cp) {
		bzero(cp, size);
		msgbufp->msg_magic = MSG_MAGIC;
		msgbufp->msg_size = (char *)msgbufp - cp;
		msgbufp->msg_ptr = cp;
	}
	if (msgbufmapped && oldp != msgbufp)
		msgbufcopy(oldp);
	msgbufmapped = 1;
	oldp = msgbufp;
}

/* Sysctls for accessing/clearing the msgbuf */
static int
sysctl_kern_msgbuf(SYSCTL_HANDLER_ARGS)
{
	int error;

	/*
	 * Unwind the buffer, so that it's linear (possibly starting with
	 * some initial nulls).
	 */
	error = sysctl_handle_opaque(oidp, msgbufp->msg_ptr + msgbufp->msg_bufx,
	    msgbufp->msg_size - msgbufp->msg_bufx, req);
	if (error)
		return (error);
	if (msgbufp->msg_bufx > 0) {
		error = sysctl_handle_opaque(oidp, msgbufp->msg_ptr,
		    msgbufp->msg_bufx, req);
	}
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, msgbuf, CTLTYPE_STRING | CTLFLAG_RD,
    0, 0, sysctl_kern_msgbuf, "A", "Contents of kernel message buffer");

static int msgbuf_clear;

static int
sysctl_kern_msgbuf_clear(SYSCTL_HANDLER_ARGS)
{
	int error;
	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
	if (!error && req->newptr) {
		/* Clear the buffer and reset write pointer */
		bzero(msgbufp->msg_ptr, msgbufp->msg_size);
		msgbufp->msg_bufr = msgbufp->msg_bufx = 0;
		msgbuf_clear = 0;
	}
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, msgbuf_clear,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE, &msgbuf_clear, 0, 
    sysctl_kern_msgbuf_clear, "I", "Clear kernel message buffer");

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(msgbuf, db_show_msgbuf)
{
	int i, j;

	if (!msgbufmapped) {
		db_printf("msgbuf not mapped yet\n");
		return;
	}
	db_printf("msgbufp = %p\n", msgbufp);
	db_printf("magic = %x, size = %d, r= %d, w = %d, ptr = %p\n",
	    msgbufp->msg_magic, msgbufp->msg_size, msgbufp->msg_bufr,
	    msgbufp->msg_bufx, msgbufp->msg_ptr);
	for (i = 0; i < msgbufp->msg_size; i++) {
		j = (i + msgbufp->msg_bufr) % msgbufp->msg_size;
		db_printf("%c", msgbufp->msg_ptr[j]);
	}
	db_printf("\n");
}

#endif /* DDB */
