/*	$OpenBSD: courier.c,v 1.9 2001/10/24 18:38:58 millert Exp $	*/
/*	$NetBSD: courier.c,v 1.7 1997/02/11 09:24:16 mrg Exp $	*/

/*
 * Copyright (c) 1986, 1993
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
#if 0
static char sccsid[] = "@(#)courier.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: courier.c,v 1.9 2001/10/24 18:38:58 millert Exp $";
#endif /* not lint */

/*
 * Routines for calling up on a Courier modem.
 * Derived from Hayes driver.
 */
#include "tip.h"
#include <sys/ioctl.h>
#include <stdio.h>

#define	MAXRETRY	5

static	void sigALRM();
static	int timeout = 0;
static	int connected = 0;
static	jmp_buf timeoutbuf, intbuf;
static	int coursync(), cour_connect(), cour_swallow();
void	cour_nap();

void cour_disconnect __P((void));

int
cour_dialer(num, acu)
	char *num;
	char *acu;
{
	char *cp;
#ifdef ACULOG
	char line[80];
#endif
	struct termios cntrl;

	if (boolean(value(VERBOSE)))
		printf("Using \"%s\"\n", acu);

	tcgetattr(FD, &cntrl);
	cntrl.c_cflag |= HUPCL;
	tcsetattr(FD, TCSAFLUSH, &cntrl);
	/*
	 * Get in synch.
	 */
	if (!coursync()) {
badsynch:
		printf("can't synchronize with courier\n");
#ifdef ACULOG
		logent(value(HOST), num, "courier", "can't synch up");
#endif
		return (0);
	}
	cour_write(FD, "AT E0\r", 6);	/* turn off echoing */
	sleep(1);
#ifdef DEBUG
	if (boolean(value(VERBOSE)))
		cour_verbose_read();
#endif
	tcflush(FD, TCIOFLUSH);
	cour_write(FD, "AT C1 E0 H0 Q0 X6 V1\r", 21);
	if (!cour_swallow("\r\nOK\r\n"))
		goto badsynch;
	fflush(stdout);
	cour_write(FD, "AT D", 4);
	for (cp = num; *cp; cp++)
		if (*cp == '=')
			*cp = ',';
	cour_write(FD, num, strlen(num));
	cour_write(FD, "\r", 1);
	connected = cour_connect();
#ifdef ACULOG
	if (timeout) {
		(void)sprintf(line, "%ld second dial timeout",
			number(value(DIALTIMEOUT)));
		logent(value(HOST), num, "cour", line);
	}
#endif
	if (timeout)
		cour_disconnect();
	return (connected);
}

void
cour_disconnect()
{
	 /* first hang up the modem*/
	ioctl(FD, TIOCCDTR, 0);
	sleep(1);
	ioctl(FD, TIOCSDTR, 0);
	coursync();				/* reset */
	close(FD);
}

void
cour_abort()
{
	cour_write(FD, "\r", 1);	/* send anything to abort the call */
	cour_disconnect();
}

static void
sigALRM()
{
	printf("\07timeout waiting for reply\n");
	timeout = 1;
	longjmp(timeoutbuf, 1);
}

static int
cour_swallow(match)
	char *match;
{
	sig_t f;
	char c;

	f = signal(SIGALRM, sigALRM);
	timeout = 0;
	do {
		if (*match =='\0') {
			signal(SIGALRM, f);
			return (1);
		}
		if (setjmp(timeoutbuf)) {
			signal(SIGALRM, f);
			return (0);
		}
		alarm(number(value(DIALTIMEOUT)));
		read(FD, &c, 1);
		alarm(0);
		c &= 0177;
#ifdef DEBUG
		if (boolean(value(VERBOSE)))
			putchar(c);
#endif
	} while (c == *match++);
#ifdef DEBUG
	if (boolean(value(VERBOSE)))
		fflush(stdout);
#endif
	signal(SIGALRM, SIG_DFL);
	return (0);
}

struct baud_msg {
	char *msg;
	int baud;
} baud_msg[] = {
	"",		B300,
	" 1200",	B1200,
	" 2400",	B2400,
	" 9600",	B9600,
	" 9600/ARQ",	B9600,
	0,		0,
};

static int
cour_connect()
{
	char c;
	int nc, nl, n;
	char dialer_buf[64];
	struct baud_msg *bm;
	sig_t f;

	if (cour_swallow("\r\n") == 0)
		return (0);
	f = signal(SIGALRM, sigALRM);
again:
	nc = 0; nl = sizeof(dialer_buf)-1;
	bzero(dialer_buf, sizeof(dialer_buf));
	timeout = 0;
	for (nc = 0, nl = sizeof(dialer_buf)-1 ; nl > 0 ; nc++, nl--) {
		if (setjmp(timeoutbuf))
			break;
		alarm(number(value(DIALTIMEOUT)));
		n = read(FD, &c, 1);
		alarm(0);
		if (n <= 0)
			break;
		c &= 0x7f;
		if (c == '\r') {
			if (cour_swallow("\n") == 0)
				break;
			if (!dialer_buf[0])
				goto again;
			if (strcmp(dialer_buf, "RINGING") == 0 &&
			    boolean(value(VERBOSE))) {
#ifdef DEBUG
				printf("%s\r\n", dialer_buf);
#endif
				goto again;
			}
			if (strncmp(dialer_buf, "CONNECT",
				    sizeof("CONNECT")-1) != 0)
				break;
			for (bm = baud_msg ; bm->msg ; bm++)
				if (strcmp(bm->msg,
				    dialer_buf+sizeof("CONNECT")-1) == 0) {
					struct termios	cntrl;

					tcgetattr(FD, &cntrl);
					cfsetospeed(&cntrl, bm->baud);
					cfsetispeed(&cntrl, bm->baud);
					tcsetattr(FD, TCSAFLUSH, &cntrl);
					signal(SIGALRM, f);
#ifdef DEBUG
					if (boolean(value(VERBOSE)))
						printf("%s\r\n", dialer_buf);
#endif
					return (1);
				}
			break;
		}
		dialer_buf[nc] = c;
#ifdef notdef
		if (boolean(value(VERBOSE)))
			putchar(c);
#endif
	}
	printf("%s\r\n", dialer_buf);
	signal(SIGALRM, f);
	return (0);
}

/*
 * This convoluted piece of code attempts to get
 * the courier in sync.
 */
static int
coursync()
{
	int already = 0;
	int len;
	char buf[40];

	while (already++ < MAXRETRY) {
		tcflush(FD, TCIOFLUSH);
		cour_write(FD, "\rAT Z\r", 6);	/* reset modem */
		bzero(buf, sizeof(buf));
		sleep(1);
		ioctl(FD, FIONREAD, &len);
		if (len) {
			len = read(FD, buf, sizeof(buf));
#ifdef DEBUG
			buf[len] = '\0';
			printf("coursync: (\"%s\")\n\r", buf);
#endif
			if (strchr(buf, '0') || 
		   	   (strchr(buf, 'O') && strchr(buf, 'K')))
				return(1);
		}
		/*
		 * If not strapped for DTR control,
		 * try to get command mode.
		 */
		sleep(1);
		cour_write(FD, "+++", 3);
		sleep(1);
		/*
		 * Toggle DTR to force anyone off that might have left
		 * the modem connected.
		 */
		ioctl(FD, TIOCCDTR, 0);
		sleep(1);
		ioctl(FD, TIOCSDTR, 0);
	}
	cour_write(FD, "\rAT Z\r", 6);
	return (0);
}

static void
cour_write(fd, cp, n)
int fd;
char *cp;
int n;
{
#ifdef notdef
	if (boolean(value(VERBOSE)))
		write(1, cp, n);
#endif
	tcdrain(fd);
	cour_nap();
	for ( ; n-- ; cp++) {
		write(fd, cp, 1);
		tcdrain(fd);
		cour_nap();
	}
}

#ifdef DEBUG
cour_verbose_read()
{
	int n = 0;
	char buf[BUFSIZ];

	if (ioctl(FD, FIONREAD, &n) < 0)
		return;
	if (n <= 0)
		return;
	if (read(FD, buf, n) != n)
		return;
	write(1, buf, n);
}
#endif

/* Give the courier 50 milliseconds between characters */
void
cour_nap()
{
	struct timespec ts;

	ts.tv_sec = 0;
	ts.tv_nsec = 50 * 1000000;

	nanosleep(&ts, NULL);
}
