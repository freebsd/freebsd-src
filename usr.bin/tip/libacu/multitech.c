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
static char sccsid[] = "@(#)multitech.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * Routines for calling up on a Courier modem.
 * Derived from Hayes driver.
 */
#include "tipconf.h"
#include "tip.h"
#include "acucommon.h"

#include <stdio.h>

/* #define DEBUG */
#define	MAXRETRY	5
/*
	Configuration
*/
static CONST char *dial_command = "ATDT";
static CONST char *hangup_command = "ATH\r";
static CONST char *echo_off_command = "ATE0\r";
static CONST char *reset_command = "\rATZ\r";
static CONST char *init_string = "AT$BA0$SB38400&E1&E4&E13&E15Q0V1X4E0S0=0\r";
static CONST char *escape_sequence = "+++"; /* return to command escape sequence */
static CONST int lock_baud = 1;
static CONST unsigned int intercharacter_delay = 20;
static CONST unsigned int intercommand_delay = 250;
static CONST unsigned int escape_guard_time = 250;
static CONST unsigned int reset_delay = 2000;

/*
	Forward declarations
*/
void multitech_write (int fd, CONST char *cp, int n);
void multitech_write_str (int fd, CONST char *cp);
void multitech_disconnect ();
void acu_nap (unsigned int how_long);
static void sigALRM ();
static int multitechsync ();
static int multitech_swallow (register char *match);

/*
	Global vars
*/
static int timeout = 0;
static int connected = 0;
static jmp_buf timeoutbuf, intbuf;

int multitech_dialer (register char *num, char *acu)
{
	register char *cp;
#if ACULOG
	char line [80];
#endif
	static int multitech_connect(), multitech_swallow();

	if (lock_baud)
	{
		int i;
		if ((i = speed(number(value(BAUDRATE)))) == 0)
			return 0;
		ttysetup (i);
	}

	if (boolean(value(VERBOSE)))
		printf("Using \"%s\"\n", acu);

	acu_hupcl ();

	/*
	 * Get in synch.
	 */
	if (!multitechsync()) {
badsynch:
		printf("can't synchronize with multitech\n");
#if ACULOG
		logent(value(HOST), num, "multitech", "can't synch up");
#endif
		return (0);
	}
	acu_nap (intercommand_delay);

	multitech_write_str (FD, echo_off_command);	/* turn off echoing */

	sleep(1);

#ifdef DEBUG
	if (boolean(value(VERBOSE)))
		multitech_verbose_read();
#endif

	acu_flush ();

	acu_nap (intercommand_delay);
	multitech_write_str (FD, init_string);

	if (!multitech_swallow ("\r\nOK\r\n"))
		goto badsynch;

	fflush (stdout);

	acu_nap (intercommand_delay);
	multitech_write_str (FD, dial_command);

	for (cp = num; *cp; cp++)
		if (*cp == '=')
			*cp = ',';

	multitech_write_str (FD, num);

	multitech_write_str (FD, "\r");

	connected = multitech_connect();

#if ACULOG
	if (timeout) {
		sprintf(line, "%d second dial timeout",
			number(value(DIALTIMEOUT)));
		logent(value(HOST), num, "multitech", line);
	}
#endif
	if (timeout)
		multitech_disconnect ();
	return (connected);
}

void multitech_disconnect ()
{
	int okay, retries;
	for (retries = okay = 0; retries < 3 && !okay; retries++)
	{
		 /* first hang up the modem*/
		ioctl (FD, TIOCCDTR, 0);
		acu_nap (escape_guard_time);
		ioctl (FD, TIOCSDTR, 0);
		acu_nap (escape_guard_time);
		/*
		 * If not strapped for DTR control, try to get command mode.
		 */
		acu_nap (escape_guard_time);
		multitech_write_str (FD, escape_sequence);
		acu_nap (escape_guard_time);
		multitech_write_str (FD, hangup_command);
		okay = multitech_swallow ("\r\nOK\r\n");
	}
	if (!okay)
	{
		#if ACULOG
		logent(value(HOST), "", "multitech", "can't hang up modem");
		#endif
		if (boolean(value(VERBOSE)))
			printf("hang up failed\n");
	}
	close (FD);
}

void multitech_abort ()
{
	multitech_write_str (FD, "\r");	/* send anything to abort the call */
	multitech_disconnect ();
}

static void sigALRM ()
{
	(void) printf("\07timeout waiting for reply\n");
	timeout = 1;
	longjmp(timeoutbuf, 1);
}

static int multitech_swallow (register char *match)
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
		fflush (stdout);
#endif
	signal(SIGALRM, SIG_DFL);
	return (0);
}

static struct baud_msg {
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

static int multitech_connect ()
{
	char c;
	int nc, nl, n;
	char dialer_buf[64];
	struct baud_msg *bm;
	sig_t f;

	if (multitech_swallow("\r\n") == 0)
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
			if (multitech_swallow("\n") == 0)
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
			if (lock_baud) {
				signal(SIGALRM, f);
#ifdef DEBUG
				if (boolean(value(VERBOSE)))
					printf("%s\r\n", dialer_buf);
#endif
				return (1);
			}
			for (bm = baud_msg ; bm->msg ; bm++)
				if (strcmp(bm->msg, dialer_buf+sizeof("CONNECT")-1) == 0) {
					if (!acu_setspeed (bm->baud))
						goto error;
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
	}
error1:
	printf("%s\r\n", dialer_buf);
error:
	signal(SIGALRM, f);
	return (0);
}

/*
 * This convoluted piece of code attempts to get
 * the multitech in sync.
 */
static int multitechsync ()
{
	int already = 0;
	int len;
	char buf[40];

	while (already++ < MAXRETRY) {
		acu_nap (intercommand_delay);
		ioctl (FD, TIOCFLUSH, 0);	/* flush any clutter */
		multitech_write_str (FD, reset_command); /* reset modem */
		bzero(buf, sizeof(buf));
		acu_nap (reset_delay);
		ioctl (FD, FIONREAD, &len);
		if (len) {
			len = read(FD, buf, sizeof(buf));
#ifdef DEBUG
			buf [len] = '\0';
			printf("multitechsync: (\"%s\")\n\r", buf);
#endif
			if (index(buf, '0') ||
		   	   (index(buf, 'O') && index(buf, 'K')))
				return(1);
		}
		/*
		 * If not strapped for DTR control,
		 * try to get command mode.
		 */
		acu_nap (escape_guard_time);
		multitech_write_str (FD, escape_sequence);
		acu_nap (escape_guard_time);
		multitech_write_str (FD, hangup_command);
		/*
		 * Toggle DTR to force anyone off that might have left
		 * the modem connected.
		 */
		acu_nap (escape_guard_time);
		ioctl (FD, TIOCCDTR, 0);
		acu_nap (escape_guard_time);
		ioctl (FD, TIOCSDTR, 0);
	}
	acu_nap (intercommand_delay);
	multitech_write_str (FD, reset_command);
	return (0);
}

void multitech_write_str (int fd, const char *cp)
{
#ifdef DEBUG
	printf ("multitech: sending %s\n", cp);
#endif
	multitech_write (fd, cp, strlen (cp));
}

void multitech_write (int fd, const char *cp, int n)
{
	acu_flush ();
	acu_nap (intercharacter_delay);
	for ( ; n-- ; cp++) {
		write (fd, cp, 1);
		acu_flush ();
		acu_nap (intercharacter_delay);
	}
}

#ifdef DEBUG
multitech_verbose_read()
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

/* end of multitech.c */
