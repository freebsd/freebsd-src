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
static const char sccsid[] = "@(#)tk_util.c	8.14 (Berkeley) 7/19/96";
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

static int tk_op_push __P((SCR *, TK_PRIVATE *, e_event_t));

/*
 * tk_op --
 *	Events provided directly from Tcl/Tk.
 *
 * PUBLIC: int tk_op __P((ClientData, Tcl_Interp *, int, char *[]));
 */
int
tk_op(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	SCR *sp;
	TK_PRIVATE *tkp;

	sp = __global_list->dq.cqh_first;		/* XXX */
	tkp = (TK_PRIVATE *)clientData;

	switch (argv[1][0]) {
	case 'q':
		if (!strcmp(argv[1], "quit"))
			return (tk_op_push(sp, tkp, E_QUIT));
		break;
	case 'w':
		if (!strcmp(argv[1], "write"))
			return (tk_op_push(sp, tkp, E_WRITE));
		if (!strcmp(argv[1], "writequit")) {
			if (tk_op_push(sp, tkp, E_WRITE) != TCL_OK)
				return (TCL_ERROR);
			if (tk_op_push(sp, tkp, E_QUIT) != TCL_OK)
				return (TCL_ERROR);
			return (TCL_OK);
		}
		break;
	}
	(void)Tcl_VarEval(tkp->interp,
	    "tk_err {", argv[1], ": unknown event", "}", NULL);
	return (TCL_OK);
}

/*
 * tk_op_push --
 *	Push an event.
 */
static int
tk_op_push(sp, tkp, et)
	SCR *sp;
	TK_PRIVATE *tkp;
	e_event_t et;
{
	EVENT *evp;

	CALLOC(sp, evp, EVENT *, 1, sizeof(EVENT));
	if (evp == NULL)
		return (TCL_ERROR);

	evp->e_event = et;
	TAILQ_INSERT_TAIL(&tkp->evq, evp, q);
	return (TCL_OK);
}

/*
 * tk_opt_init --
 *	Initialize the values of the current options.
 *
 * PUBLIC: int tk_opt_init __P((ClientData, Tcl_Interp *, int, char *[]));
 */
int
tk_opt_init(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	OPTLIST const *op;
	SCR *sp;
	TK_PRIVATE *tkp;
	int off;
	char buf[20];

	sp = __global_list->dq.cqh_first;		/* XXX */

	tkp = (TK_PRIVATE *)clientData;
	for (op = optlist, off = 0; op->name != NULL; ++op, ++off) {
		if (F_ISSET(op, OPT_NDISP))
			continue;
		switch (op->type) {
		case OPT_0BOOL:
		case OPT_1BOOL:
			(void)Tcl_VarEval(tkp->interp, "set tko_",
			    op->name, O_ISSET(sp, off) ? " 1" : " 0", NULL);
			break;
		case OPT_NUM:
			(void)snprintf(buf,
			    sizeof(buf), " %lu", O_VAL(sp, off));
			(void)Tcl_VarEval(tkp->interp,
			    "set tko_", op->name, buf, NULL);
			break;
		case OPT_STR:
			(void)Tcl_VarEval(tkp->interp,
			    "set tko_", op->name, " {",
			    O_STR(sp, off) == NULL ? "" : O_STR(sp, off),
			    "}", NULL);
			break;
		}
	}
	return (TCL_OK);
}

/*
 * tk_opt_set --
 *	Set an options button.
 *
 * PUBLIC: int tk_opt_set __P((ClientData, Tcl_Interp *, int, char *[]));
 */
int
tk_opt_set(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	ARGS *ap[2], a, b;
	GS *gp;
	SCR *sp;
	int rval;
	void (*msg) __P((SCR *, mtype_t, char *, size_t));
	char buf[64];

	gp = __global_list;
	sp = gp->dq.cqh_first;				/* XXX */

	switch (argc) {
	case 2:
		a.bp = argv[1] + (sizeof("tko_") - 1);
		a.len = strlen(a.bp);
		break;
	case 3:
		a.bp = buf;
		a.len = snprintf(buf, sizeof(buf),
		    "%s%s", atoi(argv[2]) ? "no" : "", argv[1]);
		break;
	default:
		abort();
	}
	b.bp = NULL;
	b.len = 0;
	ap[0] = &a;
	ap[1] = &b;

	/* Use Tcl/Tk for error messages. */
	msg = gp->scr_msg;
	gp->scr_msg = tk_msg;

	rval = opts_set(sp, ap, NULL);

	gp->scr_msg = msg;
	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tk_version --
 *	Display the version in Tcl/Tk.
 *
 * PUBLIC: int tk_version __P((ClientData, Tcl_Interp *, int, char *[]));
 */
int
tk_version(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	EXCMD cmd;
	GS *gp;
	SCR *sp;
	int rval;
	void (*msg) __P((SCR *, mtype_t, char *, size_t));

	gp = __global_list;
	sp = gp->dq.cqh_first;				/* XXX */

	msg = gp->scr_msg;
	gp->scr_msg = tk_msg;

	ex_cinit(&cmd, C_VERSION, 0, OOBLNO, OOBLNO, 0, NULL);
	rval = cmd.cmd->fn(sp, &cmd);
	(void)ex_fflush(sp);

	gp->scr_msg = msg;
	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tk_msg --
 *	Display an error message in Tcl/Tk.
 *
 * PUBLIC: void tk_msg __P((SCR *, mtype_t, char *, size_t));
 */
void
tk_msg(sp, mtype, line, rlen)
	SCR *sp;
	mtype_t mtype;
	char *line;
	size_t rlen;
{
	TK_PRIVATE *tkp;
	char buf[2048];

	if (line == NULL)
		return;

	tkp = TKP(sp);
	(void)snprintf(buf, sizeof(buf), "%.*s", (int)rlen, line);
	(void)Tcl_VarEval(tkp->interp, "tk_err {", buf, "}", NULL);
}
