/*
 * Copyright (c) 1992, 1993
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
static char sccsid[] = "@(#)t3000.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * Routines for calling up on a Telebit T3000 modem.
 * Derived from Courier driver.
 */
#include "tip.h"
#include <stdio.h>

#define	MAXRETRY	5

static	void sigALRM();
static	int timeout = 0;
static	int connected = 0;
static	jmp_buf timeoutbuf, intbuf;
static	int t3000_sync();

t3000_dialer(num, acu)
	register char *num;
	char *acu;
{
	register char *cp;
#ifdef ACULOG
	char line[80];
#endif
	static int t3000_connect(), t3000_swallow();

	if (boolean(value(VERBOSE)))
		printf("Using \"%s\"\n", acu);

	ioctl(FD, TIOCHPCL, 0);
	/*
	 * Get in synch.
	 */
	if (!t3000_sync()) {
badsynch:
		printf("can't synchronize with t3000\n");
#ifdef ACULOG
		logent(value(HOST), num, "t3000", "can't synch up");
#endif
		return (0);
	}
	t3000_write(FD, "AT E0\r", 6);	/* turn off echoing */
	sleep(1);
#ifdef DEBUG
	if (boolean(value(VERBOSE)))
		t3000_verbose_read();
#endif
	ioctl(FD, TIOCFLUSH, 0);	/* flush any clutter */
	t3000_write(FD, "AT E0 H0 Q0 X4 V1\r", 18);
	if (!t3000_swallow("\r\nOK\r\n"))
		goto badsynch;
	fflush(stdout);
	t3000_write(FD, "AT D", 4);
	for (cp = num; *cp; cp++)
		if (*cp == '=')
			*cp = ',';
	t3000_write(FD, num, strlen(num));
	t3000_write(FD, "\r", 1);
	connected = t3000_connect();
#ifdef ACULOG
	if (timeout) {
		sprintf(line, "%d second dial timeout",
			number(value(DIALTIMEOUT)));
		logent(value(HOST), num, "t3000", line);
	}
#endif
	if (timeout)
		t3000_disconnect();
	return (connected);
}

t3000_disconnect()
{
	 /* first hang up the modem*/
	ioctl(FD, TIOCCDTR, 0);
	sleep(1);
	ioctl(FD, TIOCSDTR, 0);
	t3000_sync();				/* reset */
	close(FD);
}

t3000_abort()
{
	t3000_write(FD, "\r", 1);	/* send anything to abort the call */
	t3000_disconnect();
}

static void
sigALRM()
{
	printf("\07timeout waiting for reply\n");
	timeout = 1;
	longjmp(timeoutbuf, 1);
}

static int
t3000_swallow(match)
  register char *match;
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

#ifndef B19200		/* XXX */
#define	B19200	EXTA
#define	B38400	EXTB
#endif

struct tbaud_msg {
	char *msg;
	int baud;
	int baud2;
} tbaud_msg[] = {
	"",		B300,	0,
	" 1200",	B1200,	0,
	" 2400",	B2400,	0,
	" 4800",	B4800,	0,
	" 9600",	B9600,	0,
	" 14400",	B19200,	B9600,
	" 19200",	B19200,	B9600,
	" 38400",	B38400,	B9600,
	" 57600",	B38400,	B9600,
	" 7512",	B9600,	0,
	" 1275",	B2400,	0,
	" 7200",	B9600,	0,
	" 12000",	B19200,	B9600,
	0,		0,	0,
};

static int
t3000_connect()
{
	char c;
	int nc, nl, n;
	struct sgttyb sb;
	char dialer_buf[64];
	struct tbaud_msg *bm;
	sig_t f;

	if (t3000_swallow("\r\n") == 0)
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
			if (t3000_swallow("\n") == 0)
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
			for (bm = tbaud_msg ; bm->msg ; bm++)
				if (strcmp(bm->msg,
				    dialer_buf+sizeof("CONNECT")-1) == 0) {
					if (ioctl(FD, TIOCGETP, &sb) < 0) {
						perror("TIOCGETP");
						goto error;
					}
					sb.sg_ispeed = sb.sg_ospeed = bm->baud;
					if (ioctl(FD, TIOCSETP, &sb) < 0) {
						if (bm->baud2) {
							sb.sg_ispeed =
							sb.sg_ospeed =
								bm->baud2;
							if (ioctl(FD,
								  TIOCSETP,
								  &sb) >= 0)
								goto isok;
						}
						perror("TIOCSETP");
						goto error;
					}
isok:
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
error1:
	printf("%s\r\n", dialer_buf);
error:
	signal(SIGALRM, f);
	return (0);
}

/*
 * This convoluted piece of code attempts to get
 * the t3000 in sync.
 */
static int
t3000_sync()
{
	int already = 0;
	int len;
	char buf[40];

	while (already++ < MAXRETRY) {
		ioctl(FD, TIOCFLUSH, 0);	/* flush any clutter */
		t3000_write(FD, "\rAT Z\r", 6);	/* reset modem */
		bzero(buf, sizeof(buf));
		sleep(2);
		ioctl(FD, FIONREAD, &len);
#if 1
if (len == 0) len = 1;
#endif
		if (len) {
			len = read(FD, buf, sizeof(buf));
#ifdef DEBUG
			buf[len] = '\0';
			printf("t3000_sync: (\"%s\")\n\r", buf);
#endif
			if (index(buf, '0') || 
		   	   (index(buf, 'O') && index(buf, 'K')))
				return(1);
		}
		/*
		 * If not strapped for DTR control,
		 * try to get command mode.
		 */
		sleep(1);
		t3000_write(FD, "+++", 3);
		sleep(1);
		/*
		 * Toggle DTR to force anyone off that might have left
		 * the modem connected.
		 */
		ioctl(FD, TIOCCDTR, 0);
		sleep(1);
		ioctl(FD, TIOCSDTR, 0);
	}
	t3000_write(FD, "\rAT Z\r", 6);
	return (0);
}

t3000_write(fd, cp, n)
int fd;
char *cp;
int n;
{
	struct sgttyb sb;

#ifdef notdef
	if (boolean(value(VERBOSE)))
		write(1, cp, n);
#endif
	ioctl(fd, TIOCGETP, &sb);
	ioctl(fd, TIOCSETP, &sb);
	t3000_nap();
	for ( ; n-- ; cp++) {
		write(fd, cp, 1);
		ioctl(fd, TIOCGETP, &sb);
		ioctl(fd, TIOCSETP, &sb);
		t3000_nap();
	}
}

#ifdef DEBUG
t3000_verbose_read()
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

/*
 * Code stolen from /usr/src/lib/libc/gen/sleep.c
 */
#define mask(s) (1<<((s)-1))
#define setvec(vec, a) \
        vec.sv_handler = a; vec.sv_mask = vec.sv_onstack = 0

static napms = 50; /* Give the t3000 50 milliseconds between characters */

static int ringring;

t3000_nap()
{

        static void t3000_napx();
	int omask;
        struct itimerval itv, oitv;
        register struct itimerval *itp = &itv;
        struct sigvec vec, ovec;

        timerclear(&itp->it_interval);
        timerclear(&itp->it_value);
        if (setitimer(ITIMER_REAL, itp, &oitv) < 0)
                return;
        setvec(ovec, SIG_DFL);
        omask = sigblock(mask(SIGALRM));
        itp->it_value.tv_sec = napms/1000;
	itp->it_value.tv_usec = ((napms%1000)*1000);
        setvec(vec, t3000_napx);
        ringring = 0;
        (void) sigvec(SIGALRM, &vec, &ovec);
        (void) setitimer(ITIMER_REAL, itp, (struct itimerval *)0);
        while (!ringring)
                sigpause(omask &~ mask(SIGALRM));
        (void) sigvec(SIGALRM, &ovec, (struct sigvec *)0);
        (void) setitimer(ITIMER_REAL, &oitv, (struct itimerval *)0);
	(void) sigsetmask(omask);
}

static void
t3000_napx()
{
        ringring = 1;
}
