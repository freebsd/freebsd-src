/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
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
static char const sccsid[] = "@(#)misc.c	8.3 (Berkeley) 4/2/94";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <sys/time.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "dd.h"
#include "extern.h"

void
summary()
{
	struct timeval tv;
	double secs;
	char buf[100];

	(void)gettimeofday(&tv, (struct timezone *)NULL);
	secs = tv.tv_sec + tv.tv_usec * 1e-6 - st.start;
	if (secs < 1e-6)
		secs = 1e-6;
	/* Use snprintf(3) so that we don't reenter stdio(3). */
	(void)snprintf(buf, sizeof(buf),
	    "%u+%u records in\n%u+%u records out\n",
	    st.in_full, st.in_part, st.out_full, st.out_part);
	(void)write(STDERR_FILENO, buf, strlen(buf));
	if (st.swab) {
		(void)snprintf(buf, sizeof(buf), "%u odd length swab %s\n",
		     st.swab, (st.swab == 1) ? "block" : "blocks");
		(void)write(STDERR_FILENO, buf, strlen(buf));
	}
	if (st.trunc) {
		(void)snprintf(buf, sizeof(buf), "%u truncated %s\n",
		     st.trunc, (st.trunc == 1) ? "block" : "blocks");
		(void)write(STDERR_FILENO, buf, strlen(buf));
	}
	(void)snprintf(buf, sizeof(buf),
	    "%qu bytes transferred in %.6f secs (%.0f bytes/sec)\n",
	    st.bytes, secs, st.bytes / secs);
	(void)write(STDERR_FILENO, buf, strlen(buf));
}

/* ARGSUSED */
void
summaryx(notused)
	int notused;
{
	int save_errno = errno;

	summary();
	errno = save_errno;
}

/* ARGSUSED */
void
terminate(sig)
	int sig;
{

	/* XXX exit() shouldn't call exit() from a signal handler. */
	exit(sig == 0 ? 0 : 1);
}
