/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)v_at.c	10.8 (Berkeley) 4/27/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"
#include "vi.h"

/*
 * v_at -- @
 *	Execute a buffer.
 *
 * PUBLIC: int v_at __P((SCR *, VICMD *));
 */
int
v_at(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	CB *cbp;
	CHAR_T name;
	TEXT *tp;
	size_t len;
	char nbuf[20];

	/*
	 * !!!
	 * Historically, [@*]<carriage-return> and [@*][@*] executed the most
	 * recently executed buffer in ex mode.  In vi mode, only @@ repeated
	 * the last buffer.  We change historic practice and make @* work from
	 * vi mode as well, it's simpler and more consistent.
	 *
	 * My intent is that *[buffer] will, in the future, pass the buffer to
	 * whatever interpreter is loaded.
	 */
	name = F_ISSET(vp, VC_BUFFER) ? vp->buffer : '@';
	if (name == '@' || name == '*') {
		if (!F_ISSET(sp, SC_AT_SET)) {
			ex_emsg(sp, NULL, EXM_NOPREVBUF);
			return (1);
		}
		name = sp->at_lbuf;
	}
	F_SET(sp, SC_AT_SET);

	CBNAME(sp, cbp, name);
	if (cbp == NULL) {
		ex_emsg(sp, KEY_NAME(sp, name), EXM_EMPTYBUF);
		return (1);
	}

	/* Save for reuse. */
	sp->at_lbuf = name;

	/*
	 * The buffer is executed in vi mode, while in vi mode, so simply
	 * push it onto the terminal queue and continue.
	 *
	 * !!!
	 * Historic practice is that if the buffer was cut in line mode,
	 * <newlines> were appended to each line as it was pushed onto
	 * the stack.  If the buffer was cut in character mode, <newlines>
	 * were appended to all lines but the last one.
	 *
	 * XXX
	 * Historic practice is that execution of an @ buffer could be
	 * undone by a single 'u' command, i.e. the changes were grouped
	 * together.  We don't get this right; I'm waiting for the new DB
	 * logging code to be available.
	 */
	for (tp = cbp->textq.cqh_last;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_prev)
		if ((F_ISSET(cbp, CB_LMODE) ||
		    tp->q.cqe_next != (void *)&cbp->textq) &&
		    v_event_push(sp, NULL, "\n", 1, 0) ||
		    v_event_push(sp, NULL, tp->lb, tp->len, 0))
			return (1);

	/*
	 * !!!
	 * If any count was supplied, it applies to the first command in the
	 * at buffer.
	 */
	if (F_ISSET(vp, VC_C1SET)) {
		len = snprintf(nbuf, sizeof(nbuf), "%lu", vp->count);
		if (v_event_push(sp, NULL, nbuf, len, 0))
			return (1);
	}
	return (0);
}
