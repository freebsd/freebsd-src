/*
 * panic.c - terminate fast in case of error
 * Copyright (c) 1993 by Thomas Koenig
 * All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* System Headers */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Local headers */

#include "panic.h"
#include "at.h"

/* File scope variables */

static char rcsid[] = "$Id: panic.c,v 1.1 1993/12/05 11:36:51 cgd Exp $";

/* External variables */

/* Global functions */

void
panic(a)
	char *a;
{
/* Something fatal has happened, print error message and exit.
 */
	fprintf(stderr, "%s: %s\n", namep, a);
	if (fcreated)
		unlink(atfile);

	exit(EXIT_FAILURE);
}

void
perr(a)
	char *a;
{
/* Some operating system error; print error message and exit.
 */
	perror(a);
	if (fcreated)
		unlink(atfile);

	exit(EXIT_FAILURE);
}

void 
perr2(a, b)
	char *a, *b;
{
	fprintf(stderr, "%s", a);
	perr(b);
}

void
usage(void)
{
/* Print usage and exit.
*/
	fprintf(stderr, "Usage: at [-q x] [-f file] [-m] time\n"
	    "       atq [-q x] [-v]\n"
	    "       atrm [-q x] job ...\n"
	    "       batch [-f file] [-m]\n");
	exit(EXIT_FAILURE);
}
