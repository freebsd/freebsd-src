/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil. All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)tcl.c	8.16 (Berkeley) 10/16/96";
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
#include <tcl.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "tcl_extern.h"

static int  getint __P((Tcl_Interp *, char *, char *, int *));
static int  getscreenid __P((Tcl_Interp *, SCR **, char *, char *));
static void msghandler __P((SCR *, mtype_t, char *, size_t));

extern GS *__global_list;			/* XXX */

/*
 * INITMESSAGE --
 *	Macros to point messages at the Tcl message handler.
 */
#define	INITMESSAGE							\
	scr_msg = __global_list->scr_msg;				\
	__global_list->scr_msg = msghandler;
#define	ENDMESSAGE							\
	__global_list->scr_msg = scr_msg;

/*
 * tcl_fscreen --
 *	Return the screen id associated with file name.
 *
 * Tcl Command: viFindScreen
 * Usage: viFindScreen file
 */
static int
tcl_fscreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: viFindScreen file", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, NULL, argv[1]))
		return (TCL_ERROR);

	(void)sprintf(interp->result, "%d", sp->id);
	return (TCL_OK);
}

/*
 * tcl_aline --
 *	-- Append the string text after the line in lineNumber.
 *
 * Tcl Command: viAppendLine
 * Usage: viAppendLine screenId lineNumber text
 */
static int
tcl_aline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int lno, rval;

	if (argc != 4) {
		Tcl_SetResult(interp,
		    "Usage: viAppendLine screenId lineNumber text", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL) ||
	    getint(interp, "line number", argv[2], &lno))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_aline(sp, (recno_t)lno, argv[3], strlen(argv[3]));
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_dline --
 *	Delete lineNum.
 *
 * Tcl Command: viDelLine
 * Usage: viDelLine screenId lineNum
 */
static int
tcl_dline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int lno, rval;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viDelLine screenId lineNumber", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL) ||
	    getint(interp, "line number", argv[2], &lno))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_dline(sp, (recno_t)lno);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_gline --
 *	Return lineNumber.
 *
 * Tcl Command: viGetLine
 * Usage: viGetLine screenId lineNumber
 */
static int
tcl_gline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	size_t len;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int lno, rval;
	char *line, *p;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viGetLine screenId lineNumber", TCL_STATIC);
		return (TCL_ERROR);
	}
	if (getscreenid(interp, &sp, argv[1], NULL) ||
	    getint(interp, "line number", argv[2], &lno))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_gline(sp, (recno_t)lno, &p, &len);
	ENDMESSAGE;

	if (rval)
		return (TCL_ERROR);

	if ((line = malloc(len + 1)) == NULL)
		exit(1);				/* XXX */
	memmove(line, p, len);
	line[len] = '\0';
	Tcl_SetResult(interp, line, TCL_DYNAMIC);
	return (TCL_OK);
}

/*
 * tcl_iline --
 *	Insert the string text after the line in lineNumber.
 *
 * Tcl Command: viInsertLine
 * Usage: viInsertLine screenId lineNumber text
 */
static int
tcl_iline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int lno, rval;

	if (argc != 4) {
		Tcl_SetResult(interp,
		    "Usage: viInsertLine screenId lineNumber text", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL) ||
	    getint(interp, "line number", argv[2], &lno))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_iline(sp, (recno_t)lno, argv[3], strlen(argv[3]));
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_lline --
 *	Return the last line in the screen.
 *
 * Tcl Command: viLastLine
 * Usage: viLastLine screenId
 */
static int
tcl_lline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	recno_t last;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: viLastLine screenId", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_lline(sp, &last);
	ENDMESSAGE;
	if (rval)
		return (TCL_ERROR);

	(void)sprintf(interp->result, "%lu", (unsigned long)last);
	return (TCL_OK);
}

/*
 * tcl_sline --
 *	Set lineNumber to the text supplied.
 *
 * Tcl Command: viSetLine
 * Usage: viSetLine screenId lineNumber text
 */
static int
tcl_sline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int lno, rval;

	if (argc != 4) {
		Tcl_SetResult(interp,
		    "Usage: viSetLine screenId lineNumber text", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL) ||
	    getint(interp, "line number", argv[2], &lno))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_sline(sp, (recno_t)lno, argv[3], strlen(argv[3]));
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_getmark --
 *	Return the mark's cursor position as a list with two elements.
 *	{line, column}.
 *
 * Tcl Command: viGetMark
 * Usage: viGetMark screenId mark
 */
static int
tcl_getmark(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	MARK cursor;
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char buf[20];

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viGetMark screenId mark", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_getmark(sp, (int)argv[2][0], &cursor);
	ENDMESSAGE;

	if (rval)
		return (TCL_ERROR);

	(void)snprintf(buf, sizeof(buf), "%lu", (u_long)cursor.lno);
	Tcl_AppendElement(interp, buf);
	(void)snprintf(buf, sizeof(buf), "%lu", (u_long)cursor.cno);
	Tcl_AppendElement(interp, buf);
	return (TCL_OK);
}

/*
 * tcl_setmark --
 *	Set the mark to the line and column numbers supplied.
 *
 * Tcl Command: viSetMark
 * Usage: viSetMark screenId mark line column
 */
static int
tcl_setmark(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	MARK cursor;
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int i, rval;

	if (argc != 5) {
		Tcl_SetResult(interp,
		    "Usage: viSetMark screenId mark line column", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	if (getint(interp, "line number", argv[3], &i))
		return (TCL_ERROR);
	cursor.lno = i;
	if (getint(interp, "column number", argv[4], &i))
		return (TCL_ERROR);
	cursor.cno = i;
	INITMESSAGE;
	rval = api_setmark(sp, (int)argv[2][0], &cursor);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_getcursor --
 *	Return the current cursor position as a list with two elements.
 *	{line, column}.
 *
 * Tcl Command: viGetCursor
 * Usage: viGetCursor screenId
 */
static int
tcl_getcursor(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	MARK cursor;
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char buf[20];

	if (argc != 2) {
		Tcl_SetResult(interp,
		    "Usage: viGetCursor screenId", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_getcursor(sp, &cursor);
	ENDMESSAGE;

	if (rval)
		return (TCL_ERROR);

	(void)snprintf(buf, sizeof(buf), "%lu", (u_long)cursor.lno);
	Tcl_AppendElement(interp, buf);
	(void)snprintf(buf, sizeof(buf), "%lu", (u_long)cursor.cno);
	Tcl_AppendElement(interp, buf);
	return (TCL_OK);
}

/*
 * tcl_setcursor --
 *	Set the cursor to the line and column numbers supplied.
 *
 * Tcl Command: viSetCursor
 * Usage: viSetCursor screenId line column
 */
static int
tcl_setcursor(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	MARK cursor;
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int i, rval;

	if (argc != 4) {
		Tcl_SetResult(interp,
		    "Usage: viSetCursor screenId line column", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	if (getint(interp, "screen id", argv[2], &i))
		return (TCL_ERROR);
	cursor.lno = i;
	if (getint(interp, "screen id", argv[3], &i))
		return (TCL_ERROR);
	cursor.cno = i;
	INITMESSAGE;
	rval = api_setcursor(sp, &cursor);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_msg --
 *	Set the message line to text.
 *
 * Tcl Command: viMsg
 * Usage: viMsg screenId text
 */
static int
tcl_msg(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;

	if (argc != 3) {
		Tcl_SetResult(interp, "Usage: viMsg screenId text", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	api_imessage(sp, argv[2]);

	return (TCL_OK);
}

/*
 * tcl_iscreen --
 *	Create a new screen.  If a filename is specified then the screen
 *	is opened with that file.
 *
 * Tcl Command: viNewScreen
 * Usage: viNewScreen screenId [file]
 */
static int
tcl_iscreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp, *nsp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 2 && argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viNewScreen screenId [file]", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_edit(sp, argv[2], &nsp, 1);
	ENDMESSAGE;

	if (rval)
		return (TCL_ERROR);

	(void)sprintf(interp->result, "%d", nsp->id);
	return (TCL_OK);
}

/*
 * tcl_escreen --
 *	End a screen.
 *
 * Tcl Command: viEndScreen
 * Usage: viEndScreen screenId
 */
static int
tcl_escreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 2) {
		Tcl_SetResult(interp,
		     "Usage: viEndScreen screenId", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_escreen(sp);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_swscreen --
 *	Change the current focus to screen.
 *
 * Tcl Command: viSwitchScreen
 * Usage: viSwitchScreen screenId screenId
 */
static int
tcl_swscreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp, *new;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viSwitchScreen cur_screenId new_screenId",
		    TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	if (getscreenid(interp, &new, argv[2], NULL))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_swscreen(sp, new);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_map --
 *	Associate a key with a tcl procedure.
 *
 * Tcl Command: viMapKey
 * Usage: viMapKey screenId key tclproc
 */
static int
tcl_map(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char command[256];

	if (argc != 4) {
		Tcl_SetResult(interp,
		    "Usage: viMapKey screenId key tclproc", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	INITMESSAGE;
	(void)snprintf(command, sizeof(command), ":tcl %s\n", argv[3]);
	rval = api_map(sp, argv[2], command, strlen(command));
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_unmap --
 *	Unmap a key.
 *
 * Tcl Command: viUnmapKey
 * Usage: viUnmMapKey screenId key
 */
static int
tcl_unmap(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viUnmapKey screenId key", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_unmap(sp, argv[2]);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_opts_set --
 *	Set an option.
 *
 * Tcl Command: viSetOpt
 * Usage: viSetOpt screenId command
 */
static int
tcl_opts_set(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *setting;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viSetOpt screenId command", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	INITMESSAGE;
	/*rval = api_opts_set(sp, argv[2]);*/
	MALLOC(sp, setting, char *, strlen(argv[2])+6);
	strcpy(setting, ":set ");
	strcpy(setting+5, argv[2]);
	rval=api_run_str(sp, setting);
	free(setting);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_opts_get --
 	Return the value of an option.
 *	
 * Tcl Command: viGetOpt
 * Usage: viGetOpt screenId option
 */
static int
tcl_opts_get(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *value;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viGetOpt screenId option", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (getscreenid(interp, &sp, argv[1], NULL))
		return (TCL_ERROR);
	INITMESSAGE;
	rval = api_opts_get(sp, argv[2], &value, NULL);
	ENDMESSAGE;
	if (rval)
		return (TCL_ERROR);

	Tcl_SetResult(interp, value, TCL_DYNAMIC);
	return (TCL_OK);
}

/*
 * tcl_init --
 *	Create the TCL commands used by nvi.
 *
 * PUBLIC: int tcl_init __P((GS *));
 */
int
tcl_init(gp)
	GS *gp;
{
	gp->tcl_interp = Tcl_CreateInterp();
	if (Tcl_Init(gp->tcl_interp) == TCL_ERROR)
		return (1);

#define	TCC(name, function) {						\
	Tcl_CreateCommand(gp->tcl_interp, name, function,		\
	    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);		\
}
	TCC("viAppendLine", tcl_aline);
	TCC("viDelLine", tcl_dline);
	TCC("viEndScreen", tcl_escreen);
	TCC("viFindScreen", tcl_fscreen);
	TCC("viGetCursor", tcl_getcursor);
	TCC("viGetLine", tcl_gline);
	TCC("viGetMark", tcl_getmark);
	TCC("viGetOpt", tcl_opts_get);
	TCC("viInsertLine", tcl_iline);
	TCC("viLastLine", tcl_lline);
	TCC("viMapKey", tcl_map);
	TCC("viMsg", tcl_msg);
	TCC("viNewScreen", tcl_iscreen);
	TCC("viSetCursor", tcl_setcursor);
	TCC("viSetLine", tcl_sline);
	TCC("viSetMark", tcl_setmark);
	TCC("viSetOpt", tcl_opts_set);
	TCC("viSwitchScreen", tcl_swscreen);
	TCC("viUnmapKey", tcl_unmap);

	return (0);
}

/*
 * getscreenid --
 *	Get the specified screen pointer.
 *
 * XXX
 * This is fatal.  We can't post a message into vi that we're unable to find
 * the screen without first finding the screen... So, this must be the first
 * thing a Tcl routine does, and, if it fails, the last as well.
 */
static int 
getscreenid(interp, spp, id, name)
	Tcl_Interp *interp;
	SCR **spp;
	char *id, *name;
{
	int scr_no;
	char buf[64];

	if (id != NULL && getint(interp, "screen id", id, &scr_no))
		return (1);
	if ((*spp = api_fscreen(scr_no, name)) == NULL) {
		(void)snprintf(buf, sizeof(buf),
		    "unknown screen id: %s", name == NULL ? id : name);
		Tcl_SetResult(interp, buf, TCL_VOLATILE);
		return (1);
	}
	return (0);
}

/*
 * getint --
 *	Get a Tcl integer.
 *
 * XXX
 * This code assumes that both recno_t and size_t are larger than ints.
 */
static int
getint(interp, msg, s, intp)
	Tcl_Interp *interp;
	char *msg, *s;
	int *intp;
{
	char buf[64];

	if (Tcl_GetInt(interp, s, intp) == TCL_ERROR)
		return (1);
	if (*intp < 0) {
		(void)snprintf(buf, sizeof(buf),
		    "illegal %s %s: may not be negative", msg, s);
		Tcl_SetResult(interp, buf, TCL_VOLATILE);
		return (1);
	}
	return (0);
}

/*
 * msghandler --
 *	Tcl message routine so that error messages are processed in
 *	Tcl, not in nvi.
 */
static void
msghandler(sp, mtype, msg, len)
	SCR *sp;
	mtype_t mtype;
	char *msg;
	size_t len;
{
	/* Replace the trailing <newline> with an EOS. */
	msg[len - 1] = '\0';

	Tcl_SetResult(sp->gp->tcl_interp, msg, TCL_VOLATILE);
}
