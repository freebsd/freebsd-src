/*-
 * Copyright (c) 1992, 1993, 1994
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
static char sccsid[] = "@(#)ex_usage.c	8.14 (Berkeley) 3/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

/*
 * ex_help -- :help
 *	Display help message.
 */
int
ex_help(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	(void)ex_printf(EXCOOKIE,
	    "To see the list of vi commands, enter \":viusage<CR>\"\n");
	(void)ex_printf(EXCOOKIE,
	    "To see the list of ex commands, enter \":exusage<CR>\"\n");
	(void)ex_printf(EXCOOKIE,
	    "For an ex command usage statement enter \":exusage [cmd]<CR>\"\n");
	(void)ex_printf(EXCOOKIE,
	    "For a vi key usage statement enter \":viusage [key]<CR>\"\n");
	(void)ex_printf(EXCOOKIE, "To exit, enter \":q!\"\n");
	return (0);
}

/*
 * ex_usage -- :exusage [cmd]
 *	Display ex usage strings.
 */
int
ex_usage(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	ARGS *ap;
	EXCMDLIST const *cp;
	char *name;

	switch (cmdp->argc) {
	case 1:
		ap = cmdp->argv[0];
		for (cp = cmds; cp->name != NULL &&
		    memcmp(ap->bp, cp->name, ap->len); ++cp);
		if (cp->name == NULL)
			(void)ex_printf(EXCOOKIE,
			    "The %.*s command is unknown.",
			    (int)ap->len, ap->bp);
		else {
			(void)ex_printf(EXCOOKIE,
			    "Command: %s\n  Usage: %s\n", cp->help, cp->usage);
			/*
			 * !!!
			 * The "visual" command has two modes, one from ex,
			 * one from the vi colon line.  Don't ask.
			 */
			if (cp != &cmds[C_VISUAL_EX] &&
			    cp != &cmds[C_VISUAL_VI])
				break;
			if (cp == &cmds[C_VISUAL_EX])
				cp = &cmds[C_VISUAL_VI];
			else
				cp = &cmds[C_VISUAL_EX];
			(void)ex_printf(EXCOOKIE,
			    "Command: %s\n  Usage: %s\n", cp->help, cp->usage);
		}
		break;
	case 0:
		F_SET(sp, S_INTERRUPTIBLE);
		for (cp = cmds; cp->name != NULL; ++cp) {
			/* The ^D command has an unprintable name. */
			if (cp == &cmds[C_SCROLL])
				name = "^D";
			else
				name = cp->name;
			(void)ex_printf(EXCOOKIE,
			    "%*s: %s\n", MAXCMDNAMELEN, name, cp->help);
		}
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * ex_viusage -- :viusage [key]
 *	Display vi usage strings.
 */
int
ex_viusage(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	VIKEYS const *kp;
	int key;

	switch (cmdp->argc) {
	case 1:
		key = cmdp->argv[0]->bp[0];
		if (key > MAXVIKEY)
			goto nokey;

		/* Special case: '[' and ']' commands. */
		if ((key == '[' || key == ']') && cmdp->argv[0]->bp[1] != key)
			goto nokey;

		kp = &vikeys[key];
		if (kp->func == NULL)
nokey:			(void)ex_printf(EXCOOKIE,
			    "The %s key has no current meaning",
			    charname(sp, key));
		else
			(void)ex_printf(EXCOOKIE,
			    "  Key:%s%s\nUsage: %s\n",
			    isblank(*kp->help) ? "" : " ", kp->help, kp->usage);
		break;
	case 0:
		F_SET(sp, S_INTERRUPTIBLE);
		for (key = 0; key <= MAXVIKEY; ++key) {
			kp = &vikeys[key];
			if (kp->help != NULL)
				(void)ex_printf(EXCOOKIE, "%s\n", kp->help);
		}
		break;
	default:
		abort();
	}
	return (0);
}
