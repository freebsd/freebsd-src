/*-
 * Copyright (c) 1992, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	8.65 (Berkeley) 1/23/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"
#include "tag.h"

static int	 exrc_isok __P((SCR *, char *, int));
static void	 gs_end __P((GS *));
static GS	*gs_init __P((void));
static void	 h_hup __P((int));
static void	 h_term __P((int));
static void	 h_winch __P((int));
static void	 obsolete __P((char *[]));
static void	 usage __P((int));

GS *__global_list;			/* GLOBAL: List of screens. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	extern char *optarg;
	static int reenter;		/* STATIC: Re-entrancy check. */
	struct sigaction act;
	GS *gp;
	FREF *frp;
	SCR *sp;
	u_int flags, saved_vi_mode;
	int ch, eval, flagchk, readonly, silent, snapshot;
	char *excmdarg, *myname, *p, *rec_f, *tag_f, *trace_f, *wsizearg;
	char path[MAXPATHLEN];

	/* Stop if indirecting through a NULL pointer. */
	if (reenter++)
		abort();

	/* Set screen type and mode based on the program name. */
	readonly = 0;
	if ((myname = strrchr(*argv, '/')) == NULL)
		myname = *argv;
	else
		++myname;
	if (!strcmp(myname, "ex") || !strcmp(myname, "nex"))
		LF_INIT(S_EX);
	else {
		/* View is readonly. */
		if (!strcmp(myname, "view"))
			readonly = 1;
		LF_INIT(S_VI_CURSES);
	}
	saved_vi_mode = S_VI_CURSES;

	/* Convert old-style arguments into new-style ones. */
	obsolete(argv);

	/* Parse the arguments. */
	flagchk = '\0';
	excmdarg = rec_f = tag_f = trace_f = wsizearg = NULL;
	silent = 0;
	snapshot = 1;
	while ((ch = getopt(argc, argv, "c:eFlRr:sT:t:vw:x:")) != EOF)
		switch (ch) {
		case 'c':		/* Run the command. */
			excmdarg = optarg;
			break;
		case 'e':		/* Ex mode. */
			LF_CLR(S_SCREENS);
			LF_SET(S_EX);
			break;
		case 'F':		/* No snapshot. */
			snapshot = 0;
			break;
		case 'l':
			if (flagchk != '\0' && flagchk != 'l')
				errx(1,
				    "only one of -%c and -l may be specified.",
				    flagchk);
			flagchk = 'l';
			break;
		case 'R':		/* Readonly. */
			readonly = 1;
			break;
		case 'r':		/* Recover. */
			if (flagchk == 'r')
				errx(1,
				    "only one recovery file may be specified.");
			if (flagchk != '\0')
				errx(1,
				    "only one of -%c and -r may be specified.",
				    flagchk);
			flagchk = 'r';
			rec_f = optarg;
			break;
		case 's':
			if (!LF_ISSET(S_EX))
				errx(1, "-s only applicable to ex.");
			silent = 1;
			break;
		case 'T':		/* Trace. */
			trace_f = optarg;
			break;
		case 't':		/* Tag. */
			if (flagchk == 't')
				errx(1,
				    "only one tag file may be specified.");
			if (flagchk != '\0')
				errx(1,
				    "only one of -%c and -t may be specified.",
				    flagchk);
			flagchk = 't';
			tag_f = optarg;
			break;
		case 'v':		/* Vi mode. */
			LF_CLR(S_SCREENS);
			LF_SET(S_VI_CURSES);
			break;
		case 'w':
			wsizearg = optarg;
			break;
		case 'x':
			if (!strcmp(optarg, "aw")) {
				LF_CLR(S_SCREENS);
				LF_SET(S_VI_XAW);
				saved_vi_mode = S_VI_XAW;
				break;
			}
			/* FALLTHROUGH */
		case '?':
		default:
			usage(LF_ISSET(S_EX));
		}
	argc -= optind;
	argv += optind;

	/* Build and initialize the GS structure. */
	__global_list = gp = gs_init();
		
	if (snapshot)
		F_SET(gp, G_SNAPSHOT);

	/*
	 * Build and initialize the first/current screen.  This is a bit
	 * tricky.  If an error is returned, we may or may not have a
	 * screen structure.  If we have a screen structure, put it on a
	 * display queue so that the error messages get displayed.
	 */
	if (screen_init(NULL, &sp, flags)) {
		if (sp != NULL)
			CIRCLEQ_INSERT_HEAD(&__global_list->dq, sp, q);
		goto err;
	}
	sp->saved_vi_mode = saved_vi_mode;
	CIRCLEQ_INSERT_HEAD(&__global_list->dq, sp, q);

	if (trace_f != NULL) {
#ifdef DEBUG
		if ((gp->tracefp = fopen(optarg, "w")) == NULL)
			err(1, "%s", optarg);
		(void)fprintf(gp->tracefp, "\n===\ntrace: open %s\n", optarg);
#else
		msgq(sp, M_ERR, "-T support not compiled into this version.");
#endif
	}

	if (set_window_size(sp, 0, 0))	/* Set the window size. */
		goto err;
	if (opts_init(sp))		/* Options initialization. */
		goto err;
	if (readonly)			/* Global read-only bit. */
		O_SET(sp, O_READONLY);
	if (silent) {			/* Ex batch mode. */
		O_CLR(sp, O_AUTOPRINT);
		O_CLR(sp, O_PROMPT);
		O_CLR(sp, O_VERBOSE);
		O_CLR(sp, O_WARN);
		F_SET(sp, S_EXSILENT);
	}
	if (wsizearg != NULL) {
		ARGS *av[2], a, b;
		if (strtol(optarg, &p, 10) < 0 || *p)
			errx(1, "illegal window size -- %s", optarg);
		(void)snprintf(path, sizeof(path), "window=%s", optarg);
		a.bp = (CHAR_T *)path;
		a.len = strlen(path);
		b.bp = NULL;
		b.len = 0;
		av[0] = &a;
		av[1] = &b;
		if (opts_set(sp, av))
			 msgq(sp, M_ERR,
			     "Unable to set command line window option");
	}

	/* Keymaps, special keys, must follow option initializations. */
	if (term_init(sp))
		goto err;

#ifdef	DIGRAPHS
	if (digraph_init(sp))		/* Digraph initialization. */
		goto err;
#endif

	/*
	 * Source the system, environment, ~user and local .exrc values.
	 * Vi historically didn't check ~user/.exrc if the environment
	 * variable EXINIT was set.  This is all done before the file is
	 * read in because things in the .exrc information can set, for
	 * example, the recovery directory.
	 *
	 * !!!
	 * While nvi can handle any of the options settings of historic vi,
	 * the converse is not true.  Since users are going to have to have
	 * files and environmental variables that work with both, we use nvi
	 * versions if they exist, otherwise the historic ones.
	 */
	if (!silent) {
		if (exrc_isok(sp, _PATH_SYSEXRC, 1))
			(void)ex_cfile(sp, NULL, _PATH_SYSEXRC);

		/* Source the {N,}EXINIT environment variable. */
		if ((p = getenv("NEXINIT")) != NULL ||
		    (p = getenv("EXINIT")) != NULL)
			if ((p = strdup(p)) == NULL) {
				msgq(sp, M_SYSERR, NULL);
				goto err;
			} else {
				(void)ex_icmd(sp, NULL, p, strlen(p));
				free(p);
			}
		else if ((p = getenv("HOME")) != NULL && *p) {
			(void)snprintf(path,
			    sizeof(path), "%s/%s", p, _PATH_NEXRC);
			if (exrc_isok(sp, path, 0))
				(void)ex_cfile(sp, NULL, path);
			else {
				(void)snprintf(path,
				    sizeof(path), "%s/%s", p, _PATH_EXRC);
				if (exrc_isok(sp, path, 0))
					(void)ex_cfile(sp, NULL, path);
			}
		}
		/*
		 * !!!
		 * According to O'Reilly ("Learning the VI Editor", Fifth Ed.,
		 * May 1992, page 106), System V release 3.2 and later, has an
		 * option "[no]exrc", causing vi to not "read .exrc files in
		 * the current directory unless you first set the exrc option
		 * in your home directory's .exrc file".  Yeah, right.  Did
		 * someone actually believe that users would change their home
		 * .exrc file based on whether or not they wanted to source the
		 * current local .exrc?  Or that users would want ALL the local
		 * .exrc files on some systems, and none of them on others?
		 * I think not.
		 *
		 * Apply the same tests to local .exrc files that are applied
		 * to any other .exrc file.
		 */
		if (exrc_isok(sp, _PATH_EXRC, 0))
			(void)ex_cfile(sp, NULL, _PATH_EXRC);
	}

	/* List recovery files if -l specified. */
	if (flagchk == 'l')
		exit(rcv_list(sp));

	/* Use a tag file or recovery file if specified. */
	if (tag_f != NULL && ex_tagfirst(sp, tag_f))
		goto err;
	else if (rec_f != NULL && rcv_read(sp, rec_f))
		goto err;

	/* Append any remaining arguments as file names. */
	if (*argv != NULL)
		for (; *argv != NULL; ++argv)
			if (file_add(sp, NULL, *argv, 0) == NULL)
				goto err;

	/*
	 * If no recovery or tag file, get an EXF structure.
	 * If no argv file, use a temporary file.
	 */
	if (tag_f == NULL && rec_f == NULL) {
		if ((frp = file_first(sp)) == NULL &&
		    (frp = file_add(sp, NULL, NULL, 1)) == NULL)
			goto err;
		if (file_init(sp, frp, NULL, 0))
			goto err;
	}

	/* Set up the argument pointer. */
	sp->a_frp = sp->frp;

	/*
	 * Initialize the signals.  Use sigaction(2), not signal(3), because
	 * we don't want to always restart system calls on 4BSD systems.  It
	 * would be nice in some cases to restart system calls, but SA_RESTART
	 * is a 4BSD extension so we can't use it.
	 *
	 * SIGWINCH, SIGHUP, SIGTERM:
	 *	Catch and set a global bit.
	 */
	act.sa_handler = h_hup;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void)sigaction(SIGHUP, &act, NULL);
	act.sa_handler = h_term;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void)sigaction(SIGTERM, &act, NULL);
	act.sa_handler = h_winch;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void)sigaction(SIGWINCH, &act, NULL);

	/*
	 * SIGQUIT:
	 *	Always ignore.
	 */
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void)sigaction(SIGQUIT, &act, NULL);

	/*
	 * If there's an initial command, push it on the command stack.
	 * Historically, it was always an ex command, not vi in vi mode
	 * or ex in ex mode.  So, make it look like an ex command to vi.
	 */
	if (excmdarg != NULL)
		if (IN_EX_MODE(sp)) {
			if (term_push(sp, excmdarg, strlen(excmdarg), 0, 0))
				goto err;
		} else if (IN_VI_MODE(sp)) {
			if (term_push(sp, "\n", 1, 0, 0))
				goto err;
			if (term_push(sp, excmdarg, strlen(excmdarg), 0, 0))
				goto err;
			if (term_push(sp, ":", 1, 0, 0))
				goto err;
		}
		
	/* Vi reads from the terminal. */
	if (!F_ISSET(gp, G_ISFROMTTY) && !F_ISSET(sp, S_EX)) {
		msgq(sp, M_ERR, "Vi's standard input must be a terminal.");
		goto err;
	}

	for (;;) {
		if (sp->s_edit(sp, sp->ep))
			goto err;

		/*
		 * Edit the next screen on the display queue, or, move
		 * a screen from the hidden queue to the display queue.
		 */
		if ((sp = __global_list->dq.cqh_first) ==
		    (void *)&__global_list->dq)
			if ((sp = __global_list->hq.cqh_first) !=
			    (void *)&__global_list->hq) {
				CIRCLEQ_REMOVE(&sp->gp->hq, sp, q);
				CIRCLEQ_INSERT_TAIL(&sp->gp->dq, sp, q);
			} else
				break;

		/*
		 * The screen type may have changed -- reinitialize the
		 * functions in case it has.
		 */
		switch (F_ISSET(sp, S_SCREENS)) {
		case S_EX:
			if (sex_screen_init(sp))
				goto err;
			break;
		case S_VI_CURSES:
			if (svi_screen_init(sp))
				goto err;
			break;
		case S_VI_XAW:
			if (xaw_screen_init(sp))
				goto err;
			break;
		default:
			abort();
		}
	}

	eval = 0;
	if (0)
err:		eval = 1;

	/*
	 * NOTE: sp may be GONE when the screen returns, so only
	 * the gp can be trusted.
	 */
	gs_end(gp);

	/*
	 * XXX
	 * Make absolutely sure that the modes are restored correctly.
	 *
	 * This should no longer be needed, and it's here to handle what I
	 * believe are SunOS/Solaris curses problems.  The problem is that
	 * for some unknown reason, when endwin() is called in the svi
	 * routines, it isn't resetting the terminal correctly.  I have not
	 * been able to figure it out, so this resets the terminal to the
	 * right modes regardless.  The problem is that, in most tty driver
	 * implementations, you can only reset the terminal modes once
	 * (changing from !ICANON to ICANON) without losing the re-parsing
	 * effect on the pending input.  This means that this "fix" will make
	 * other systems mess up characters typed after the quit command to
	 * vi but before vi actually exits.
	 */
	if (F_ISSET(gp, G_ISFROMTTY))
		(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &gp->original_termios);
	exit(eval);
}

/*
 * gs_init --
 *	Build and initialize the GS structure.
 */
static GS *
gs_init()
{
	GS *gp;
	int fd;

	CALLOC_NOMSG(NULL, gp, GS *, 1, sizeof(GS));
	if (gp == NULL)
		err(1, NULL);

	CIRCLEQ_INIT(&gp->dq);
	CIRCLEQ_INIT(&gp->hq);
	LIST_INIT(&gp->msgq);

	/* Structures shared by screens so stored in the GS structure. */
	CALLOC_NOMSG(NULL, gp->tty, IBUF *, 1, sizeof(IBUF));
	if (gp->tty == NULL)
		err(1, NULL);

	LIST_INIT(&gp->cutq);
	LIST_INIT(&gp->seqq);

	/* Set a flag if we're reading from the tty. */
	if (isatty(STDIN_FILENO))
		F_SET(gp, G_ISFROMTTY);

	/*
	 * XXX
	 * Set a flag and don't do terminal sets/resets if the input isn't
	 * from a tty.  Under all circumstances put reasonable things into
	 * the original_termios field, as some routines (seq.c:seq_save()
	 * and term.c:term_init()) want values for special characters.
	 */
	if (F_ISSET(gp, G_ISFROMTTY)) {
		if (tcgetattr(STDIN_FILENO, &gp->original_termios))
			err(1, "tcgetattr");
	} else {
		if ((fd = open(_PATH_TTY, O_RDONLY, 0)) == -1)
			err(1, "%s", _PATH_TTY);
		if (tcgetattr(fd, &gp->original_termios))
			err(1, "tcgetattr");
		(void)close(fd);
	}

	return (gp);
}


/*
 * gs_end --
 *	End the GS structure.
 */
static void
gs_end(gp)
	GS *gp;
{
	MSG *mp;
	SCR *sp;
	char *tty;

	/* Reset anything that needs resetting. */
	if (gp->flags & G_SETMODE)			/* O_MESG */
		if ((tty = ttyname(STDERR_FILENO)) == NULL)
			warn("ttyname");
		else if (chmod(tty, gp->origmode) < 0)
			warn("%s", tty);

	/* Ring the bell if scheduled. */
	if (F_ISSET(gp, G_BELLSCHED))
		(void)fprintf(stderr, "\07");		/* \a */

	/* If there are any remaining screens, flush their messages. */
	for (sp = __global_list->dq.cqh_first;
	    sp != (void *)&__global_list->dq; sp = sp->q.cqe_next)
		for (mp = sp->msgq.lh_first;
		    mp != NULL && !(F_ISSET(mp, M_EMPTY)); mp = mp->q.le_next) 
			(void)fprintf(stderr, "%.*s\n", (int)mp->len, mp->mbuf);
	for (sp = __global_list->hq.cqh_first;
	    sp != (void *)&__global_list->hq; sp = sp->q.cqe_next)
		for (mp = sp->msgq.lh_first;
		    mp != NULL && !(F_ISSET(mp, M_EMPTY)); mp = mp->q.le_next) 
			(void)fprintf(stderr, "%.*s\n", (int)mp->len, mp->mbuf);
	/* Flush messages on the global queue. */
	for (mp = gp->msgq.lh_first;
	    mp != NULL && !(F_ISSET(mp, M_EMPTY)); mp = mp->q.le_next) 
		(void)fprintf(stderr, "%.*s\n", (int)mp->len, mp->mbuf);

	if (gp->special_key != NULL)
		FREE(gp->special_key, MAX_FAST_KEY);

	/*
	 * DON'T FREE THE GLOBAL STRUCTURE -- WE DIDN'T TURN
	 * OFF SIGNALS/TIMERS, SO IT MAY STILL BE REFERENCED.
	 */
}

/*
 * h_hup --
 *	Handle SIGHUP.
 */
static void
h_hup(signo)
	int signo;
{
	F_SET(__global_list, G_SIGHUP);

	/*
	 * If we're asleep, just die.
	 *
	 * XXX
	 * This isn't right if the windows are independent.
	 */
	if (F_ISSET(__global_list, G_SLEEPING))
		rcv_hup();
}

/*
 * h_term --
 *	Handle SIGTERM.
 */
static void
h_term(signo)
	int signo;
{
	F_SET(__global_list, G_SIGTERM);

	/*
	 * If we're asleep, just die.
	 *
	 * XXX
	 * This isn't right if the windows are independent.
	 */
	if (F_ISSET(__global_list, G_SLEEPING))
		rcv_term();
}

/*
 * h_winch --
 *	Handle SIGWINCH.
 */
static void
h_winch(signo)
	int signo;
{
	F_SET(__global_list, G_SIGWINCH);
}

/*
 * exrc_isok --
 *	Check a .exrc for source-ability.
 */
static int
exrc_isok(sp, path, rootok)
	SCR *sp;
	char *path;
	int rootok;
{
	struct stat sb;
	uid_t uid;
	char *emsg, buf[MAXPATHLEN];

	/* Check for the file's existence. */
	if (stat(path, &sb))
		return (0);

	/*
	 * !!!
	 * Historically, vi did not read the .exrc files if they were owned
	 * by someone other than the user, unless the undocumented option
	 * sourceany was set.  We don't support the sourceany option.  We
	 * check that the user (or root, for system files) owns the file and
	 * require that it not be writeable by anyone other than the owner.
	 */

	/* Owned by the user or root. */
	uid = getuid();
	if (rootok) {
		if (sb.st_uid != uid && sb.st_uid != 0) {
			emsg = "not owned by you or root";
			goto err;
		}
	} else
		if (sb.st_uid != uid) {
			emsg = "not owned by you";
			goto err;
		}

	/* Not writeable by anyone but the owner. */
	if (sb.st_mode & (S_IWGRP | S_IWOTH)) {
		emsg = "writeable by a user other than the owner";
err:		if (strchr(path, '/') == NULL &&
		    getcwd(buf, sizeof(buf)) != NULL)
			msgq(sp, M_ERR,
			    "%s/%s: not sourced: %s.", buf, path, emsg);
		else
			msgq(sp, M_ERR,
			    "%s: not sourced: %s.", path, emsg);
		return (0);
	}
	return (1);
}

static void
obsolete(argv)
	char *argv[];
{
	size_t len;
	char *p, *myname;

	/*
	 * Translate old style arguments into something getopt will like.
	 * Make sure it's not text space memory, because ex changes the
	 * strings.
	 *	Change "+" into "-c$".
	 *	Change "+<anything else>" into "-c<anything else>".
	 *	Change "-" into "-s"
	 *	Change "-r" into "-l"
	 */
	for (myname = argv[0]; *++argv;)
		if (argv[0][0] == '+') {
			if (argv[0][1] == '\0') {
				MALLOC_NOMSG(NULL, argv[0], char *, 4);
				if (argv[0] == NULL)
					err(1, NULL);
				(void)strcpy(argv[0], "-c$");
			} else  {
				p = argv[0];
				len = strlen(argv[0]);
				MALLOC_NOMSG(NULL, argv[0], char *, len + 2);
				if (argv[0] == NULL)
					err(1, NULL);
				argv[0][0] = '-';
				argv[0][1] = 'c';
				(void)strcpy(argv[0] + 2, p + 1);
			}
		} else if (argv[0][0] == '-') {
			if (argv[0][1] == 'r') {
				if (argv[0][2] == '\0' && argv[1] == NULL)
					argv[0][1] = 'l';
			} else if (argv[0][1] == '\0') {
				MALLOC_NOMSG(NULL, argv[0], char *, 3);
				if (argv[0] == NULL)
					err(1, NULL);
				(void)strcpy(argv[0], "-s");
			}
		}
}

static void
usage(is_ex)
	int is_ex;
{
#define	EX_USAGE \
	"usage: ex [-eFlRsv] [-c command] [-r file] [-t tag] [-w size] [-x aw]"
#define	VI_USAGE \
	"usage: vi [-eFlRv] [-c command] [-r file] [-t tag] [-w size] [-x aw]"

	(void)fprintf(stderr, "%s\n", is_ex ? EX_USAGE : VI_USAGE);
	exit(1);
}
