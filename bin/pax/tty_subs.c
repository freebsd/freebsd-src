/*-
 * Copyright (c) 1992 Keith Muller.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
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
static char sccsid[] = "@(#)tty_subs.c	8.2 (Berkeley) 4/18/94";
#endif
static const char rcsid[] =
  "$FreeBSD: src/bin/pax/tty_subs.c,v 1.11 1999/08/27 23:14:48 peter Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "pax.h"
#include "extern.h"
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * routines that deal with I/O to and from the user
 */

#define DEVTTY          "/dev/tty"      /* device for interactive i/o */
static FILE *ttyoutf = NULL;		/* output pointing at control tty */
static FILE *ttyinf = NULL;		/* input pointing at control tty */

/*
 * tty_init()
 *	try to open the controlling terminal (if any) for this process. if the
 *	open fails, future ops that require user input will get an EOF
 */

#if __STDC__
int
tty_init(void)
#else
int
tty_init()
#endif
{
	int ttyfd;

        if ((ttyfd = open(DEVTTY, O_RDWR)) >= 0) {
		if ((ttyoutf = fdopen(ttyfd, "w")) != NULL) {
			if ((ttyinf = fdopen(ttyfd, "r")) != NULL)
				return(0);
			(void)fclose(ttyoutf);
		}
		(void)close(ttyfd);
	}

	if (iflag) {
		pax_warn(1, "Fatal error, cannot open %s", DEVTTY);
		return(-1);
	}
	return(0);
}

/*
 * tty_prnt()
 *	print a message using the specified format to the controlling tty
 *	if there is no controlling terminal, just return.
 */

#if __STDC__
void
tty_prnt(char *fmt, ...)
#else
void
tty_prnt(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#	if __STDC__
	va_start(ap, fmt);
#	else
	va_start(ap);
#	endif
	if (ttyoutf == NULL)
		return;
	(void)vfprintf(ttyoutf, fmt, ap);
	va_end(ap);
	(void)fflush(ttyoutf);
}

/*
 * tty_read()
 *	read a string from the controlling terminal if it is open into the
 *	supplied buffer
 * Return:
 *	0 if data was read, -1 otherwise.
 */

#if __STDC__
int
tty_read(char *str, int len)
#else
int
tty_read(str, len)
	char *str;
	int len;
#endif
{
	register char *pt;

	if ((--len <= 0) || (ttyinf == NULL) || (fgets(str,len,ttyinf) == NULL))
		return(-1);
	*(str + len) = '\0';

	/*
	 * strip off that trailing newline
	 */
	if ((pt = strchr(str, '\n')) != NULL)
		*pt = '\0';
	return(0);
}

/*
 * pax_warn()
 *	write a pax_warning message to stderr. if "set" the exit value of pax
 *	will be non-zero.
 */

#if __STDC__
void
pax_warn(int set, char *fmt, ...)
#else
void
pax_warn(set, fmt, va_alist)
	int set;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#	if __STDC__
	va_start(ap, fmt);
#	else
	va_start(ap);
#	endif
	if (set)
		exit_val = 1;
	/*
	 * when vflag we better ship out an extra \n to get this message on a
	 * line by itself
	 */
	if (vflag && vfpart) {
		(void)fputc('\n', stderr);
		vfpart = 0;
	}
	(void)fprintf(stderr, "%s: ", argv0);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fputc('\n', stderr);
}

/*
 * sys_warn()
 *	write a pax_warning message to stderr. if "set" the exit value of pax
 *	will be non-zero.
 */

#if __STDC__
void
sys_warn(int set, int errnum, char *fmt, ...)
#else
void
sys_warn(set, errnum, fmt, va_alist)
	int set;
	int errnum;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#	if __STDC__
	va_start(ap, fmt);
#	else
	va_start(ap);
#	endif
	if (set)
		exit_val = 1;
	/*
	 * when vflag we better ship out an extra \n to get this message on a
	 * line by itself
	 */
	if (vflag && vfpart) {
		(void)fputc('\n', stderr);
		vfpart = 0;
	}
	(void)fprintf(stderr, "%s: ", argv0);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);

	/*
	 * format and print the errno
	 */
	if (errnum > 0)
		(void)fprintf(stderr, " <%s>", sys_errlist[errnum]);
	(void)fputc('\n', stderr);
}
