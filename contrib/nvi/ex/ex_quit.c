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
static const char sccsid[] = "@(#)ex_quit.c	10.7 (Berkeley) 4/27/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"

/*
 * ex_quit -- :quit[!]
 *	Quit.
 *
 * PUBLIC: int ex_quit __P((SCR *, EXCMD *));
 */
int
ex_quit(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	int force;

	force = FL_ISSET(cmdp->iflags, E_C_FORCE);

	/* Check for file modifications, or more files to edit. */
	if (file_m2(sp, force) || ex_ncheck(sp, force))
		return (1);

	F_SET(sp, force ? SC_EXIT_FORCE : SC_EXIT);
	return (0);
}
