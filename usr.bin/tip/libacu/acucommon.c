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
static char sccsid[] = "@(#)acucommon.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * Routines for calling up on a Courier modem.
 * Derived from Hayes driver.
 */
#include "tipconf.h"
#include "tip.h"

#include <err.h>

#if HAVE_SELECT
#include <sys/types.h>
#include <sys/times.h>
#include <unistd.h>

void acu_nap (unsigned int how_long)
{
	struct timeval t;
	t.tv_usec = (how_long % 1000) * 1000;
	t.tv_sec = how_long / 1000;
	(void) select (0, NULL, NULL, NULL, &t);
}

#elif HAVE_USLEEP
void acu_nap (unsigned int how_long)
{
	(void) usleep (how_long * 1000);
}

#else

/*
 * Code stolen from /usr/src/lib/libc/gen/sleep.c
 */
#define mask(s) (1<<((s)-1))
#define setvec(vec, a) \
        vec.sv_handler = a; vec.sv_mask = vec.sv_onstack = 0

static int ringring;

static void acunap_napx()
{
	ringring = 1;
}

void acu_nap (unsigned int how_long)
{
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
        itp->it_value.tv_sec = how_long / 1000;
				itp->it_value.tv_usec = ((how_long % 1000) * 1000);
        setvec(vec, acunap_napx);
        ringring = 0;
        (void) sigvec(SIGALRM, &vec, &ovec);
        (void) setitimer(ITIMER_REAL, itp, (struct itimerval *)0);
        while (!ringring)
					sigpause(omask &~ mask(SIGALRM));
        (void) sigvec(SIGALRM, &ovec, (struct sigvec *)0);
        (void) setitimer(ITIMER_REAL, &oitv, (struct itimerval *)0);
				(void) sigsetmask(omask);
}

#endif /* HAVE_USLEEP */

void acu_hw_flow_control (hw_flow_control)
{
#if HAVE_TERMIOS
	struct termios t;
	if (tcgetattr (FD, &t) == 0) {
		if (hw_flow_control)
			t.c_cflag |= CRTSCTS;
		else
			t.c_cflag &= ~CRTSCTS;
		tcsetattr (FD, TCSANOW, &t);
	}
#endif /* HAVE_TERMIOS */
}

int acu_flush ()
{
#ifdef TIOCFLUSH
	int flags = 0;
	return (ioctl (FD, TIOCFLUSH, &flags) == 0);	/* flush any clutter */
#elif !HAVE_TERMIOS
	struct sgttyb buf;
	return (ioctl (FD, TIOCGETP, &buf) == 0 && ioctl (FD, TIOCSETP, &buf) == 0);
#endif
}

int acu_getspeed ()
{
#if HAVE_TERMIOS
	struct termios term;
	tcgetattr (FD, &term);
	return (term.c_ospeed);
#else /* HAVE_TERMIOS */
	struct sgttyb buf;
	ioctl (FD, TIOCGETP, &buf);
	return (buf.sg_ospeed);
#endif
}

int acu_setspeed (int speed)
{
	int rc = 0;
#if HAVE_TERMIOS
	struct termios term;
	if (tcgetattr (FD, &term) == 0) {
#ifndef _POSIX_SOURCE
		cfsetspeed (&term, speed);
#else
		cfsetispeed (&term, speed);
		cfsetospeed (&term, speed);
#endif
		if (tcsetattr (FD, TCSANOW, &term) == 0)
			++rc;
	}
#else /* HAVE TERMIOS */
	struct sgttyb sb;
	if (ioctl(FD, TIOCGETP, &sb) < 0) {
		warn("TIOCGETP");
	}
	else {
		sb.sg_ispeed = sb.sg_ospeed = speed;
		if (ioctl(FD, TIOCSETP, &sb) < 0) {
			warn("TIOCSETP");
		}
		else
			++rc;
	}
#endif /* HAVE TERMIOS */
	return (rc);
}

void acu_hupcl ()
{
#if HAVE_TERMIOS
	struct termios term;
	tcgetattr (FD, &term);
	term.c_cflag |= HUPCL;
	tcsetattr (FD, TCSANOW, &term);
#elif defined(TIOCHPCL)
	ioctl(FD, TIOCHPCL, 0);
#endif
}

/* end of acucommon.c */
