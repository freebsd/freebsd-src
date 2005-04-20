/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)ip_term.c	8.2 (Berkeley) 10/13/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>
 
#include "../common/common.h"
#include "ip.h"

/*
 * ip_term_init --
 *	Initialize the terminal special keys.
 *
 * PUBLIC: int ip_term_init __P((SCR *));
 */
int
ip_term_init(sp)
	SCR *sp;
{
	SEQ *qp;

	/*
	 * Rework any function key mappings that were set before the
	 * screen was initialized.
	 */
	for (qp = sp->gp->seqq.lh_first; qp != NULL; qp = qp->q.le_next)
		if (F_ISSET(qp, SEQ_FUNCMAP))
			(void)ip_fmap(sp, qp->stype,
			    qp->input, qp->ilen, qp->output, qp->olen);
	return (0);
}

/*
 * ip_term_end --
 *	End the special keys defined by the termcap/terminfo entry.
 *
 * PUBLIC: int ip_term_end __P((GS *));
 */
int
ip_term_end(gp)
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
 * ip_fmap --
 *	Map a function key.
 *
 * PUBLIC: int ip_fmap __P((SCR *, seq_t, CHAR_T *, size_t, CHAR_T *, size_t));
 */
int
ip_fmap(sp, stype, from, flen, to, tlen)
	SCR *sp;
	seq_t stype;
	CHAR_T *from, *to;
	size_t flen, tlen;
{
	/* Bind a function key to a string sequence. */
	return (1);
}

/*
 * ip_optchange --
 *	IP screen specific "option changed" routine.
 *
 * PUBLIC: int ip_optchange __P((SCR *, int, char *, u_long *));
 */
int
ip_optchange(sp, opt, str, valp)
	SCR *sp;
	int opt;
	char *str;
	u_long *valp;
{
	switch (opt) {
	case O_COLUMNS:
	case O_LINES:
		F_SET(sp->gp, G_SRESTART);
		F_CLR(sp, SC_SCR_EX | SC_SCR_VI);
		break;
	case O_TERM:
		msgq(sp, M_ERR, "The screen type may not be changed");
		return (1);
	}
	return (0);
}
