/*-
 * Copyright (c) 1991, 1993, 1994
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
static const char sccsid[] = "@(#)ex_args.c	8.28 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_next -- :next [+cmd] [files]
 *	Edit the next file, optionally setting the list of files.
 *
 * !!!
 * The :next command behaved differently from the :rewind command in
 * historic vi.  See nvi/docs/autowrite for details, but the basic
 * idea was that it ignored the force flag if the autowrite flag was
 * set.  This implementation handles them all identically.
 */
int
ex_next(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	ARGS **argv, **pc;
	FREF *frp;
	int noargs;
	char **ap;

	if (file_m1(sp, ep, F_ISSET(cmdp, E_FORCE), FS_ALL | FS_POSSIBLE))
		return (1);

	/*
	 * If the first argument is a plus sign, '+', it's an initial
	 * ex command.
	 */
	argv = cmdp->argv;
	if (cmdp->argc && argv[0]->bp[0] == '+') {
		--cmdp->argc;
		pc = argv++;
	} else
		pc = NULL;

	/* Any other arguments are a replacement file list. */
	if (cmdp->argc) {
		/* Free the current list. */
		if (!F_ISSET(sp, S_ARGNOFREE) && sp->argv != NULL) {
			for (ap = sp->argv; *ap != NULL; ++ap)
				free(*ap);
			free(sp->argv);
		}
		F_CLR(sp, S_ARGNOFREE | S_ARGRECOVER);
		sp->cargv = NULL;

		/* Create a new list. */
		CALLOC_RET(sp,
		    sp->argv, char **, cmdp->argc + 1, sizeof(char *));
		for (ap = sp->argv,
		    argv = cmdp->argv; argv[0]->len != 0; ++ap, ++argv)
			if ((*ap =
			    v_strdup(sp, argv[0]->bp, argv[0]->len)) == NULL)
				return (1);
		*ap = NULL;

		/* Switch to the first one. */
		sp->cargv = sp->argv;
		if ((frp = file_add(sp, *sp->cargv)) == NULL)
			return (1);
		noargs = 0;
	} else {
		if (sp->cargv == NULL || sp->cargv[1] == NULL) {
			msgq(sp, M_ERR, "No more files to edit");
			return (1);
		}
		if ((frp = file_add(sp, sp->cargv[1])) == NULL)
			return (1);
		if (F_ISSET(sp, S_ARGRECOVER))
			F_SET(frp, FR_RECOVER);
		noargs = 1;
	}

	if (file_init(sp, frp, NULL, F_ISSET(cmdp, E_FORCE)))
		return (1);
	if (noargs)
		++sp->cargv;

	/* Push the initial command onto the stack. */
	if (pc != NULL)
		if (IN_EX_MODE(sp))
			(void)term_push(sp, pc[0]->bp, pc[0]->len, 0);
		else if (IN_VI_MODE(sp)) {
			(void)term_push(sp, "\n", 1, 0);
			(void)term_push(sp, pc[0]->bp, pc[0]->len, 0);
			(void)term_push(sp, ":", 1, 0);
			(void)file_lline(sp, sp->ep, &sp->frp->lno);
			F_SET(sp->frp, FR_CURSORSET);
		}

	F_SET(sp, S_FSWITCH);
	return (0);
}

/*
 * ex_prev -- :prev
 *	Edit the previous file.
 */
int
ex_prev(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	FREF *frp;

	if (file_m1(sp, ep, F_ISSET(cmdp, E_FORCE), FS_ALL | FS_POSSIBLE))
		return (1);

	if (sp->cargv == sp->argv) {
		msgq(sp, M_ERR, "No previous files to edit");
		return (1);
	}
	if ((frp = file_add(sp, sp->cargv[-1])) == NULL)
		return (1);

	if (file_init(sp, frp, NULL, F_ISSET(cmdp, E_FORCE)))
		return (1);

	--sp->cargv;
	F_SET(sp, S_FSWITCH);
	return (0);
}

/*
 * ex_rew -- :rew
 *	Re-edit the list of files.
 */
int
ex_rew(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	FREF *frp;

	/*
	 * !!!
	 * Historic practice -- you can rewind to the current file.
	 */
	if (sp->argv == NULL) {
		msgq(sp, M_ERR, "No previous files to rewind");
		return (1);
	}

	if (file_m1(sp, ep, F_ISSET(cmdp, E_FORCE), FS_ALL | FS_POSSIBLE))
		return (1);

	/*
	 * !!!
	 * Historic practice, start at the beginning of the file.
	 */
	for (frp = sp->frefq.cqh_first;
	    frp != (FREF *)&sp->frefq; frp = frp->q.cqe_next)
		F_CLR(frp, FR_CURSORSET | FR_FNONBLANK);
	
	/* Switch to the first one. */
	sp->cargv = sp->argv;
	if ((frp = file_add(sp, *sp->cargv)) == NULL)
		return (1);
	if (file_init(sp, frp, NULL, F_ISSET(cmdp, E_FORCE)))
		return (1);

	F_SET(sp, S_FSWITCH);
	return (0);
}

/*
 * ex_args -- :args
 *	Display the list of files.
 */
int
ex_args(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	int cnt, col, len, sep;
	char **ap;

	if (sp->argv == NULL) {
		(void)ex_printf(EXCOOKIE, "No file list to display.\n");
		return (0);
	}
		
	col = len = sep = 0;
	for (cnt = 1, ap = sp->argv; *ap != NULL; ++ap) {
		col += len = strlen(*ap) + sep + (ap == sp->cargv ? 2 : 0);
		if (col >= sp->cols - 1) {
			col = len;
			sep = 0;
			(void)ex_printf(EXCOOKIE, "\n");
		} else if (cnt != 1) {
			sep = 1;
			(void)ex_printf(EXCOOKIE, " ");
		}
		++cnt;

		if (ap == sp->cargv)
			(void)ex_printf(EXCOOKIE, "[%s]", *ap);
		else
			(void)ex_printf(EXCOOKIE, "%s", *ap);
	}
	(void)ex_printf(EXCOOKIE, "\n");
	return (0);
}
