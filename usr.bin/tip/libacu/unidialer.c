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
static char sccsid[] = "@(#)unidialer.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

/*
 * Generalized routines for calling up on a Hayes AT command set based modem.
 * Control variables are pulled out of a modem caps-style database to
 * configure the driver for a particular modem.
 */
#include "tipconf.h"
#include "tip.h"
#include "pathnames.h"

#include <sys/times.h>
#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "acucommon.h"
#include "tod.h"

/* #define DEBUG */
#define	MAXRETRY	5

typedef enum
{
	mpt_notype, mpt_string, mpt_number, mpt_boolean
} modem_parm_type_t;

typedef struct {
	modem_parm_type_t modem_parm_type;
	const char *name;
	union {
		char **string;
		unsigned int *number;
	} value;
	union {
		char *string;
		unsigned int number;
	} default_value;
} modem_parm_t;

/*
	Configuration
*/
static char modem_name [80];
static char *dial_command;
static char *hangup_command;
static char *echo_off_command;
static char *reset_command;
static char *init_string;
static char *escape_sequence;
static int hw_flow_control;
static int lock_baud;
static unsigned int intercharacter_delay;
static unsigned int intercommand_delay;
static unsigned int escape_guard_time;
static unsigned int reset_delay;

static int unidialer_dialer (register char *num, char *acu);
static void unidialer_disconnect ();
static void unidialer_abort ();

static acu_t unidialer =
{
	modem_name,
	unidialer_dialer,
	unidialer_disconnect,
	unidialer_abort
};

/*
	Table of parameters kept in modem database
*/
modem_parm_t modem_parms [] = {
	{ mpt_string, "dial_command", &dial_command, "ATDT%s\r" },
	{ mpt_string, "hangup_command", &hangup_command, "ATH\r", },
	{ mpt_string, "echo_off_command", &echo_off_command, "ATE0\r" },
	{ mpt_string, "reset_command", &reset_command, "ATZ\r" },
	{ mpt_string, "init_string", &init_string, "AT&F\r", },
	{ mpt_string, "escape_sequence", &escape_sequence, "+++" },
	{ mpt_boolean, "hw_flow_control", (char **)&hw_flow_control, NULL },
	{ mpt_boolean, "lock_baud", (char **)&lock_baud, NULL },
	{ mpt_number, "intercharacter_delay", (char **)&intercharacter_delay, (char *)50 },
	{ mpt_number, "intercommand_delay", (char **)&intercommand_delay, (char *)300 },
	{ mpt_number, "escape_guard_time", (char **)&escape_guard_time, (char *)300 },
	{ mpt_number, "reset_delay", (char **)&reset_delay, (char *)3000 },
	{ mpt_notype, NULL, NULL, NULL }
};

/*
	Forward declarations
*/
static void unidialer_verbose_read ();
static void unidialer_modem_cmd (int fd, CONST char *cmd);
static void unidialer_write (int fd, CONST char *cp, int n);
static void unidialer_write_str (int fd, CONST char *cp);
static void unidialer_disconnect ();
static void sigALRM ();
static int unidialersync ();
static int unidialer_swallow (register char *match);

/*
	Global vars
*/
static int timeout = 0;
static int connected = 0;
static jmp_buf timeoutbuf, intbuf;

#define cgetflag(f)	(cgetcap(bp, f, ':') != NULL)

#ifdef DEBUG

#define print_str(x) printf (#x " = %s\n", x)
#define print_num(x) printf (#x " = %d\n", x)

void dumpmodemparms (char *modem)
{
		printf ("modem parms for %s\n", modem);
		print_str (dial_command);
		print_str (hangup_command);
		print_str (echo_off_command);
		print_str (reset_command);
		print_str (init_string);
		print_str (escape_sequence);
		print_num (lock_baud);
		print_num (intercharacter_delay);
		print_num (intercommand_delay);
		print_num (escape_guard_time);
		print_num (reset_delay);
		printf ("\n");
}
#endif

static int getmodemparms (const char *modem)
{
	char *bp, *db_array [3], *modempath;
	int ndx, stat;
	modem_parm_t *mpp;

	modempath = getenv ("MODEMS");

	ndx = 0;

	if (modempath != NULL)
		db_array [ndx++] = modempath;

	db_array [ndx++] = _PATH_MODEMS;
	db_array [ndx] = NULL;

	if ((stat = cgetent (&bp, db_array, (char *)modem)) < 0) {
		switch (stat) {
		case -1:
			warnx ("unknown modem %s", modem);
			break;
		case -2:
			warnx ("can't open modem description file");
			break;
		case -3:
			warnx ("possible reference loop in modem description file");
			break;
		}
		return 0;
	}
	for (mpp = modem_parms; mpp->name; mpp++)
	{
		switch (mpp->modem_parm_type)
		{
			case mpt_string:
				if (cgetstr (bp, (char *)mpp->name, mpp->value.string) == -1)
					*mpp->value.string = mpp->default_value.string;
				break;

			case mpt_number:
			{
				long l;
				if (cgetnum (bp, (char *)mpp->name, &l) == -1)
					*mpp->value.number = mpp->default_value.number;
				else
					*mpp->value.number = (unsigned int)l;
			}
				break;

			case mpt_boolean:
				*mpp->value.number = cgetflag ((char *)mpp->name);
				break;
		}
	}
	strncpy (modem_name, modem, sizeof (modem_name) - 1);
	modem_name [sizeof (modem_name) - 1] = '\0';
	return 1;
}

/*
*/
acu_t* unidialer_getmodem (const char *modem_name)
{
	acu_t* rc = NOACU;
	if (getmodemparms (modem_name))
		rc = &unidialer;
	return rc;
}

static int unidialer_modem_ready ()
{
#ifdef TIOCMGET
	int state;
	ioctl (FD, TIOCMGET, &state);
	return (state & TIOCM_DSR) ? 1 : 0;
#else
	return (1);
#endif
}

static int unidialer_waitfor_modem_ready (int ms)
{
#ifdef TIOCMGET
	int count;
	for (count = 0; count < ms; count += 100)
	{
		if (unidialer_modem_ready ())
		{
#ifdef DEBUG
			printf ("unidialer_waitfor_modem_ready: modem ready.\n");
#endif
			break;
		}
		acu_nap (100);
	}
	return (count < ms);
#else
	acu_nap (250);
	return (1);
#endif
}

int unidialer_tty_clocal (int flag)
{
#if HAVE_TERMIOS
	struct termios t;
	tcgetattr (FD, &t);
	if (flag)
		t.c_cflag |= CLOCAL;
	else
		t.c_cflag &= ~CLOCAL;
	tcsetattr (FD, TCSANOW, &t);
#elif defined(TIOCMSET)
	int state;
	/*
		Don't have CLOCAL so raise CD in software to
		get the same effect.
	*/
	ioctl (FD, TIOCMGET, &state);
	if (flag)
		state |= TIOCM_CD;
	else
		state &= ~TIOCM_CD;
	ioctl (FD, TIOCMSET, &state);
#endif
}

int unidialer_get_modem_response (char *buf, int bufsz, int response_timeout)
{
	sig_t f;
	char c, *p = buf, *lid = buf + bufsz - 1;
	int state;

	assert (bufsz > 0);

	f = signal (SIGALRM, sigALRM);

	timeout = 0;

	if (setjmp (timeoutbuf)) {
		signal (SIGALRM, f);
		*p = '\0';
#ifdef DEBUG
		printf ("get_response: timeout buf=%s, state=%d\n", buf, state);
#endif
		return (0);
	}

	ualarm (response_timeout * 1000, 0);

	state = 0;

	while (1)
	{
		switch (state)
		{
			case 0:
				if (read (FD, &c, 1) == 1)
				{
					if (c == '\r')
					{
						++state;
					}
					else
					{
#ifdef DEBUG
						printf ("get_response: unexpected char %s.\n", ctrl (c));
#endif
					}
				}
				break;

			case 1:
				if (read (FD, &c, 1) == 1)
				{
					if (c == '\n')
					{
#ifdef DEBUG
						printf ("get_response: <CRLF> encountered.\n", buf);
#endif
						++state;
					}
					else
					{
							state = 0;
#ifdef DEBUG
							printf ("get_response: unexpected char %s.\n", ctrl (c));
#endif
					}
				}
				break;

			case 2:
				if (read (FD, &c, 1) == 1)
				{
					if (c == '\r')
						++state;
					else if (c >= ' ' && p < lid)
						*p++ = c;
				}
				break;

			case 3:
				if (read (FD, &c, 1) == 1)
				{
					if (c == '\n')
					{
						signal (SIGALRM, f);
						/* ualarm (0, 0); */
						alarm (0);
						*p = '\0';
#ifdef DEBUG
						printf ("get_response: %s\n", buf);
#endif
						return (1);
					}
					else
					{
						state = 0;
						p = buf;
					}
				}
				break;
		}
	}
}

int unidialer_get_okay (int ms)
{
	int okay;
	char buf [BUFSIZ];
	okay = unidialer_get_modem_response (buf, sizeof (buf), ms) &&
		strcmp (buf, "OK") == 0;
	return okay;
}

static int unidialer_dialer (register char *num, char *acu)
{
	register char *cp;
	char dial_string [80];
#if ACULOG
	char line [80];
#endif
	static int unidialer_connect(), unidialer_swallow();

	#ifdef DEBUG
	dumpmodemparms (modem_name);
	#endif

	if (lock_baud) {
		int i;
		if ((i = speed(number(value(BAUDRATE)))) == NULL)
			return 0;
		ttysetup (i);
	}

	if (boolean(value(VERBOSE)))
		printf("Using \"%s\"\n", acu);

	acu_hupcl ();

	/*
	 * Get in synch.
	 */
	if (!unidialersync()) {
badsynch:
		printf("tip: can't synchronize with %s\n", modem_name);
#if ACULOG
		logent(value(HOST), num, modem_name, "can't synch up");
#endif
		return (0);
	}

	unidialer_modem_cmd (FD, echo_off_command);	/* turn off echoing */

	sleep(1);

#ifdef DEBUG
	if (boolean(value(VERBOSE)))
		unidialer_verbose_read();
#endif

	acu_flush (); /* flush any clutter */

	unidialer_modem_cmd (FD, init_string);

	if (!unidialer_get_okay (reset_delay))
		goto badsynch;

	fflush (stdout);

	for (cp = num; *cp; cp++)
		if (*cp == '=')
			*cp = ',';

	(void) sprintf (dial_string, dial_command, num);

	unidialer_modem_cmd (FD, dial_string);

	connected = unidialer_connect ();

	if (connected && hw_flow_control) {
		acu_hw_flow_control (hw_flow_control);
	}

#if ACULOG
	if (timeout) {
		sprintf(line, "%d second dial timeout",
			number(value(DIALTIMEOUT)));
		logent(value(HOST), num, modem_name, line);
	}
#endif

	if (timeout)
		unidialer_disconnect ();

	return (connected);
}

static void unidialer_disconnect ()
{
	int okay, retries;

	acu_flush (); /* flush any clutter */

	unidialer_tty_clocal (TRUE);

 	/* first hang up the modem*/
	ioctl (FD, TIOCCDTR, 0);
	acu_nap (250);
	ioctl (FD, TIOCSDTR, 0);

	/*
	 * If AT&D2, then dropping DTR *should* just hangup the modem. But
	 * some modems reset anyway; also, the modem may be programmed to reset
	 * anyway with AT&D3. Play it safe and wait for the full reset time before
	 * proceeding.
	 */
	acu_nap (reset_delay);

	if (!unidialer_waitfor_modem_ready (reset_delay))
	{
#ifdef DEBUG
			printf ("unidialer_disconnect: warning CTS low.\r\n");
#endif
	}

	/*
	 * If not strapped for DTR control, try to get command mode.
	 */
	for (retries = okay = 0; retries < MAXRETRY && !okay; retries++)
	{
		int timeout_value;
		/* flush any clutter */
		if (!acu_flush ())
		{
#ifdef DEBUG
			printf ("unidialer_disconnect: warning flush failed.\r\n");
#endif
		}
		timeout_value = escape_guard_time;
		timeout_value += (timeout_value * retries / MAXRETRY);
		acu_nap (timeout_value);
		acu_flush (); /* flush any clutter */
		unidialer_modem_cmd (FD, escape_sequence);
		acu_nap (timeout_value);
		unidialer_modem_cmd (FD, hangup_command);
		okay = unidialer_get_okay (reset_delay);
	}
	if (!okay)
	{
		#if ACULOG
		logent(value(HOST), "", modem_name, "can't hang up modem");
		#endif
		if (boolean(value(VERBOSE)))
			printf("hang up failed\n");
	}
	(void) acu_flush ();
	close (FD);
}

static void unidialer_abort ()
{
	unidialer_write_str (FD, "\r");	/* send anything to abort the call */
	unidialer_disconnect ();
}

static void sigALRM ()
{
	(void) printf("\07timeout waiting for reply\n");
	timeout = 1;
	longjmp(timeoutbuf, 1);
}

static int unidialer_swallow (register char *match)
{
	sig_t f;
	char c;

	f = signal(SIGALRM, sigALRM);

	timeout = 0;

	if (setjmp(timeoutbuf)) {
		signal(SIGALRM, f);
		return (0);
	}

	alarm(number(value(DIALTIMEOUT)));

	do {
		if (*match =='\0') {
			signal(SIGALRM, f);
			alarm (0);
			return (1);
		}
		do {
			read (FD, &c, 1);
		} while (c == '\0');
		c &= 0177;
#ifdef DEBUG
		if (boolean(value(VERBOSE)))
		{
			/* putchar(c); */
			printf (ctrl (c));
		}
#endif
	} while (c == *match++);
	signal(SIGALRM, SIG_DFL);
	alarm(0);
#ifdef DEBUG
	if (boolean(value(VERBOSE)))
		fflush (stdout);
#endif
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

static int unidialer_connect ()
{
	char c;
	int nc, nl, n;
	char dialer_buf[64];
	struct baud_msg *bm;
	sig_t f;

	if (unidialer_swallow("\r\n") == 0)
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
			if (unidialer_swallow("\n") == 0)
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
 * the unidialer in sync.
 */
static int unidialersync ()
{
	int already = 0;
	int len;
	char buf[40];

	while (already++ < MAXRETRY) {
		acu_nap (intercommand_delay);
		acu_flush (); /* flush any clutter */
		unidialer_write_str (FD, reset_command); /* reset modem */
		bzero(buf, sizeof(buf));
		acu_nap (reset_delay);
		ioctl (FD, FIONREAD, &len);
		if (len) {
			len = read(FD, buf, sizeof(buf));
#ifdef DEBUG
			buf [len] = '\0';
			printf("unidialersync (%s): (\"%s\")\n\r", modem_name, buf);
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
		unidialer_write_str (FD, escape_sequence);
		acu_nap (escape_guard_time);
		unidialer_write_str (FD, hangup_command);
		/*
		 * Toggle DTR to force anyone off that might have left
		 * the modem connected.
		 */
		acu_nap (escape_guard_time);
		ioctl (FD, TIOCCDTR, 0);
		acu_nap (1000);
		ioctl (FD, TIOCSDTR, 0);
	}
	acu_nap (intercommand_delay);
	unidialer_write_str (FD, reset_command);
	return (0);
}

/*
	Send commands to modem; impose delay between commands.
*/
static void unidialer_modem_cmd (int fd, const char *cmd)
{
	static struct timeval oldt = { 0, 0 };
	struct timeval newt;
	tod_gettime (&newt);
	if (tod_lt (&newt, &oldt))
	{
		unsigned int naptime;
		tod_subfrom (&oldt, newt);
		naptime = oldt.tv_sec * 1000 + oldt.tv_usec / 1000;
		if (naptime > intercommand_delay)
		{
#ifdef DEBUG
		printf ("unidialer_modem_cmd: suspicious naptime (%u ms)\r\n", naptime);
#endif
			naptime = intercommand_delay;
		}
#ifdef DEBUG
		printf ("unidialer_modem_cmd: delaying %u ms\r\n", naptime);
#endif
		acu_nap (naptime);
	}
	unidialer_write_str (fd, cmd);
	tod_gettime (&oldt);
	newt.tv_sec = 0;
	newt.tv_usec = intercommand_delay;
	tod_addto (&oldt, &newt);
}

static void unidialer_write_str (int fd, const char *cp)
{
#ifdef DEBUG
	printf ("unidialer (%s): sending %s\n", modem_name, cp);
#endif
	unidialer_write (fd, cp, strlen (cp));
}

static void unidialer_write (int fd, const char *cp, int n)
{
	acu_nap (intercharacter_delay);
	for ( ; n-- ; cp++) {
		write (fd, cp, 1);
		acu_nap (intercharacter_delay);
	}
}

#ifdef DEBUG
static void unidialer_verbose_read()
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

/* end of unidialer.c */
