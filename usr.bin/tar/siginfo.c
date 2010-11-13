/*-
 * Copyright 2008 Colin Percival
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bsdtar_platform.h"
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsdtar.h"
#include "err.h"

/* Is there a pending SIGINFO or SIGUSR1? */
static volatile sig_atomic_t siginfo_received = 0;

struct siginfo_data {
	/* What sort of operation are we doing? */
	char * oper;

	/* What path are we handling? */
	char * path;

	/* How large is the archive entry? */
	int64_t size;

	/* Old signal handlers. */
#ifdef SIGINFO
	void (*siginfo_old)(int);
#endif
	void (*sigusr1_old)(int);
};

static void		 siginfo_handler(int sig);

/* Handler for SIGINFO / SIGUSR1. */
static void
siginfo_handler(int sig)
{

	(void)sig; /* UNUSED */

	/* Record that SIGINFO or SIGUSR1 has been received. */
	siginfo_received = 1;
}

void
siginfo_init(struct bsdtar *bsdtar)
{

	/* Allocate space for internal structure. */
	if ((bsdtar->siginfo = malloc(sizeof(struct siginfo_data))) == NULL)
		bsdtar_errc(1, errno, "malloc failed");

	/* Set the strings to NULL so that free() is safe. */
	bsdtar->siginfo->path = bsdtar->siginfo->oper = NULL;

#ifdef SIGINFO
	/* We want to catch SIGINFO, if it exists. */
	bsdtar->siginfo->siginfo_old = signal(SIGINFO, siginfo_handler);
#endif
#ifdef SIGUSR1
	/* ... and treat SIGUSR1 the same way as SIGINFO. */
	bsdtar->siginfo->sigusr1_old = signal(SIGUSR1, siginfo_handler);
#endif
}

void
siginfo_setinfo(struct bsdtar *bsdtar, const char * oper, const char * path,
    int64_t size)
{

	/* Free old operation and path strings. */
	free(bsdtar->siginfo->oper);
	free(bsdtar->siginfo->path);

	/* Duplicate strings and store entry size. */
	if ((bsdtar->siginfo->oper = strdup(oper)) == NULL)
		bsdtar_errc(1, errno, "Cannot strdup");
	if ((bsdtar->siginfo->path = strdup(path)) == NULL)
		bsdtar_errc(1, errno, "Cannot strdup");
	bsdtar->siginfo->size = size;
}

void
siginfo_printinfo(struct bsdtar *bsdtar, off_t progress)
{

	/* If there's a signal to handle and we know what we're doing... */
	if ((siginfo_received == 1) &&
	    (bsdtar->siginfo->path != NULL) &&
	    (bsdtar->siginfo->oper != NULL)) {
		if (bsdtar->verbose)
			fprintf(stderr, "\n");
		if (bsdtar->siginfo->size > 0) {
			safe_fprintf(stderr, "%s %s (%ju / %" PRId64 ")",
			    bsdtar->siginfo->oper, bsdtar->siginfo->path,
			    (uintmax_t)progress, bsdtar->siginfo->size);
		} else {
			safe_fprintf(stderr, "%s %s",
			    bsdtar->siginfo->oper, bsdtar->siginfo->path);
		}
		if (!bsdtar->verbose)
			fprintf(stderr, "\n");
		siginfo_received = 0;
	}
}

void
siginfo_done(struct bsdtar *bsdtar)
{

#ifdef SIGINFO
	/* Restore old SIGINFO handler. */
	signal(SIGINFO, bsdtar->siginfo->siginfo_old);
#endif
#ifdef SIGUSR1
	/* And the old SIGUSR1 handler, too. */
	signal(SIGUSR1, bsdtar->siginfo->sigusr1_old);
#endif

	/* Free strings. */
	free(bsdtar->siginfo->path);
	free(bsdtar->siginfo->oper);

	/* Free internal data structure. */
	free(bsdtar->siginfo);
}
