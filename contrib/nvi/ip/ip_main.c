/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)ip_main.c	8.3 (Berkeley) 10/13/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include "../common/common.h"
#include "ip.h"

static void	   ip_func_std __P((GS *));
static IP_PRIVATE *ip_init __P((GS *, char *));
static void	   perr __P((char *, char *));

/*
 * main --
 *      This is the main loop for the vi-as-library editor.
 */
int
ip_main(argc, argv, gp, ip_arg)
	int argc;
	char *argv[], *ip_arg;
	GS *gp;
{
	EVENT ev;
	IP_PRIVATE *ipp;
	IP_BUF ipb;
	int rval;

	/* Create and partially initialize the IP structure. */
	if ((ipp = ip_init(gp, ip_arg)) == NULL)
		return (1);

	/* Add the terminal type to the global structure. */
	if ((OG_D_STR(gp, GO_TERM) =
	    OG_STR(gp, GO_TERM) = strdup("ip_curses")) == NULL)
		perr(gp->progname, NULL);

	/*
	 * Figure out how big the screen is -- read events until we get
	 * the rows and columns.
	 */
	do {
		if (ip_event(NULL, &ev, 0, 0))
			return (1);
	} while (ev.e_event != E_EOF && ev.e_event != E_ERR &&
	    ev.e_event != E_QUIT && ev.e_event != E_WRESIZE &&
	    ev.e_event != E_SIGHUP && ev.e_event != E_SIGTERM);
	if (ev.e_event != E_WRESIZE)
		return (1);

	/* Run ex/vi. */
	rval = editor(gp, argc, argv);

	/* Clean up the screen. */
	(void)ip_quit(gp);

	/* Free the global and IP private areas. */
#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
	free(ipp);
	free(gp);
#endif

	return (rval);
}

/*
 * ip_init --
 *	Create and partially initialize the GS structure.
 */
static IP_PRIVATE *
ip_init(gp, ip_arg)
	GS *gp;
	char *ip_arg;
{
	IP_PRIVATE *ipp;
	char *ep;

	/* Allocate the IP private structure. */
	CALLOC_NOMSG(NULL, ipp, IP_PRIVATE *, 1, sizeof(IP_PRIVATE));
	if (ipp == NULL)
		perr(gp->progname,  NULL);
	gp->ip_private = ipp;

	/*
	 * Crack ip_arg -- it's of the form #.#, where the first number is the
	 * file descriptor from the screen, the second is the file descriptor
	 * to the screen.
	 */
	if (!isdigit(ip_arg[0]))
		goto usage;
	ipp->i_fd = strtol(ip_arg, &ep, 10);
	if (ep[0] != '.' || !isdigit(ep[1]))
		goto usage;
	ipp->o_fd = strtol(++ep, &ep, 10);
	if (ep[0] != '\0') {
usage:		ip_usage();
		return (NULL);
	}

	/* Initialize the list of ip functions. */
	ip_func_std(gp);

	return (ipp);
}

/*
 * ip_func_std --
 *	Initialize the standard ip functions.
 */
static void
ip_func_std(gp)
	GS *gp;
{
	gp->scr_addstr = ip_addstr;
	gp->scr_attr = ip_attr;
	gp->scr_baud = ip_baud;
	gp->scr_bell = ip_bell;
	gp->scr_busy = ip_busy;
	gp->scr_clrtoeol = ip_clrtoeol;
	gp->scr_cursor = ip_cursor;
	gp->scr_deleteln = ip_deleteln;
	gp->scr_event = ip_event;
	gp->scr_ex_adjust = ip_ex_adjust;
	gp->scr_fmap = ip_fmap;
	gp->scr_insertln = ip_insertln;
	gp->scr_keyval = ip_keyval;
	gp->scr_move = ip_move;
	gp->scr_msg = NULL;
	gp->scr_optchange = ip_optchange;
	gp->scr_refresh = ip_refresh;
	gp->scr_rename = ip_rename;
	gp->scr_screen = ip_screen;
	gp->scr_suspend = ip_suspend;
	gp->scr_usage = ip_usage;
}

/*
 * perr --
 *	Print system error.
 */
static void
perr(name, msg)
	char *name, *msg;
{
	(void)fprintf(stderr, "%s:", name);
	if (msg != NULL)
		(void)fprintf(stderr, "%s:", msg);
	(void)fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
}
