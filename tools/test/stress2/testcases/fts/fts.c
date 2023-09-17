/*-
 * Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* Directory scan */

#include <sys/param.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <libutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stress.h"

int
setup(int nb __unused)
{
	return (0);
}

void
cleanup(void)
{
}

int
test(void)
{

	FTS *fts;
	FTSENT *p;
	int ftsoptions;
	char *args[2] = {NULL, NULL};

	ftsoptions = FTS_LOGICAL;
	args[0] = strdup(".");

	if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
		err(1, "fts_open");

	while ((p = fts_read(fts)) != NULL && done_testing == 0) {
		if (op->verbose > 2)
			(void) printf("%s\n", p->fts_path);
		switch (p->fts_info) {
			case FTS_F:			/* Ignore. */
				break;
			case FTS_D:			/* Ignore. */
				break;
			case FTS_DP:
				break;
			case FTS_DC:			/* Ignore. */
				break;
			case FTS_SL:			/* Ignore. */
				break;
			case FTS_SLNONE:		/* Ignore. */
				break;
			case FTS_DNR:			/* Warn, continue. */
			case FTS_ERR:
			case FTS_NS:
			case FTS_DEFAULT:
				if (op->verbose > 1)
					warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
				break;
			default:
				printf("%s: default, %d\n", getprogname(), p->fts_info);
				break;
		}
	}

	if (fts_close(fts) == -1)
		err(1, "fts_close()");

	return (0);
}
