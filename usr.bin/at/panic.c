/* 
 *  panic.c - terminate fast in case of error
 *  Copyright (C) 1993  Thomas Koenig
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/usr.bin/at/panic.c,v 1.10 1999/12/05 19:57:14 charnier Exp $";
#endif /* not lint */

/* System Headers */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Local headers */

#include "panic.h"
#include "at.h"

/* External variables */

/* Global functions */

void
panic(char *a)
{
/* Something fatal has happened, print error message and exit.
 */
	if (fcreated)
		unlink(atfile);

	errx(EXIT_FAILURE, "%s", a);
}

void
perr(char *a)
{
/* Some operating system error; print error message and exit.
 */
	int serrno = errno;

	if (fcreated)
		unlink(atfile);

	errno = serrno;
	err(EXIT_FAILURE, "%s", a);
}

void
usage(void)
{
	/* Print usage and exit. */
    fprintf(stderr, "usage: at [-V] [-q x] [-f file] [-m] time\n"
		    "       at [-V] -c job [job ...]\n"
		    "       atq [-V] [-q x] [-v]\n"
		    "       atrm [-V] job [job ...]\n"
		    "       batch [-V] [-f file] [-m]\n");
    exit(EXIT_FAILURE);
}
