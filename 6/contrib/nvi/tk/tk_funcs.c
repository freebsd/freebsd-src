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
static const char sccsid[] = "@(#)tk_funcs.c	8.11 (Berkeley) 9/23/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "../vi/vi.h"
#include "tki.h"

/*
 * tk_addstr --
 *	Add len bytes from the string at the cursor, advancing the cursor.
 *
 * PUBLIC: int tk_addstr __P((SCR *, const char *, size_t));
 */
int
tk_addstr(sp, str, len)
	SCR *sp;
	const char *str;
	size_t len;
{
	TK_PRIVATE *tkp;
	int iv;
	char buf[20];

	iv = 0;

	tkp = TKP(sp);
	if (iv)
		(void)Tcl_Eval(tkp->interp, "tk_standout");

	(void)snprintf(buf, sizeof(buf), "%d ", (int)len);
	if ((Tcl_VarEval(tkp->interp,
	    "tk_addstr ", buf, "{", str, "}", NULL) != TCL_OK))
		return (1);

	if (iv)
		(void)Tcl_Eval(tkp->interp, "tk_standend");
	return (0);
}

/*
 * tk_attr --
 *	Toggle a screen attribute on/off.
 *
 * PUBLIC: int tk_attr __P((SCR *, scr_attr_t, int));
 */
int
tk_attr(sp, attribute, on)
	SCR *sp;
	scr_attr_t attribute;
	int on;
{
	TK_PRIVATE *tkp;

	tkp = TKP(sp);
	switch (attribute) {
	case SA_ALTERNATE:			/* No alternate screen. */
		break;
	case SA_INVERSE:
		if (on)
			(void)Tcl_Eval(tkp->interp, "tk_standout");
		else
			(void)Tcl_Eval(tkp->interp, "tk_standend");
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * tk_baud --
 *	Return the baud rate.
 *
 * PUBLIC: int tk_baud __P((SCR *, u_long *));
 */
int
tk_baud(sp, ratep)
	SCR *sp;
	u_long *ratep;
{
	*ratep = 9600;
	return (0);
}

/*
 * tk_bell --
 *	Ring the bell/flash the screen.
 *
 * PUBLIC: int tk_bell __P((SCR *));
 */
int
tk_bell(sp)
	SCR *sp;
{
	TK_PRIVATE *tkp;

	tkp = TKP(sp);
	return (Tcl_Eval(tkp->interp, "tk_flash") != TCL_OK);
}

/*
 * tk_clrtoeol --
 *	Clear from the current cursor to the end of the line.
 *
 * PUBLIC: int tk_clrtoeol __P((SCR *));
 */
int
tk_clrtoeol(sp)
	SCR *sp;
{
	TK_PRIVATE *tkp;

	tkp = TKP(sp);
	return (Tcl_Eval(tkp->interp, "tk_clrtoeol") != TCL_OK);
}

/*
 * tk_cursor --
 *	Return the current cursor position.
 *
 * PUBLIC: int tk_cursor __P((SCR *, size_t *, size_t *));
 */
int
tk_cursor(sp, yp, xp)
	SCR *sp;
	size_t *yp, *xp;
{
	TK_PRIVATE *tkp;

	tkp = TKP(sp);
	*yp = (tkp->tk_cursor_row - 1) - sp->woff;
	*xp = tkp->tk_cursor_col;
	return (0);
}

/*
 * tk_deleteln --
 *	Delete the current line, scrolling all lines below it.
 *
 * PUBLIC: int tk_deleteln __P((SCR *));
 */
int
tk_deleteln(sp)
	SCR *sp;
{
	TK_PRIVATE *tkp;

	tkp = TKP(sp);
	return (Tcl_Eval(tkp->interp, "tk_deleteln") != TCL_OK);
}

/* 
 * tk_ex_adjust --
 *	Adjust the screen for ex.
 *
 * PUBLIC: int tk_ex_adjust __P((SCR *, exadj_t));
 */
int
tk_ex_adjust(sp, action)
	SCR *sp;
	exadj_t action;
{
	abort();
	/* NOTREACHED */
}

/*
 * tk_insertln --
 *	Push down the current line, discarding the bottom line.
 *
 * PUBLIC: int tk_insertln __P((SCR *));
 */
int
tk_insertln(sp)
	SCR *sp;
{
	TK_PRIVATE *tkp;

	tkp = TKP(sp);
	return (Tcl_Eval(tkp->interp, "tk_insertln") != TCL_OK);
}

/*
 * tk_keyval --
 *	Return the value for a special key.
 *
 * PUBLIC: int tk_keyval __P((SCR *, scr_keyval_t, CHAR_T *, int *));
 */
int
tk_keyval(sp, val, chp, dnep)
	SCR *sp;
	scr_keyval_t val;
	CHAR_T *chp;
	int *dnep;
{
	TK_PRIVATE *tkp;

	/*
	 * VEOF, VERASE and VKILL are required by POSIX 1003.1-1990,
	 * VWERASE is a 4BSD extension.
	 */
	tkp = TKP(sp);
	switch (val) {
	case KEY_VEOF:
		*dnep = (*chp = tkp->orig.c_cc[VEOF]) == _POSIX_VDISABLE;
		break;
	case KEY_VERASE:
		*dnep = (*chp = tkp->orig.c_cc[VERASE]) == _POSIX_VDISABLE;
		break;
	case KEY_VKILL:
		*dnep = (*chp = tkp->orig.c_cc[VKILL]) == _POSIX_VDISABLE;
		break;
#ifdef VWERASE
	case KEY_VWERASE:
		*dnep = (*chp = tkp->orig.c_cc[VWERASE]) == _POSIX_VDISABLE;
		break;
#endif
	default:
		*dnep = 1;
		break;
	}
	return (0);
}

/*
 * tk_move --
 *	Move the cursor.
 *
 * PUBLIC: int tk_move __P((SCR *, size_t, size_t));
 */
int
tk_move(sp, lno, cno)
	SCR *sp;
	size_t lno, cno;
{
	TK_PRIVATE *tkp;
	char buf[40];

	(void)snprintf(buf, sizeof(buf), "%d %d", RLNO(sp, lno), cno);

	tkp = TKP(sp);
	return (Tcl_VarEval(tkp->interp, "tk_move ", buf, NULL) != TCL_OK);
}

/*
 * tk_refresh --
 *	Refresh the screen.
 *
 * PUBLIC: int tk_refresh __P((SCR *, int));
 */
int
tk_refresh(sp, repaint)
	SCR *sp;
	int repaint;
{
	TK_PRIVATE *tkp;

	/*
	 * If repaint is set, the editor is telling us that we don't know
	 * what's on the screen, so we have to repaint from scratch.
	 *
	 * XXX
	 * I have no idea how to do this in Tk.  My guess is that we have
	 * to delete all of the text and call the editor with an E_REPAINT
	 * event.
	 */
	if (repaint) {
	}

	tkp = TKP(sp);
	return (Tcl_Eval(tkp->interp, "update idletasks") != TCL_OK);
}

/*
 * tk_rename --
 *	Rename the file.
 *
 * PUBLIC: int tk_rename __P((SCR *));
 */
int
tk_rename(sp)
	SCR *sp;
{
	TK_PRIVATE *tkp;

	tkp = TKP(sp);
	return (Tcl_VarEval(tkp->interp,
	    "tk_rename ", sp->frp->name, NULL) != TCL_OK);
}

/*
 * tk_suspend --
 *	Suspend a screen.
 *
 * PUBLIC: int tk_suspend __P((SCR *, int *));
 */
int
tk_suspend(sp, allowedp)
	SCR *sp;
	int *allowedp;
{
	*allowedp = 0;
	return (0);
}

/*
 * tk_usage --
 *	Print out the Tk/Tcl usage messages.
 * 
 * PUBLIC: void tk_usage __P((void));
 */
void
tk_usage()
{
#define	USAGE "\
usage: tkvi [-eFlRrSv] [-c command] [-bg color] [-fg color]\n\
	    [-geometry widthxheight+x+y] [-i script] [-t tag] [-w size]\n\
	    [file ...]\n"
	(void)fprintf(stderr, "%s", USAGE);
#undef	USAGE
}
