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
static char sccsid[] = "@(#)ex_read.c	8.26 (Berkeley) 3/23/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/stat.h>
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

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_read --	:read [file]
 *		:read [! cmd]
 *	Read from a file or utility.
 */
int
ex_read(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	struct stat sb;
	FILE *fp;
	MARK rm;
	recno_t nlines;
	size_t blen, len;
	int btear, itear, rval;
	char *bp, *name;

	/*
	 * If "read !", it's a pipe from a utility.
	 *
	 * !!!
	 * Historical vi wouldn't undo a filter read, for no apparent
	 * reason.
	 */
	if (F_ISSET(cmdp, E_FORCE)) {
		/* Expand the user's argument. */
		if (argv_exp1(sp, ep,
		    cmdp, cmdp->argv[0]->bp, cmdp->argv[0]->len, 0))
			return (1);

		/* If argc still 1, there wasn't anything to expand. */
		if (cmdp->argc == 1) {
			msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
			return (1);
		}

		/* Redisplay the user's argument if it's changed. */
		if (F_ISSET(cmdp, E_MODIFY) && IN_VI_MODE(sp)) {
			len = cmdp->argv[1]->len;
			GET_SPACE_RET(sp, bp, blen, len + 2);
			bp[0] = '!';
			memmove(bp + 1, cmdp->argv[1], cmdp->argv[1]->len + 1);
			(void)sp->s_busy(sp, bp);
			FREE_SPACE(sp, bp, blen);
		}

		if (filtercmd(sp, ep,
		    &cmdp->addr1, NULL, &rm, cmdp->argv[1]->bp, FILTER_READ))
			return (1);
		sp->lno = rm.lno;
		return (0);
	}

	/* Expand the user's argument. */
	if (argv_exp2(sp, ep,
	    cmdp, cmdp->argv[0]->bp, cmdp->argv[0]->len, 0))
		return (1);

	switch (cmdp->argc) {
	case 1:
		/*
		 * No arguments, read the current file.
		 * Doesn't set the alternate file name.
		 */
		name = FILENAME(sp->frp);
		break;
	case 2:
		/*
		 * One argument, read it.
		 * Sets the alternate file name.
		 */
		name = cmdp->argv[1]->bp;
		set_alt_name(sp, name);
		break;
	default:
		/* If expanded to more than one argument, object. */
		msgq(sp, M_ERR,
		    "%s expanded into too many file names", cmdp->argv[0]->bp);
		msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	/*
	 * !!!
	 * Historically, vi did not permit reads from non-regular files,
	 * nor did it distinguish between "read !" and "read!", so there
	 * was no way to "force" it.
	 */
	if ((fp = fopen(name, "r")) == NULL || fstat(fileno(fp), &sb)) {
		msgq(sp, M_SYSERR, name);
		return (1);
	}
	if (!S_ISREG(sb.st_mode)) {
		(void)fclose(fp);
		msgq(sp, M_ERR, "Only regular files may be read.");
		return (1);
	}

	/*
	 * Nvi handles the interrupt when reading from a file, but not
	 * when reading from a filter, since the terminal settings have
	 * been reset.
	 */
	btear = F_ISSET(sp, S_EXSILENT) ? 0 : !busy_on(sp, "Reading...");
	itear = !intr_init(sp);
	rval = ex_readfp(sp, ep, name, fp, &cmdp->addr1, &nlines, 1);
	if (btear)
		busy_off(sp);
	if (itear)
		intr_end(sp);

	/*
	 * Set the cursor to the first line read in, if anything read
	 * in, otherwise, the address.  (Historic vi set it to the
	 * line after the address regardless, but since that line may
	 * not exist we don't bother.)
	 */
	sp->lno = cmdp->addr1.lno;
	if (nlines)
		++sp->lno;

	F_SET(EXP(sp), EX_AUTOPRINT);
	return (rval);
}

/*
 * ex_readfp --
 *	Read lines into the file.
 */
int
ex_readfp(sp, ep, name, fp, fm, nlinesp, success_msg)
	SCR *sp;
	EXF *ep;
	char *name;
	FILE *fp;
	MARK *fm;
	recno_t *nlinesp;
	int success_msg;
{
	EX_PRIVATE *exp;
	recno_t lcnt, lno;
	size_t len;
	u_long ccnt;			/* XXX: can't print off_t portably. */
	int rval;

	rval = 0;
	exp = EXP(sp);

	/*
	 * Add in the lines from the output.  Insertion starts at the line
	 * following the address.
	 */
	ccnt = 0;
	lcnt = 0;
	for (lno = fm->lno; !ex_getline(sp, fp, &len); ++lno, ++lcnt) {
		if (F_ISSET(sp, S_INTERRUPTED)) {
			msgq(sp, M_INFO, "Interrupted.");
			break;
		}
		if (file_aline(sp, ep, 1, lno, exp->ibp, len)) {
			rval = 1;
			break;
		}
		ccnt += len;
	}

	if (ferror(fp)) {
		msgq(sp, M_SYSERR, name);
		rval = 1;
	}

	if (fclose(fp)) {
		msgq(sp, M_SYSERR, name);
		return (1);
	}

	if (rval)
		return (1);

	/* Return the number of lines read in. */
	if (nlinesp != NULL)
		*nlinesp = lcnt;

	if (success_msg)
		msgq(sp, M_INFO, "%s: %lu line%s, %lu characters.",
		    name, lcnt, lcnt == 1 ? "" : "s", ccnt);

	return (0);
}
