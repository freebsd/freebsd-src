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
static const char sccsid[] = "@(#)tk_screen.c	8.9 (Berkeley) 5/24/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "tki.h"

/*
 * tk_screen --
 *	Initialize/shutdown the Tcl/Tk screen.
 *
 * PUBLIC: int tk_screen __P((SCR *, u_int32_t));
 */
int
tk_screen(sp, flags)
	SCR *sp;
	u_int32_t flags;
{
	TK_PRIVATE *tkp;

	tkp = TKP(sp);

	/* See if we're already in the right mode. */
	if (LF_ISSET(SC_VI) && F_ISSET(sp, SC_SCR_VI))
		return (0);

	/* Ex isn't possible. */
	if (LF_ISSET(SC_EX))
		return (1);

	/* Initialize terminal based information. */
	if (tk_term_init(sp)) 
		return (1);

	/* Put up the first file name. */
	if (tk_rename(sp))
		return (1);

	F_SET(tkp, TK_SCR_VI_INIT);
	return (0);
}

/*
 * tk_quit --
 *	Shutdown the screens.
 *
 * PUBLIC: int tk_quit __P((GS *));
 */
int
tk_quit(gp)
	GS *gp;
{
	TK_PRIVATE *tkp;
	int rval;

	/* Clean up the terminal mappings. */
	rval = tk_term_end(gp);

	tkp = GTKP(gp);
	F_CLR(tkp, TK_SCR_VI_INIT);

	return (rval);
}
