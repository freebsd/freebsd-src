/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)tk_term.c	8.12 (Berkeley) 10/13/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "tki.h"

/*
 * tk_term_init --
 *	Initialize the terminal special keys.
 *
 * PUBLIC: int tk_term_init __P((SCR *));
 */
int
tk_term_init(sp)
	SCR *sp;
{
	SEQ *qp;

	/*
	 * Rework any function key mappings that were set before the
	 * screen was initialized.
	 */
	for (qp = sp->gp->seqq.lh_first; qp != NULL; qp = qp->q.le_next)
		if (F_ISSET(qp, SEQ_FUNCMAP))
			(void)tk_fmap(sp, qp->stype,
			    qp->input, qp->ilen, qp->output, qp->olen);
	return (0);
}

/*
 * tk_term_end --
 *	End the special keys defined by the termcap/terminfo entry.
 *
 * PUBLIC: int tk_term_end __P((GS *));
 */
int
tk_term_end(gp)
	GS *gp;
{
	SEQ *qp, *nqp;

	/* Delete screen specific mappings. */
	for (qp = gp->seqq.lh_first; qp != NULL; qp = nqp) {
		nqp = qp->q.le_next;
		if (F_ISSET(qp, SEQ_SCREEN))
			(void)seq_mdel(qp);
	}
	return (0);
}

/*
 * tk_fmap --
 *	Map a function key.
 *
 * PUBLIC: int tk_fmap __P((SCR *, seq_t, CHAR_T *, size_t, CHAR_T *, size_t));
 */
int
tk_fmap(sp, stype, from, flen, to, tlen)
	SCR *sp;
	seq_t stype;
	CHAR_T *from, *to;
	size_t flen, tlen;
{
	VI_INIT_IGNORE(sp);

	/* Bind a Tk/Tcl function key to a string sequence. */
	return (0);
}

/*
 * tk_optchange --
 *	Curses screen specific "option changed" routine.
 *
 * PUBLIC: int tk_optchange __P((SCR *, int, char *, u_long *));
 */
int
tk_optchange(sp, opt, str, valp)
	SCR *sp;
	int opt;
	char *str;
	u_long *valp;
{
	switch (opt) {
	case O_COLUMNS:
	case O_LINES:
		/*
		 * Changing the columns or lines require that we restart
		 * the screen.
		 */
		F_SET(sp->gp, G_SRESTART);
		F_CLR(sp, SC_SCR_EX | SC_SCR_VI);
		break;
	case O_TERM:
		msgq(sp, M_ERR, "The screen type may not be changed");
		return (1);
	}
	return (0);
}

/*
 * tk_ssize --
 *	Return the window size.
 *
 * PUBLIC: int tk_ssize __P((SCR *, int, size_t *, size_t *, int *));
 */
int
tk_ssize(sp, sigwinch, rowp, colp, changedp)
	SCR *sp;
	int sigwinch;
	size_t *rowp, *colp;
	int *changedp;
{
	TK_PRIVATE *tkp;

	tkp = GTKP(__global_list);
	(void)Tcl_Eval(tkp->interp, "tk_ssize");

	/*
	 * SunOS systems deliver SIGWINCH when windows are uncovered
	 * as well as when they change size.  In addition, we call
	 * here when continuing after being suspended since the window
	 * may have changed size.  Since we don't want to background
	 * all of the screens just because the window was uncovered,
	 * ignore the signal if there's no change.
	 *
	 * !!!
	 * sp may be NULL.
	 */
	if (sigwinch && sp != NULL &&
	    tkp->tk_ssize_row == O_VAL(sp, O_LINES) &&
	    tkp->tk_ssize_col == O_VAL(sp, O_COLUMNS)) {
		if (changedp != NULL)
			*changedp = 0;
		return (0);
	}

	if (rowp != NULL)
		*rowp = tkp->tk_ssize_row;
	if (colp != NULL)
		*colp = tkp->tk_ssize_col;
	if (changedp != NULL)
		*changedp = 1;
	return (0);
}
