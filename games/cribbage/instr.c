/*-
 * Copyright (c) 1990, 1993
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
static char sccsid[] = "@(#)instr.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/cribbage/instr.c,v 1.5 1999/12/12 03:04:15 billf Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "deck.h"
#include "cribbage.h"
#include "pathnames.h"

void
instructions()
{
	struct stat sb;
	union wait pstat;
	pid_t pid;
	char *pager, *path;

	if (stat(_PATH_INSTR, &sb)) {
		(void)fprintf(stderr, "cribbage: %s: %s.\n", _PATH_INSTR,
		    strerror(errno));
		exit(1);
	}
	switch (pid = vfork()) {
	case -1:
		(void)fprintf(stderr, "cribbage: %s.\n", strerror(errno));
		exit(1);
	case 0:
		if (!(path = getenv("PAGER")))
			path = _PATH_MORE;
		if ((pager = rindex(path, '/')) != NULL)
			++pager;
		pager = path;
		execlp(path, pager, _PATH_INSTR, (char *)NULL);
		(void)fprintf(stderr, "cribbage: %s.\n", strerror(errno));
		_exit(1);
	default:
		do {
			pid = waitpid(pid, (int *)&pstat, 0);
		} while (pid == -1 && errno == EINTR);
		if (pid == -1 || pstat.w_status)
			exit(1);
	}
}
