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
static const char sccsid[] = "@(#)tk_main.c	8.18 (Berkeley) 9/24/96";
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
#include "tki.h"
#include "pathnames.h"

GS *__global_list;				/* GLOBAL: List of screens. */
sigset_t __sigblockset;				/* GLOBAL: Blocked signals. */

static GS	*gs_init __P((char *));
static void	 killsig __P((SCR *));
static void	 perr __P((char *, char *));
static void	 sig_end __P((GS *));
static int	 sig_init __P((GS *));
static int	 tcl_init __P((GS *));
static void	 tcl_err __P((TK_PRIVATE *));

/*
 * main --
 *	This is the main loop for the standalone Tcl/Tk editor.
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	static int reenter;
	GS *gp;
	TK_PRIVATE *tkp;
	size_t rows, cols;
	int rval;
	char **p_av, **t_av, *script;

	/* If loaded at 0 and jumping through a NULL pointer, stop. */
	if (reenter++)
		abort();

	/* Create and initialize the global structure. */
	__global_list = gp = gs_init(argv[0]);

	/* Initialize Tk/Tcl. */
	if (tcl_init(gp))
		exit (1);

	/*
	 * Strip out any arguments that the common editor doesn't understand
	 * (i.e. the Tk/Tcl arguments).  Search for -i first, it's the Tk/Tcl
	 * startup script and needs to be run first.
	 *
	 * XXX
	 * There's no way to portably call getopt twice.
	 */
	script = "init.tcl";
	for (p_av = t_av = argv;;) {
		if (*t_av == NULL) {
			*p_av = NULL;
			break;
		}
		if (!strcmp(*t_av, "--")) {
			while ((*p_av++ = *t_av++) != NULL);
			break;
		}
		if (!memcmp(*t_av, "-i", sizeof("-i") - 1)) {
			if (t_av[0][2] != '\0') {
				script = t_av[0] + 2;
				++t_av;
				--argc;
				continue;
			}
			if (t_av[1] != NULL) {
				script = t_av[1];
				t_av += 2;
				argc -= 2;
				continue;
			}
		}
		*p_av++ = *t_av++;
	}
	for (p_av = t_av = argv;;) {
		if (*t_av == NULL) {
			*p_av = NULL;
			break;
		}
		if (!strcmp(*t_av, "--")) {
			while ((*p_av++ = *t_av++) != NULL);
			break;
		}
		if (t_av[1] != NULL &&
		   (!memcmp(*t_av, "-background", sizeof("-background") - 1) ||
		    !memcmp(*t_av, "-bg", sizeof("-bg") - 1) ||
		    !memcmp(*t_av, "-borderwidth", sizeof("-borderwidth") - 1)||
		    !memcmp(*t_av, "-bd", sizeof("-bd") - 1) ||
		    !memcmp(*t_av, "-foreground", sizeof("-foreground") - 1) ||
		    !memcmp(*t_av, "-fg", sizeof("-fg") - 1) ||
		    !memcmp(*t_av, "-font", sizeof("-font") - 1))) {
			if (Tcl_VarEval(tkp->interp, ".t configure ",
			    t_av[0], " ", t_av[1], NULL) == TCL_ERROR)
				tcl_err(tkp);
			t_av += 2;
			argc -= 2;
			continue;
		}
		if (!memcmp(*t_av, "-geometry", sizeof("-geometry") - 1)) {
			if (Tcl_VarEval(tkp->interp, "wm geometry . ",
			    *t_av + sizeof("-geometry") - 1, NULL) == TCL_ERROR)
				tcl_err(tkp);
			++t_av;
			--argc;
			continue;
		}
		*p_av++ = *t_av++;
	}

	/* Load the initial Tcl/Tk script. */
	tkp = GTKP(gp);
	if (Tcl_EvalFile(tkp->interp, script) == TCL_ERROR)
		tcl_err(tkp);

	/* Add the terminal type to the global structure. */
	if ((OG_D_STR(gp, GO_TERM) =
	    OG_STR(gp, GO_TERM) = strdup("tkterm")) == NULL)
		perr(gp->progname, NULL);

	/* Figure out how big the screen is. */
	if (tk_ssize(NULL, 0, &rows, &cols, NULL))
		exit (1);

	/* Add the rows and columns to the global structure. */
	OG_VAL(gp, GO_LINES) = OG_D_VAL(gp, GO_LINES) = rows;
	OG_VAL(gp, GO_COLUMNS) = OG_D_VAL(gp, GO_COLUMNS) = cols;

	/* Start catching signals. */
	if (sig_init(gp))
		exit (1);

	/* Run ex/vi. */
	rval = editor(gp, argc, argv);

	/* Clean up signals. */
	sig_end(gp);

	/* Clean up the terminal. */
	(void)tk_quit(gp);

	/* If a killer signal arrived, pretend we just got it. */
	if (tkp->killersig) {
		(void)signal(tkp->killersig, SIG_DFL);
		(void)kill(getpid(), tkp->killersig);
		/* NOTREACHED */
	}

	/* Free the global and TK private areas. */
#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
	free(tkp);
	free(gp);
#endif

	exit (rval);
}

/*
 * gs_init --
 *	Create and partially initialize the GS structure.
 */
static GS *
gs_init(name)
	char *name;
{
	TK_PRIVATE *tkp;
	GS *gp;
	int fd;
	char *p;

	/* Figure out what our name is. */
	if ((p = strrchr(name, '/')) != NULL)
		name = p + 1;

	/* Allocate the global structure. */
	CALLOC_NOMSG(NULL, gp, GS *, 1, sizeof(GS));

	/* Allocate the CL private structure. */
	if (gp != NULL)
		CALLOC_NOMSG(NULL, tkp, TK_PRIVATE *, 1, sizeof(TK_PRIVATE));
	if (gp == NULL || tkp == NULL)
		perr(name, NULL);
	gp->tk_private = tkp;
	TAILQ_INIT(&tkp->evq);

	/* Initialize the list of curses functions. */
	gp->scr_addstr = tk_addstr;
	gp->scr_attr = tk_attr;
	gp->scr_baud = tk_baud;
	gp->scr_bell = tk_bell;
	gp->scr_busy = NULL;
	gp->scr_clrtoeol = tk_clrtoeol;
	gp->scr_cursor = tk_cursor;
	gp->scr_deleteln = tk_deleteln;
	gp->scr_event = tk_event;
	gp->scr_ex_adjust = tk_ex_adjust;
	gp->scr_fmap = tk_fmap;
	gp->scr_insertln = tk_insertln;
	gp->scr_keyval = tk_keyval;
	gp->scr_move = tk_move;
	gp->scr_msg = NULL;
	gp->scr_optchange = tk_optchange;
	gp->scr_refresh = tk_refresh;
	gp->scr_rename = tk_rename;
	gp->scr_screen = tk_screen;
	gp->scr_suspend = tk_suspend;
	gp->scr_usage = tk_usage;

	/*
	 * We expect that if we've lost our controlling terminal that the
	 * open() (but not the tcgetattr()) will fail.
	 */
	if (isatty(STDIN_FILENO)) {
		if (tcgetattr(STDIN_FILENO, &tkp->orig) == -1)
			goto tcfail;
	} else if ((fd = open(_PATH_TTY, O_RDONLY, 0)) != -1) {
		if (tcgetattr(fd, &tkp->orig) == -1)
tcfail:			perr(name, "tcgetattr");
		(void)close(fd);
	}

	gp->progname = name;
	return (gp);
}

/*
 *  tcl_init --
 *	Get Tcl/Tk up and running.
 */
static int
tcl_init(gp)
	GS *gp;
{
	TK_PRIVATE *tkp;

	tkp = GTKP(gp);
	if ((tkp->interp = Tcl_CreateInterp()) == NULL)
		tcl_err(tkp);
	/* XXX: Tk 4.1 has an incompatible change. */
#if (TK_MAJOR_VERSION == 4) && (TK_MINOR_VERSION == 0)
	if (Tk_CreateMainWindow(tkp->interp, NULL, "vi", "Vi") == NULL)
		tcl_err(tkp);
#endif
	if (Tcl_Init(tkp->interp) == TCL_ERROR)
		tcl_err(tkp);
	if (Tk_Init(tkp->interp) == TCL_ERROR)
		tcl_err(tkp);

	/* Shared variables. */
	(void)Tcl_LinkVar(tkp->interp,
	    "tk_cursor_row", (char *)&tkp->tk_cursor_row, TCL_LINK_INT);
	(void)Tcl_LinkVar(tkp->interp,
	    "tk_cursor_col", (char *)&tkp->tk_cursor_col, TCL_LINK_INT);
	(void)Tcl_LinkVar(tkp->interp,
	    "tk_ssize_row", (char *)&tkp->tk_ssize_row, TCL_LINK_INT);
	(void)Tcl_LinkVar(tkp->interp,
	    "tk_ssize_col", (char *)&tkp->tk_ssize_col, TCL_LINK_INT);

	/* Functions called by Tcl script. */
	Tcl_CreateCommand(tkp->interp, "tk_key", tk_key, tkp, NULL);
	Tcl_CreateCommand(tkp->interp, "tk_op", tk_op, tkp, NULL);
	Tcl_CreateCommand(tkp->interp, "tk_opt_init", tk_opt_init, tkp, NULL);
	Tcl_CreateCommand(tkp->interp, "tk_opt_set", tk_opt_set, tkp, NULL);
	Tcl_CreateCommand(tkp->interp, "tk_version", tk_version, tkp, NULL);

	/* Other initialization. */
	if (Tcl_Eval(tkp->interp, "wm geometry . =80x28+0+0") == TCL_ERROR)
		tcl_err(tkp);
	return (0);
}

/*
 * tcl_err --
 *	Tcl/Tk error message during initialization.
 */
static void
tcl_err(tkp)
	TK_PRIVATE *tkp;
{
	(void)fprintf(stderr, "%s\n", tkp->interp->result != NULL ?
	    tkp->interp->result : "Tcl/Tk: initialization error");
	(void)tk_usage();
	exit (1);
}

#define	GLOBAL_TKP \
	TK_PRIVATE *tkp = GTKP(__global_list);
static void
h_hup(signo)
	int signo;
{
	GLOBAL_TKP;

	F_SET(tkp, TK_SIGHUP);
	tkp->killersig = SIGHUP;
}

static void
h_int(signo)
	int signo;
{
	GLOBAL_TKP;

	F_SET(tkp, TK_SIGINT);
}

static void
h_term(signo)
	int signo;
{
	GLOBAL_TKP;

	F_SET(tkp, TK_SIGTERM);
	tkp->killersig = SIGTERM;
}

static void
h_winch(signo)
	int signo;
{
	GLOBAL_TKP;

	F_SET(tkp, TK_SIGWINCH);
}
#undef	GLOBAL_TKP

/*
 * sig_init --
 *	Initialize signals.
 */
static int
sig_init(gp)
	GS *gp;
{
	TK_PRIVATE *tkp;
	struct sigaction act;

	tkp = GTKP(gp);

	(void)sigemptyset(&__sigblockset);

	/*
	 * Use sigaction(2), not signal(3), since we don't always want to
	 * restart system calls.  The example is when waiting for a command
	 * mode keystroke and SIGWINCH arrives.  Besides, you can't portably
	 * restart system calls (thanks, POSIX!).
	 */
#define	SETSIG(signal, handler, off) {					\
	if (sigaddset(&__sigblockset, signal))				\
		goto err;						\
	act.sa_handler = handler;					\
	sigemptyset(&act.sa_mask);					\
	act.sa_flags = 0;						\
	if (sigaction(signal, &act, &tkp->oact[off]))			\
		goto err;						\
}
#undef SETSIG
	return (0);

err:	perr(gp->progname, NULL);
	return (1);
}

/*
 * sig_end --
 *	End signal setup.
 */
static void
sig_end(gp)
	GS *gp;
{
	TK_PRIVATE *tkp;

	tkp = GTKP(gp);
	(void)sigaction(SIGHUP, NULL, &tkp->oact[INDX_HUP]);
	(void)sigaction(SIGINT, NULL, &tkp->oact[INDX_INT]);
	(void)sigaction(SIGTERM, NULL, &tkp->oact[INDX_TERM]);
	(void)sigaction(SIGWINCH, NULL, &tkp->oact[INDX_WINCH]);
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
