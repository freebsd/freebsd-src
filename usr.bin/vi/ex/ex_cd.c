/*-
 * Copyright (c) 1992, 1993, 1994
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
static char sccsid[] = "@(#)ex_cd.c	8.18 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_cd -- :cd[!] [directory]
 *	Change directories.
 */
int
ex_cd(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	struct passwd *pw;
	ARGS *ap;
	CDPATH *cdp;
	char *dir;		/* XXX END OF THE STACK, DON'T TRUST GETCWD. */
	char buf[MAXPATHLEN * 2];

	/*
	 * !!!
	 * Historic practice is that the cd isn't attempted if the file has
	 * been modified, unless its name begins with a leading '/' or the
	 * force flag is set.
	 */
	if (F_ISSET(ep, F_MODIFIED) &&
	    !F_ISSET(cmdp, E_FORCE) && sp->frp->name[0] != '/') {
		msgq(sp, M_ERR,
    "File modified since last complete write; write or use ! to override");
		return (1);
	}

	switch (cmdp->argc) {
	case 0:
		/* If no argument, change to the user's home directory. */
		if ((dir = getenv("HOME")) == NULL) {
			if ((pw = getpwuid(getuid())) == NULL ||
			    pw->pw_dir == NULL || pw->pw_dir[0] == '\0') {
				msgq(sp, M_ERR,
			   "Unable to find home directory location");
				return (1);
			}
			dir = pw->pw_dir;
		}
		break;
	case 1:
		dir = cmdp->argv[0]->bp;
		break;
	default:
		abort();
	}

	/* Try the current directory first. */
	if (!chdir(dir))
		goto ret;

	/*
	 * If moving to the user's home directory, or, the path begins with
	 * "/", "./" or "../", it's the only place we try.
	 */
	if (cmdp->argc == 0 ||
	    (ap = cmdp->argv[0])->bp[0] == '/' ||
	    ap->len == 1 && ap->bp[0] == '.' ||
	    ap->len >= 2 && ap->bp[0] == '.' && ap->bp[1] == '.' &&
	    (ap->bp[2] == '/' || ap->bp[2] == '\0'))
		goto err;

	/* If the user has a CDPATH variable, try its elements. */
	for (cdp = EXP(sp)->cdq.tqh_first; cdp != NULL; cdp = cdp->q.tqe_next) {
		(void)snprintf(buf, sizeof(buf), "%s/%s", cdp->path, dir);
		if (!chdir(buf)) {
ret:			if (getcwd(buf, sizeof(buf)) != NULL)
				msgq(sp, M_INFO, "New directory: %s", buf);
			return (0);
		}
	}
err:	msgq(sp, M_SYSERR, "%s", dir);
	return (1);
}

#define	FREE_CDPATH(cdp) {						\
	TAILQ_REMOVE(&exp->cdq, (cdp), q);				\
	free((cdp)->path);						\
	FREE((cdp), sizeof(CDPATH));					\
}
/*
 * ex_cdalloc --
 *	Create a new list of cd paths.
 */
int
ex_cdalloc(sp, str)
	SCR *sp;
	char *str;
{
	EX_PRIVATE *exp;
	CDPATH *cdp;
	size_t len;
	int founddot;
	char *p, *t;

	/* Free current queue. */
	exp = EXP(sp);
	while ((cdp = exp->cdq.tqh_first) != NULL)
		FREE_CDPATH(cdp);

	/*
	 * Create new queue.  The CDPATH environmental variable (and the
	 * user's manual entry) are delimited by colon characters.
	 */
	for (p = t = str, founddot = 0;; ++p) {
		if (*p == '\0' || *p == ':') {
			/*
			 * Empty strings specify ".".  The only way to get an
			 * empty string is a leading colon, colons in a row,
			 * or a trailing colon.  Or, to put it the other way,
			 * if the the length is zero, then it's either ":XXX",
			 * "XXX::XXXX" , "XXX:", or "", and the only failure
			 * mode is the last one.  Note, the string ":" gives
			 * us two entries of '.', so we only include one of
			 * them.
			 */
			if ((len = p - t) == 0) {
				if (p == str && *p == '\0')
					break;
				if (founddot) {
					if (*p == '\0')
						break;
					continue;
				}
				len = 1;
				t = ".";
				founddot = 1;
			}
			MALLOC_RET(sp, cdp, CDPATH *, sizeof(CDPATH));
			MALLOC(sp, cdp->path, char *, len + 1);
			if (cdp->path == NULL) {
				free(cdp);
				return (1);
			}
			memmove(cdp->path, t, len);
			cdp->path[len] = '\0';
			TAILQ_INSERT_TAIL(&exp->cdq, cdp, q);
			t = p + 1;
		}
		if (*p == '\0')
			 break;
	}
	return (0);
}
						/* Free previous queue. */
/*
 * ex_cdfree --
 *	Free the cd path list.
 */
int
ex_cdfree(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	CDPATH *cdp;

	/* Free up cd path information. */
	exp = EXP(sp);
	while ((cdp = exp->cdq.tqh_first) != NULL)
		FREE_CDPATH(cdp);
	return (0);
}
