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
static const char sccsid[] = "@(#)tk_read.c	8.12 (Berkeley) 9/24/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "../ex/script.h"
#include "tki.h"

static input_t	tk_read __P((SCR *, int));
static int	tk_resize __P((SCR *, size_t, size_t));


/*
 * tk_event --
 *	Return a single event.
 *
 * PUBLIC: int tk_event __P((SCR *, EVENT *, u_int32_t, int));
 */
int
tk_event(sp, evp, flags, timeout)
	SCR *sp;
	EVENT *evp;
	u_int32_t flags;
	int timeout;
{
	EVENT *tevp;
	TK_PRIVATE *tkp;
	size_t lines, columns;
	int changed;

	/*
	 * Queue signal based events.  We never clear SIGHUP or SIGTERM events,
	 * so that we just keep returning them until the editor dies.
	 */
	tkp = TKP(sp);
sig:	if (LF_ISSET(EC_INTERRUPT) || F_ISSET(tkp, TK_SIGINT)) {
		if (F_ISSET(tkp, TK_SIGINT)) {
			F_CLR(tkp, TK_SIGINT);
			evp->e_event = E_INTERRUPT;
		} else
			evp->e_event = E_TIMEOUT;
		return (0);
	}
	if (F_ISSET(tkp, TK_SIGHUP | TK_SIGTERM | TK_SIGWINCH)) {
		if (F_ISSET(tkp, TK_SIGHUP)) {
			evp->e_event = E_SIGHUP;
			return (0);
		}
		if (F_ISSET(tkp, TK_SIGTERM)) {
			evp->e_event = E_SIGTERM;
			return (0);
		}
		if (F_ISSET(tkp, TK_SIGWINCH)) {
			F_CLR(tkp, TK_SIGWINCH);
			(void)tk_ssize(sp, 1, &lines, &columns, &changed);
			if (changed) {
				(void)tk_resize(sp, lines, columns);
				evp->e_event = E_WRESIZE;
				return (0);
			}
			/* No change, so ignore the signal. */
		}
	}

	/* Queue special ops. */
ops:	if ((tevp = tkp->evq.tqh_first) != NULL) {
		*evp = *tevp;
		TAILQ_REMOVE(&tkp->evq, tevp, q);
		free(tevp);
		return (0);
	}
		
	/* Read input characters. */
	switch (tk_read(sp, timeout)) {
	case INP_OK:
		evp->e_csp = tkp->ibuf;
		evp->e_len = tkp->ibuf_cnt;
		evp->e_event = E_STRING;
		tkp->ibuf_cnt = 0;
		break;
	case INP_EOF:
		evp->e_event = E_EOF;
		break;
	case INP_ERR:
		evp->e_event = E_ERR;
		break;
	case INP_INTR:
		goto sig;
		break;
	case INP_TIMEOUT:
		/* May have returned because queued a special op. */
		if (tkp->evq.tqh_first != NULL)
			goto ops;

		/* Otherwise, we timed out. */
		evp->e_event = E_TIMEOUT;
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * tk_read --
 *	Read characters from the input.
 */
static input_t
tk_read(sp, timeout)
	SCR *sp;
	int timeout;
{
	TK_PRIVATE *tkp;
	char buf[20];

	/*
	 * Check scripting window file descriptors.  It's ugly that we wait
	 * on scripting file descriptors here, but it's the only way to keep
	 * from locking out scripting windows.
	 */
	if (F_ISSET(sp->gp, G_SCRWIN) && sscr_input(sp))
		return (INP_ERR);

	/* Read characters. */
	tkp = TKP(sp);
	(void)snprintf(buf, sizeof(buf), "%d", timeout);
	(void)Tcl_VarEval(tkp->interp, "tk_key_wait ", buf, NULL);

	return (tkp->ibuf_cnt == 0 ? INP_TIMEOUT : INP_OK);
}

/*
 * tk_key --
 *	Receive an input key.
 *
 * PUBLIC: int tk_key __P((ClientData, Tcl_Interp *, int, char *[]));
 */
int
tk_key(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	TK_PRIVATE *tkp;
	u_int8_t *p, *t;

	tkp = (TK_PRIVATE *)clientData;
	for (p =
	    tkp->ibuf + tkp->ibuf_cnt, t = argv[1]; (*p++ = *t++) != '\0';
	    ++tkp->ibuf_cnt); 
	return (TCL_OK);
}

/* 
 * tk_resize --
 *	Reset the options for a resize event.
 */
static int
tk_resize(sp, lines, columns)
	SCR *sp;
	size_t lines, columns;
{
	ARGS *argv[2], a, b;
	int rval;
	char b1[1024];

	a.bp = b1;
	b.bp = NULL;
	a.len = b.len = 0;
	argv[0] = &a;
	argv[1] = &b;

	(void)snprintf(b1, sizeof(b1), "lines=%lu", (u_long)lines);
	a.len = strlen(b1);
	if (opts_set(sp, argv, NULL))
		return (1);
	(void)snprintf(b1, sizeof(b1), "columns=%lu", (u_long)columns);
	a.len = strlen(b1);
	if (opts_set(sp, argv, NULL))
		return (1);
	return (0);
}
