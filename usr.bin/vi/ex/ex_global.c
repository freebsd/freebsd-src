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
static char sccsid[] = "@(#)ex_global.c	8.43 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
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

enum which {GLOBAL, VGLOBAL};

static int	global __P((SCR *, EXF *, EXCMDARG *, enum which));

/*
 * ex_global -- [line [,line]] g[lobal][!] /pattern/ [commands]
 *	Exec on lines matching a pattern.
 */
int
ex_global(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (global(sp, ep,
	    cmdp, F_ISSET(cmdp, E_FORCE) ? VGLOBAL : GLOBAL));
}

/*
 * ex_vglobal -- [line [,line]] v[global] /pattern/ [commands]
 *	Exec on lines not matching a pattern.
 */
int
ex_vglobal(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (global(sp, ep, cmdp, VGLOBAL));
}

static int
global(sp, ep, cmdp, cmd)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	MARK abs;
	RANGE *rp;
	EX_PRIVATE *exp;
	recno_t elno, lno;
	regmatch_t match[1];
	regex_t *re, lre;
	size_t clen, len;
	int delim, eval, reflags, replaced, rval;
	char *cb, *ptrn, *p, *t;

	/*
	 * Skip leading white space.  Historic vi allowed any non-
	 * alphanumeric to serve as the global command delimiter.
	 */
	for (p = cmdp->argv[0]->bp; isblank(*p); ++p);
	if (*p == '\0' || isalnum(*p)) {
		msgq(sp, M_ERR, "Usage: %s", cmdp->cmd->usage);
		return (1);
	}
	delim = *p++;

	/*
	 * Get the pattern string, toss escaped characters.
	 *
	 * QUOTING NOTE:
	 * Only toss an escaped character if it escapes a delimiter.
	 */
	for (ptrn = t = p;;) {
		if (p[0] == '\0' || p[0] == delim) {
			if (p[0] == delim)
				++p;
			/*
			 * !!!
			 * Nul terminate the pattern string -- it's passed
			 * to regcomp which doesn't understand anything else.
			 */
			*t = '\0';
			break;
		}
		if (p[0] == '\\' && p[1] == delim)
			++p;
		*t++ = *p++;
	}

	/* If the pattern string is empty, use the last one. */
	if (*ptrn == '\0') {
		if (!F_ISSET(sp, S_SRE_SET)) {
			msgq(sp, M_ERR, "No previous regular expression");
			return (1);
		}
		re = &sp->sre;
	} else {
		/* Set RE flags. */
		reflags = 0;
		if (O_ISSET(sp, O_EXTENDED))
			reflags |= REG_EXTENDED;
		if (O_ISSET(sp, O_IGNORECASE))
			reflags |= REG_ICASE;

		/* Convert vi-style RE's to POSIX 1003.2 RE's. */
		if (re_conv(sp, &ptrn, &replaced))
			return (1);

		/* Compile the RE. */
		re = &lre;
		eval = regcomp(re, ptrn, reflags);

		/* Free up any allocated memory. */
		if (replaced)
			FREE_SPACE(sp, ptrn, 0);

		if (eval) {
			re_error(sp, eval, re);
			return (1);
		}

		/*
		 * Set saved RE.  Historic practice is that
		 * globals set direction as well as the RE.
		 */
		sp->sre = lre;
		sp->searchdir = FORWARD;
		F_SET(sp, S_SRE_SET);
	}

	/*
	 * Get a copy of the command string; the default command is print.
	 * Don't worry about a set of <blank>s with no command, that will
	 * default to print in the ex parser.
	 */
	if ((clen = strlen(p)) == 0) {
		p = "p";
		clen = 1;
	}
	MALLOC_RET(sp, cb, char *, clen);
	memmove(cb, p, clen);

	/*
	 * The global commands sets the substitute RE as well as
	 * the everything-else RE.
	 */
	sp->subre = sp->sre;
	F_SET(sp, S_SUBRE_SET);

	/* Set the global flag. */
	F_SET(sp, S_GLOBAL);

	/* The global commands always set the previous context mark. */
	abs.lno = sp->lno;
	abs.cno = sp->cno;
	if (mark_set(sp, ep, ABSMARK1, &abs, 1))
		goto err;

	/*
	 * For each line...  The semantics of global matching are that we first
	 * have to decide which lines are going to get passed to the command,
	 * and then pass them to the command, ignoring other changes.  There's
	 * really no way to do this in a single pass, since arbitrary line
	 * creation, deletion and movement can be done in the ex command.  For
	 * example, a good vi clone test is ":g/X/mo.-3", or "g/X/.,.+1d".
	 * What we do is create linked list of lines that are tracked through
	 * each ex command.  There's a callback routine which the DB interface
	 * routines call when a line is created or deleted.  This doesn't help
	 * the layering much.
	 */
	exp = EXP(sp);
	for (rval = 0, lno = cmdp->addr1.lno,
	    elno = cmdp->addr2.lno; lno <= elno; ++lno) {
		/* Someone's unhappy, time to stop. */
		if (INTERRUPTED(sp))
			goto interrupted;

		/* Get the line and search for a match. */
		if ((t = file_gline(sp, ep, lno, &len)) == NULL) {
			GETLINE_ERR(sp, lno);
			goto err;
		}
		match[0].rm_so = 0;
		match[0].rm_eo = len;
		switch(eval = regexec(re, t, 1, match, REG_STARTEND)) {
		case 0:
			if (cmd == VGLOBAL)
				continue;
			break;
		case REG_NOMATCH:
			if (cmd == GLOBAL)
				continue;
			break;
		default:
			re_error(sp, eval, re);
			goto err;
		}

		/* If follows the last entry, extend the last entry's range. */
		if ((rp = exp->rangeq.cqh_last) != (void *)&exp->rangeq &&
		    rp->stop == lno - 1) {
			++rp->stop;
			continue;
		}

		/* Allocate a new range, and append it to the list. */
		CALLOC(sp, rp, RANGE *, 1, sizeof(RANGE));
		if (rp == NULL)
			goto err;
		rp->start = rp->stop = lno;
		CIRCLEQ_INSERT_TAIL(&exp->rangeq, rp, q);
	}

	exp = EXP(sp);
	exp->range_lno = OOBLNO;
	for (;;) {
		/*
		 * Start at the beginning of the range each time, it may have
		 * been changed (or exhausted) if lines were inserted/deleted.
		 */
		if ((rp = exp->rangeq.cqh_first) == (void *)&exp->rangeq)
			break;
		if (rp->start > rp->stop) {
			CIRCLEQ_REMOVE(&exp->rangeq, exp->rangeq.cqh_first, q);
			free(rp);
			continue;
		}

		/*
		 * Execute the command, setting the cursor to the line so that
		 * relative addressing works.  This means that the cursor moves
		 * to the last line sent to the command, by default, even if
		 * the command fails.
		 */
		exp->range_lno = sp->lno = rp->start++;
		if (ex_cmd(sp, ep, cb, clen, 0))
			goto err;

		/* Someone's unhappy, time to stop. */
		if (INTERRUPTED(sp)) {
interrupted:		msgq(sp, M_INFO, "Interrupted");
			break;
		}
	}

	/* Set the cursor to the new value, making sure it exists. */
	if (exp->range_lno != OOBLNO) {
		if (file_lline(sp, ep, &lno))
			return (1);
		sp->lno =
		    lno < exp->range_lno ? (lno ? lno : 1) : exp->range_lno;
	}
	if (0) {
err:		rval = 1;
	}

	/* Command we ran may have set the autoprint flag, clear it. */
	F_CLR(exp, EX_AUTOPRINT);

	/* Clear the global flag. */
	F_CLR(sp, S_GLOBAL);

	/* Free any remaining ranges and the command buffer. */
	while ((rp = exp->rangeq.cqh_first) != (void *)&exp->rangeq) {
		CIRCLEQ_REMOVE(&exp->rangeq, exp->rangeq.cqh_first, q);
		free(rp);
	}
	free(cb);
	return (rval);
}

/*
 * global_insdel --
 *	Update the ranges based on an insertion or deletion.
 */
void
global_insdel(sp, ep, op, lno)
	SCR *sp;
	EXF *ep;
	enum operation op;
	recno_t lno;
{
	EX_PRIVATE *exp;
	RANGE *nrp, *rp;

	exp = EXP(sp);

	switch (op) {
	case LINE_APPEND:
		return;
	case LINE_DELETE:
		for (rp = exp->rangeq.cqh_first;
		    rp != (void *)&exp->rangeq; rp = nrp) {
			nrp = rp->q.cqe_next;
			/* If range less than the line, ignore it. */
			if (rp->stop < lno)
				continue;
			/* If range greater than the line, decrement range. */
			if (rp->start > lno) {
				--rp->start;
				--rp->stop;
				continue;
			}
			/* Lno is inside the range, decrement the end point. */
			if (rp->start > --rp->stop) {
				CIRCLEQ_REMOVE(&exp->rangeq, rp, q);
				free(rp);
			}
		}
		break;
	case LINE_INSERT:
		for (rp = exp->rangeq.cqh_first;
		    rp != (void *)&exp->rangeq; rp = rp->q.cqe_next) {
			/* If range less than the line, ignore it. */
			if (rp->stop < lno)
				continue;
			/* If range greater than the line, increment range. */
			if (rp->start >= lno) {
				++rp->start;
				++rp->stop;
				continue;
			}
			/*
			 * Lno is inside the range, so the range must be split.
			 * Since we're inserting a new element, neither range
			 * can be exhausted.
			 */
			CALLOC(sp, nrp, RANGE *, 1, sizeof(RANGE));
			if (nrp == NULL) {
				F_SET(sp, S_INTERRUPTED);
				return;
			}
			nrp->start = lno + 1;
			nrp->stop = rp->stop + 1;
			rp->stop = lno - 1;
			CIRCLEQ_INSERT_AFTER(&exp->rangeq, rp, nrp, q);
			rp = nrp;
		}
		break;
	case LINE_RESET:
		return;
	}
	/*
	 * If the command deleted/inserted lines, the cursor moves to
	 * the line after the deleted/inserted line.
	 */
	exp->range_lno = lno;
}
