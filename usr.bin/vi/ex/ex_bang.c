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
static char sccsid[] = "@(#)ex_bang.c	8.24 (Berkeley) 3/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "sex/sex_screen.h"

/*
 * ex_bang -- :[line [,line]] ! command
 *
 * Pass the rest of the line after the ! character to the program named by
 * the O_SHELL option.
 *
 * Historical vi did NOT do shell expansion on the arguments before passing
 * them, only file name expansion.  This means that the O_SHELL program got
 * "$t" as an argument if that is what the user entered.  Also, there's a
 * special expansion done for the bang command.  Any exclamation points in
 * the user's argument are replaced by the last, expanded ! command.
 *
 * There's some fairly amazing slop in this routine to make the different
 * ways of getting here display the right things.  It took a long time to
 * get get right (wrong?), so be careful.
 */
int
ex_bang(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	enum filtertype ftype;
	ARGS *ap;
	EX_PRIVATE *exp;
	MARK rm;
	recno_t lno;
	size_t blen;
	int rval;
	char *bp, *msg;

	ap = cmdp->argv[0];
	if (ap->len == 0) {
		msgq(sp, M_ERR, "Usage: %s", cmdp->cmd->usage);
		return (1);
	}

	/* Swap commands. */
	exp = EXP(sp);
	if (exp->lastbcomm != NULL)
		FREE(exp->lastbcomm, strlen(exp->lastbcomm) + 1);
	if ((exp->lastbcomm = strdup(ap->bp)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}

	/*
	 * If the command was modified by the expansion, we redisplay it.
	 * Redisplaying it in vi mode is tricky, and handled separately
	 * in each case below.  If we're in ex mode, it's easy, so we just
	 * do it here.
	 */
	bp = NULL;
	if (F_ISSET(cmdp, E_MODIFY)) {
		if (IN_EX_MODE(sp)) {
			(void)ex_printf(EXCOOKIE, "!%s\n", ap->bp);
			(void)ex_fflush(EXCOOKIE);
		}
		/*
		 * Vi: Display the command if modified.  Historic vi displayed
		 * the command if it was modified due to file name and/or bang
		 * expansion.  If piping lines, it was immediately overwritten
		 * by any error or line change reporting.  We don't the user to
		 * have to page through the responses, so we only post it until
		 * it's erased by something else.  Otherwise, pass it on to the
		 * ex_exec_proc routine to display after the screen has been
		 * cleaned up.
		 */
		if (IN_VI_MODE(sp)) {
			GET_SPACE_RET(sp, bp, blen, ap->len + 2);
			bp[0] = '!';
			memmove(bp + 1, ap->bp, ap->len + 1);
		}
	}

	/*
	 * If addresses were specified, pipe lines from the file through
	 * the command.
	 */
	if (cmdp->addrcnt != 0) {
		if (bp != NULL) {
			(void)sp->s_busy(sp, bp);
			FREE_SPACE(sp, bp, blen);
		}
		/*
		 * !!!
		 * Historical vi permitted "!!" in an empty file.  When it
		 * happens, we get called with two addresses of 1,1 and a
		 * bad attitude.
		 */
		ftype = FILTER;
		if (cmdp->addr1.lno == 1 && cmdp->addr2.lno == 1) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno == 0) {
				cmdp->addr1.lno = cmdp->addr2.lno = 0;
				ftype = FILTER_READ;
			}
		}
		if (filtercmd(sp, ep,
		    &cmdp->addr1, &cmdp->addr2, &rm, ap->bp, ftype))
			return (1);
		sp->lno = rm.lno;
		F_SET(exp, EX_AUTOPRINT);
		return (0);
	}

	/*
	 * If no addresses were specified, run the command.  If the file
	 * has been modified and autowrite is set, write the file back.
	 * If the file has been modified, autowrite is not set and the
	 * warn option is set, tell the user about the file.
	 */
	msg = "\n";
	if (F_ISSET(ep, F_MODIFIED))
		if (O_ISSET(sp, O_AUTOWRITE)) {
			if (file_write(sp, ep, NULL, NULL, NULL, FS_ALL)) {
				rval = 1;
				goto ret;
			}
		} else if (O_ISSET(sp, O_WARN))
			if (IN_VI_MODE(sp) && F_ISSET(cmdp, E_MODIFY))
				msg = "\nFile modified since last write.\n";
			else
				msg = "File modified since last write.\n";

	/* Run the command. */
	rval = ex_exec_proc(sp, ap->bp, bp, msg);

	/* Ex terminates with a bang. */
	if (IN_EX_MODE(sp))
		(void)write(STDOUT_FILENO, "!\n", 2);

	/* Vi requires user permission to continue. */
	if (IN_VI_MODE(sp))
		F_SET(sp, S_CONTINUE);

	/* Free the extra space. */
ret:	if (bp != NULL)
		FREE_SPACE(sp, bp, blen);

	return (rval);
}
