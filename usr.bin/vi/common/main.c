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
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static const char sccsid[] = "@(#)main.c	8.105 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "compat.h"
#include <db.h>
#include <regex.h>
#include <pathnames.h>

#include "vi.h"
#include "excmd.h"
#include "../ex/tag.h"

enum rc { NOEXIST, NOPERM, OK };

static enum rc	 exrc_isok __P((SCR *, struct stat *, char *, int, int));
static void	 gs_end __P((GS *));
static GS	*gs_init __P((void));
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
	struct stat hsb, lsb;
	GS *gp;
	FREF *frp;
	SCR *sp;
	u_int flags, saved_vi_mode;
	int ch, eval, flagchk, readonly, silent, snapshot;
	char *excmdarg, *myname, *p, *tag_f, *trace_f, *wsizearg;
	char path[MAXPATHLEN];

	/* Stop if indirecting through a NULL pointer. */
	if (reenter++)
		abort();

#ifdef GDBATTACH
	(void)printf("%u waiting...\n", getpid());
	(void)read(0, &eval, 1);
#endif

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
	excmdarg = tag_f = trace_f = wsizearg = NULL;
	silent = 0;
	snapshot = 1;
	while ((ch = getopt(argc, argv, "c:eFRrsT:t:vw:X:")) != EOF)
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
		case 'R':		/* Readonly. */
			readonly = 1;
			break;
		case 'r':		/* Recover. */
			if (flagchk == 't')
				errx(1,
				    "only one of -r and -t may be specified.");
			flagchk = 'r';
			break;
		case 's':
			silent = 1;
			break;
		case 'T':		/* Trace. */
			trace_f = optarg;
			break;
		case 't':		/* Tag. */
			if (flagchk == 'r')
				errx(1,
				    "only one of -r and -t may be specified.");
			if (flagchk == 't')
				errx(1,
				    "only one tag file may be specified.");
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
		case 'X':
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

	/* Silent is only applicable to ex. */
	if (silent && !LF_ISSET(S_EX))
		errx(1, "-s only applicable to ex.");

	/* Build and initialize the GS structure. */
	__global_list = gp = gs_init();

	/*
	 * If not reading from a terminal, it's like -s was specified.
	 * Vi always reads from the terminal, so fail if it's not a
	 * terminal.
	 */
	if (!F_ISSET(gp, G_STDIN_TTY)) {
		silent = 1;
		if (!LF_ISSET(S_EX)) {
			msgq(NULL, M_ERR,
			    "Vi's standard input must be a terminal");
			goto err;
		}
	}

	/*
	 * Build and initialize the first/current screen.  This is a bit
	 * tricky.  If an error is returned, we may or may not have a
	 * screen structure.  If we have a screen structure, put it on a
	 * display queue so that the error messages get displayed.
	 *
	 * !!!
	 * Signals not on, no need to block them for queue manipulation.
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
		if ((gp->tracefp = fopen(trace_f, "w")) == NULL)
			err(1, "%s", trace_f);
		(void)fprintf(gp->tracefp, "\n===\ntrace: open %s\n", trace_f);
#else
		msgq(sp, M_ERR, "-T support not compiled into this version");
#endif
	}

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
		errno = 0;
		if (strtol(wsizearg, &p, 10) < 0 || errno || *p)
			errx(1, "illegal window size -- %s.", wsizearg);
		(void)snprintf(path, sizeof(path), "window=%s", wsizearg);
		a.bp = (CHAR_T *)path;
		a.len = strlen(path);
		b.bp = NULL;
		b.len = 0;
		av[0] = &a;
		av[1] = &b;
		if (opts_set(sp, NULL, av))
			 msgq(sp, M_ERR,
			     "Unable to set command line window size option");
	}

	/* Keymaps, special keys, must follow option initializations. */
	if (term_init(sp))
		goto err;

#ifdef	DIGRAPHS
	if (digraph_init(sp))		/* Digraph initialization. */
		goto err;
#endif

	/*
	 * Source the system, environment, $HOME and local .exrc values.
	 * Vi historically didn't check $HOME/.exrc if the environment
	 * variable EXINIT was set.  This is all done before the file is
	 * read in, because things in the .exrc information can set, for
	 * example, the recovery directory.
	 *
	 * !!!
	 * While nvi can handle any of the options settings of historic vi,
	 * the converse is not true.  Since users are going to have to have
	 * files and environmental variables that work with both, we use nvi
	 * versions of both the $HOME and local startup files if they exist,
	 * otherwise the historic ones.
	 *
	 * !!!
	 * For a discussion of permissions and when what .exrc files are
	 * read, see the the comment above the exrc_isok() function below.
	 *
	 * !!!
	 * If the user started the historic of vi in $HOME, vi read the user's
	 * .exrc file twice, as $HOME/.exrc and as ./.exrc.  We avoid this, as
	 * it's going to make some commands behave oddly, and I can't imagine
	 * anyone depending on it.
	 */
	if (!silent) {
		switch (exrc_isok(sp, &hsb, _PATH_SYSEXRC, 1, 0)) {
		case NOEXIST:
		case NOPERM:
			break;
		case OK:
			(void)ex_cfile(sp, NULL, _PATH_SYSEXRC, 0);
			break;
		}

		if ((p = getenv("NEXINIT")) != NULL ||
		    (p = getenv("EXINIT")) != NULL)
			if ((p = strdup(p)) == NULL) {
				msgq(sp, M_SYSERR, NULL);
				goto err;
			} else {
				F_SET(sp, S_VLITONLY);
				(void)ex_icmd(sp, NULL, p, strlen(p), 0);
				F_CLR(sp, S_VLITONLY);
				free(p);
			}
		else if ((p = getenv("HOME")) != NULL && *p) {
			(void)snprintf(path,
			    sizeof(path), "%s/%s", p, _PATH_NEXRC);
			switch (exrc_isok(sp, &hsb, path, 0, 1)) {
			case NOEXIST:
				(void)snprintf(path,
				    sizeof(path), "%s/%s", p, _PATH_EXRC);
				if (exrc_isok(sp, &hsb, path, 0, 1) == OK)
					(void)ex_cfile(sp, NULL, path, 0);
				break;
			case NOPERM:
				break;
			case OK:
				(void)ex_cfile(sp, NULL, path, 0);
				break;
			}
		}

		if (O_ISSET(sp, O_EXRC))
			switch (exrc_isok(sp, &lsb, _PATH_NEXRC, 0, 0)) {
			case NOEXIST:
				if (exrc_isok(sp,
				    &lsb, _PATH_EXRC, 0, 0) == OK &&
				    (lsb.st_dev != hsb.st_dev ||
				    lsb.st_ino != hsb.st_ino))
					(void)ex_cfile(sp, NULL, _PATH_EXRC, 0);
				break;
			case NOPERM:
				break;
			case OK:
				if (lsb.st_dev != hsb.st_dev ||
				    lsb.st_ino != hsb.st_ino)
					(void)ex_cfile(sp,
					    NULL, _PATH_NEXRC, 0);
				break;
			}
	}

	/* List recovery files if -r specified without file arguments. */
	if (flagchk == 'r' && argv[0] == NULL)
		exit(rcv_list(sp));

	/* Set the file snapshot flag. */
	if (snapshot)
		F_SET(gp, G_SNAPSHOT);

	/* Use a tag file if specified. */
	if (tag_f != NULL && ex_tagfirst(sp, tag_f))
		goto err;

	/*
	 * Append any remaining arguments as file names.  Files are
	 * recovery files if -r specified.
	 */
	if (*argv != NULL) {
		sp->argv = sp->cargv = argv;
		F_SET(sp, S_ARGNOFREE);
		if (flagchk == 'r')
			F_SET(sp, S_ARGRECOVER);
	}

	/*
	 * If the tag option hasn't already created a file, create one.
	 * If no files as arguments, use a temporary file.
	 */
	if (tag_f == NULL) {
		if ((frp = file_add(sp,
		    sp->argv == NULL ? NULL : (CHAR_T *)(sp->argv[0]))) == NULL)
			goto err;
		if (F_ISSET(sp, S_ARGRECOVER))
			F_SET(frp, FR_RECOVER);
		if (file_init(sp, frp, NULL, 0))
			goto err;
	}

	/*
	 * If there's an initial command, push it on the command stack.
	 * Historically, it was always an ex command, not vi in vi mode
	 * or ex in ex mode.  So, make it look like an ex command to vi.
	 *
	 * !!!
	 * Historically, all such commands were executed with the last
	 * line of the file as the current line, and not the first, so
	 * set up vi to be at the end of the file.
	 */
	if (excmdarg != NULL)
		if (IN_EX_MODE(sp)) {
			if (term_push(sp, "\n", 1, 0))
				goto err;
			if (term_push(sp, excmdarg, strlen(excmdarg), 0))
				goto err;
		} else if (IN_VI_MODE(sp)) {
			if (term_push(sp, "\n", 1, 0))
				goto err;
			if (term_push(sp, excmdarg, strlen(excmdarg), 0))
				goto err;
			if (term_push(sp, ":", 1, 0))
				goto err;
			if (file_lline(sp, sp->ep, &sp->frp->lno))
				goto err;
			F_SET(sp->frp, FR_CURSORSET);
		}

	/* Set up signals. */
	if (sig_init(sp))
		goto err;

	for (;;) {
		/* Ignore errors -- other screens may succeed. */
		(void)sp->s_edit(sp, sp->ep);

		/*
		 * Edit the next screen on the display queue, or, move
		 * a screen from the hidden queue to the display queue.
		 */
		if ((sp = __global_list->dq.cqh_first) ==
		    (void *)&__global_list->dq)
			if ((sp = __global_list->hq.cqh_first) !=
			    (void *)&__global_list->hq) {
				SIGBLOCK(__global_list);
				CIRCLEQ_REMOVE(&sp->gp->hq, sp, q);
				CIRCLEQ_INSERT_TAIL(&sp->gp->dq, sp, q);
				SIGUNBLOCK(__global_list);
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

	/*
	 * !!!
	 * Signals not on, no need to block them for queue manipulation.
	 */
	CIRCLEQ_INIT(&gp->dq);
	CIRCLEQ_INIT(&gp->hq);
	LIST_INIT(&gp->msgq);

	/* Structures shared by screens so stored in the GS structure. */
	CALLOC_NOMSG(NULL, gp->tty, IBUF *, 1, sizeof(IBUF));
	if (gp->tty == NULL)
		err(1, NULL);

	CIRCLEQ_INIT(&gp->dcb_store.textq);
	LIST_INIT(&gp->cutq);
	LIST_INIT(&gp->seqq);

	/* Set a flag if we're reading from the tty. */
	if (isatty(STDIN_FILENO))
		F_SET(gp, G_STDIN_TTY);

	/*
	 * Set the G_STDIN_TTY flag.  It's purpose is to avoid setting and
	 * resetting the tty if the input isn't from there.
	 *
	 * Set the G_TERMIOS_SET flag.  It's purpose is to avoid using the
	 * original_termios information (mostly special character values)
	 * if it's not valid.  We expect that if we've lost our controlling
	 * terminal that the open() (but not the tcgetattr()) will fail.
	 */
	if (F_ISSET(gp, G_STDIN_TTY)) {
		if (tcgetattr(STDIN_FILENO, &gp->original_termios) == -1)
			err(1, "tcgetattr");
		F_SET(gp, G_TERMIOS_SET);
	} else if ((fd = open(_PATH_TTY, O_RDONLY, 0)) != -1) {
		if (tcgetattr(fd, &gp->original_termios) == -1)
			err(1, "tcgetattr");
		F_SET(gp, G_TERMIOS_SET);
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

	/* Default buffer storage. */
	(void)text_lfree(&gp->dcb_store.textq);

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
			(void)fprintf(stderr,
			    "%.*s.\n", (int)mp->len, mp->mbuf);
	for (sp = __global_list->hq.cqh_first;
	    sp != (void *)&__global_list->hq; sp = sp->q.cqe_next)
		for (mp = sp->msgq.lh_first;
		    mp != NULL && !(F_ISSET(mp, M_EMPTY)); mp = mp->q.le_next)
			(void)fprintf(stderr,
			    "%.*s.\n", (int)mp->len, mp->mbuf);
	/* Flush messages on the global queue. */
	for (mp = gp->msgq.lh_first;
	    mp != NULL && !(F_ISSET(mp, M_EMPTY)); mp = mp->q.le_next)
		(void)fprintf(stderr, "%.*s.\n", (int)mp->len, mp->mbuf);

	/*
	 * DON'T FREE THE GLOBAL STRUCTURE -- WE DIDN'T TURN
	 * OFF SIGNALS/TIMERS, SO IT MAY STILL BE REFERENCED.
	 */
}

/*
 * exrc_isok --
 *	Check a .exrc file for source-ability.
 *
 * !!!
 * Historically, vi read the $HOME and local .exrc files if they were owned
 * by the user's real ID, or the "sourceany" option was set, regardless of
 * any other considerations.  We no longer support the sourceany option as
 * it's a security problem of mammoth proportions.  We require the system
 * .exrc file to be owned by root, the $HOME .exrc file to be owned by the
 * user's effective ID (or that the user's effective ID be root) and the
 * local .exrc files to be owned by the user's effective ID.  In all cases,
 * the file cannot be writeable by anyone other than its owner.
 * 
 * In O'Reilly ("Learning the VI Editor", Fifth Ed., May 1992, page 106),
 * it notes that System V release 3.2 and later has an option "[no]exrc".
 * The behavior is that local .exrc files are read only if the exrc option
 * is set.  The default for the exrc option was off, so, by default, local
 * .exrc files were not read.  The problem this was intended to solve was
 * that System V permitted users to give away files, so there's no possible
 * ownership or writeability test to ensure that the file is safe.
 * 
 * POSIX 1003.2-1992 standardized exrc as an option.  It required the exrc
 * option to be off by default, thus local .exrc files are not to be read
 * by default.  The Rationale noted (incorrectly) that this was a change
 * to historic practice, but correctly noted that a default of off improves
 * system security.  POSIX also required that vi check the effective user
 * ID instead of the real user ID, which is why we've switched from historic
 * practice.
 * 
 * We initialize the exrc variable to off.  If it's turned on by the system
 * or $HOME .exrc files, and the local .exrc file passes the ownership and
 * writeability tests, then we read it.  This breaks historic 4BSD practice,
 * but it gives us a measure of security on systems where users can give away
 * files.
 */
static enum rc
exrc_isok(sp, sbp, path, rootown, rootid)
	SCR *sp;
	struct stat *sbp;
	char *path;
	int rootown, rootid;
{
	uid_t euid;
	char *emsg, buf[MAXPATHLEN];

	/* Check for the file's existence. */
	if (stat(path, sbp))
		return (NOEXIST);

	/* Check ownership permissions. */
	euid = geteuid();
	if (!(rootown && sbp->st_uid == 0) && 
	    !(rootid && euid == 0) && sbp->st_uid != euid) {
		emsg = rootown ?
		    "not owned by you or root" : "not owned by you";
		goto denied;
	}

	/* Check writeability. */
	if (sbp->st_mode & (S_IWGRP | S_IWOTH)) {
		emsg = "writeable by a user other than the owner";
denied:		if (strchr(path, '/') == NULL &&
		    getcwd(buf, sizeof(buf)) != NULL)
			msgq(sp, M_ERR,
			    "%s/%s: not sourced: %s", buf, path, emsg);
		else
			msgq(sp, M_ERR,
			    "%s: not sourced: %s", path, emsg);
		return (NOPERM);
	}
	return (OK);
}

static void
obsolete(argv)
	char *argv[];
{
	size_t len;
	char *p;

	/*
	 * Translate old style arguments into something getopt will like.
	 * Make sure it's not text space memory, because ex changes the
	 * strings.
	 *	Change "+" into "-c$".
	 *	Change "+<anything else>" into "-c<anything else>".
	 *	Change "-" into "-s"
	 */
	while (*++argv)
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
		} else if (argv[0][0] == '-' && argv[0][1] == '\0') {
			MALLOC_NOMSG(NULL, argv[0], char *, 3);
			if (argv[0] == NULL)
				err(1, NULL);
			(void)strcpy(argv[0], "-s");
		}
}

static void
usage(is_ex)
	int is_ex;
{
#define	EX_USAGE \
    "ex [-eFRrsv] [-c command] [-t tag] [-w size] [files ...]"
#define	VI_USAGE \
    "vi [-eFRrv] [-c command] [-t tag] [-w size] [files ...]"

	(void)fprintf(stderr, "usage: %s\n", is_ex ? EX_USAGE : VI_USAGE);
	exit(1);
}
