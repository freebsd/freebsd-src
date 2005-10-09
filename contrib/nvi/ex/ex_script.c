/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Brian Hirt.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)ex_script.c	10.30 (Berkeley) 9/24/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS5_PTY
#include <sys/stropts.h>
#endif
#include <sys/time.h>
#include <sys/wait.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>		/* XXX: OSF/1 bug: include before <grp.h> */
#include <grp.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "../vi/vi.h"
#include "script.h"
#include "pathnames.h"

static void	sscr_check __P((SCR *));
static int	sscr_getprompt __P((SCR *));
static int	sscr_init __P((SCR *));
static int	sscr_insert __P((SCR *));
static int	sscr_matchprompt __P((SCR *, char *, size_t, size_t *));
static int	sscr_pty __P((int *, int *, char *, struct termios *, void *));
static int	sscr_setprompt __P((SCR *, char *, size_t));

/*
 * ex_script -- : sc[ript][!] [file]
 *	Switch to script mode.
 *
 * PUBLIC: int ex_script __P((SCR *, EXCMD *));
 */
int
ex_script(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	/* Vi only command. */
	if (!F_ISSET(sp, SC_VI)) {
		msgq(sp, M_ERR,
		    "150|The script command is only available in vi mode");
		return (1);
	}

	/* Switch to the new file. */
	if (cmdp->argc != 0 && ex_edit(sp, cmdp))
		return (1);

	/* Create the shell, figure out the prompt. */
	if (sscr_init(sp))
		return (1);

	return (0);
}

/*
 * sscr_init --
 *	Create a pty setup for a shell.
 */
static int
sscr_init(sp)
	SCR *sp;
{
	SCRIPT *sc;
	char *sh, *sh_path;

	/* We're going to need a shell. */
	if (opts_empty(sp, O_SHELL, 0))
		return (1);

	MALLOC_RET(sp, sc, SCRIPT *, sizeof(SCRIPT));
	sp->script = sc;
	sc->sh_prompt = NULL;
	sc->sh_prompt_len = 0;

	/*
	 * There are two different processes running through this code.
	 * They are the shell and the parent.
	 */
	sc->sh_master = sc->sh_slave = -1;

	if (tcgetattr(STDIN_FILENO, &sc->sh_term) == -1) {
		msgq(sp, M_SYSERR, "tcgetattr");
		goto err;
	}

	/*
	 * Turn off output postprocessing and echo.
	 */
	sc->sh_term.c_oflag &= ~OPOST;
	sc->sh_term.c_cflag &= ~(ECHO|ECHOE|ECHONL|ECHOK);

#ifdef TIOCGWINSZ
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &sc->sh_win) == -1) {
		msgq(sp, M_SYSERR, "tcgetattr");
		goto err;
	}

	if (sscr_pty(&sc->sh_master,
	    &sc->sh_slave, sc->sh_name, &sc->sh_term, &sc->sh_win) == -1) {
		msgq(sp, M_SYSERR, "pty");
		goto err;
	}
#else
	if (sscr_pty(&sc->sh_master,
	    &sc->sh_slave, sc->sh_name, &sc->sh_term, NULL) == -1) {
		msgq(sp, M_SYSERR, "pty");
		goto err;
	}
#endif

	/*
	 * __TK__ huh?
	 * Don't use vfork() here, because the signal semantics differ from
	 * implementation to implementation.
	 */
	switch (sc->sh_pid = fork()) {
	case -1:			/* Error. */
		msgq(sp, M_SYSERR, "fork");
err:		if (sc->sh_master != -1)
			(void)close(sc->sh_master);
		if (sc->sh_slave != -1)
			(void)close(sc->sh_slave);
		return (1);
	case 0:				/* Utility. */
		/*
		 * XXX
		 * So that shells that do command line editing turn it off.
		 */
		(void)setenv("TERM", "emacs", 1);
		(void)setenv("TERMCAP", "emacs:", 1);
		(void)setenv("EMACS", "t", 1);

		(void)setsid();
#ifdef TIOCSCTTY
		/*
		 * 4.4BSD allocates a controlling terminal using the TIOCSCTTY
		 * ioctl, not by opening a terminal device file.  POSIX 1003.1
		 * doesn't define a portable way to do this.  If TIOCSCTTY is
		 * not available, hope that the open does it.
		 */
		(void)ioctl(sc->sh_slave, TIOCSCTTY, 0);
#endif
		(void)close(sc->sh_master);
		(void)dup2(sc->sh_slave, STDIN_FILENO);
		(void)dup2(sc->sh_slave, STDOUT_FILENO);
		(void)dup2(sc->sh_slave, STDERR_FILENO);
		(void)close(sc->sh_slave);

		/* Assumes that all shells have -i. */
		sh_path = O_STR(sp, O_SHELL);
		if ((sh = strrchr(sh_path, '/')) == NULL)
			sh = sh_path;
		else
			++sh;
		execl(sh_path, sh, "-i", NULL);
		msgq_str(sp, M_SYSERR, sh_path, "execl: %s");
		_exit(127);
	default:			/* Parent. */
		break;
	}

	if (sscr_getprompt(sp))
		return (1);

	F_SET(sp, SC_SCRIPT);
	F_SET(sp->gp, G_SCRWIN);
	return (0);
}

/*
 * sscr_getprompt --
 *	Eat lines printed by the shell until a line with no trailing
 *	carriage return comes; set the prompt from that line.
 */
static int
sscr_getprompt(sp)
	SCR *sp;
{
	struct timeval tv;
	CHAR_T *endp, *p, *t, buf[1024];
	SCRIPT *sc;
	fd_set fdset;
	recno_t lline;
	size_t llen, len;
	u_int value;
	int nr;

	FD_ZERO(&fdset);
	endp = buf;
	len = sizeof(buf);

	/* Wait up to a second for characters to read. */
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	sc = sp->script;
	FD_SET(sc->sh_master, &fdset);
	switch (select(sc->sh_master + 1, &fdset, NULL, NULL, &tv)) {
	case -1:		/* Error or interrupt. */
		msgq(sp, M_SYSERR, "select");
		goto prompterr;
	case  0:		/* Timeout */
		msgq(sp, M_ERR, "Error: timed out");
		goto prompterr;
	case  1:		/* Characters to read. */
		break;
	}

	/* Read the characters. */
more:	len = sizeof(buf) - (endp - buf);
	switch (nr = read(sc->sh_master, endp, len)) {
	case  0:			/* EOF. */
		msgq(sp, M_ERR, "Error: shell: EOF");
		goto prompterr;
	case -1:			/* Error or interrupt. */
		msgq(sp, M_SYSERR, "shell");
		goto prompterr;
	default:
		endp += nr;
		break;
	}

	/* If any complete lines, push them into the file. */
	for (p = t = buf; p < endp; ++p) {
		value = KEY_VAL(sp, *p);
		if (value == K_CR || value == K_NL) {
			if (db_last(sp, &lline) ||
			    db_append(sp, 0, lline, t, p - t))
				goto prompterr;
			t = p + 1;
		}
	}
	if (p > buf) {
		memmove(buf, t, endp - t);
		endp = buf + (endp - t);
	}
	if (endp == buf)
		goto more;

	/* Wait up 1/10 of a second to make sure that we got it all. */
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	switch (select(sc->sh_master + 1, &fdset, NULL, NULL, &tv)) {
	case -1:		/* Error or interrupt. */
		msgq(sp, M_SYSERR, "select");
		goto prompterr;
	case  0:		/* Timeout */
		break;
	case  1:		/* Characters to read. */
		goto more;
	}

	/* Timed out, so theoretically we have a prompt. */
	llen = endp - buf;
	endp = buf;

	/* Append the line into the file. */
	if (db_last(sp, &lline) || db_append(sp, 0, lline, buf, llen)) {
prompterr:	sscr_end(sp);
		return (1);
	}

	return (sscr_setprompt(sp, buf, llen));
}

/*
 * sscr_exec --
 *	Take a line and hand it off to the shell.
 *
 * PUBLIC: int sscr_exec __P((SCR *, recno_t));
 */
int
sscr_exec(sp, lno)
	SCR *sp;
	recno_t lno;
{
	SCRIPT *sc;
	recno_t last_lno;
	size_t blen, len, last_len, tlen;
	int isempty, matchprompt, nw, rval;
	char *bp, *p;

	/* If there's a prompt on the last line, append the command. */
	if (db_last(sp, &last_lno))
		return (1);
	if (db_get(sp, last_lno, DBG_FATAL, &p, &last_len))
		return (1);
	if (sscr_matchprompt(sp, p, last_len, &tlen) && tlen == 0) {
		matchprompt = 1;
		GET_SPACE_RET(sp, bp, blen, last_len + 128);
		memmove(bp, p, last_len);
	} else
		matchprompt = 0;

	/* Get something to execute. */
	if (db_eget(sp, lno, &p, &len, &isempty)) {
		if (isempty)
			goto empty;
		goto err1;
	}

	/* Empty lines aren't interesting. */
	if (len == 0)
		goto empty;

	/* Delete any prompt. */
	if (sscr_matchprompt(sp, p, len, &tlen)) {
		if (tlen == len) {
empty:			msgq(sp, M_BERR, "151|No command to execute");
			goto err1;
		}
		p += (len - tlen);
		len = tlen;
	}

	/* Push the line to the shell. */
	sc = sp->script;
	if ((nw = write(sc->sh_master, p, len)) != len)
		goto err2;
	rval = 0;
	if (write(sc->sh_master, "\n", 1) != 1) {
err2:		if (nw == 0)
			errno = EIO;
		msgq(sp, M_SYSERR, "shell");
		goto err1;
	}

	if (matchprompt) {
		ADD_SPACE_RET(sp, bp, blen, last_len + len);
		memmove(bp + last_len, p, len);
		if (db_set(sp, last_lno, bp, last_len + len))
err1:			rval = 1;
	}
	if (matchprompt)
		FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * sscr_input --
 *	Read any waiting shell input.
 *
 * PUBLIC: int sscr_input __P((SCR *));
 */
int
sscr_input(sp)
	SCR *sp;
{
	GS *gp;
	struct timeval poll;
	fd_set rdfd;
	int maxfd;

	gp = sp->gp;

loop:	maxfd = 0;
	FD_ZERO(&rdfd);
	poll.tv_sec = 0;
	poll.tv_usec = 0;

	/* Set up the input mask. */
	for (sp = gp->dq.cqh_first; sp != (void *)&gp->dq; sp = sp->q.cqe_next)
		if (F_ISSET(sp, SC_SCRIPT)) {
			FD_SET(sp->script->sh_master, &rdfd);
			if (sp->script->sh_master > maxfd)
				maxfd = sp->script->sh_master;
		}

	/* Check for input. */
	switch (select(maxfd + 1, &rdfd, NULL, NULL, &poll)) {
	case -1:
		msgq(sp, M_SYSERR, "select");
		return (1);
	case 0:
		return (0);
	default:
		break;
	}

	/* Read the input. */
	for (sp = gp->dq.cqh_first; sp != (void *)&gp->dq; sp = sp->q.cqe_next)
		if (F_ISSET(sp, SC_SCRIPT) &&
		    FD_ISSET(sp->script->sh_master, &rdfd) && sscr_insert(sp))
			return (1);
	goto loop;
}

/*
 * sscr_insert --
 *	Take a line from the shell and insert it into the file.
 */
static int
sscr_insert(sp)
	SCR *sp;
{
	struct timeval tv;
	CHAR_T *endp, *p, *t;
	SCRIPT *sc;
	fd_set rdfd;
	recno_t lno;
	size_t blen, len, tlen;
	u_int value;
	int nr, rval;
	char *bp;

	/* Find out where the end of the file is. */
	if (db_last(sp, &lno))
		return (1);

#define	MINREAD	1024
	GET_SPACE_RET(sp, bp, blen, MINREAD);
	endp = bp;

	/* Read the characters. */
	rval = 1;
	sc = sp->script;
more:	switch (nr = read(sc->sh_master, endp, MINREAD)) {
	case  0:			/* EOF; shell just exited. */
		sscr_end(sp);
		rval = 0;
		goto ret;
	case -1:			/* Error or interrupt. */
		msgq(sp, M_SYSERR, "shell");
		goto ret;
	default:
		endp += nr;
		break;
	}

	/* Append the lines into the file. */
	for (p = t = bp; p < endp; ++p) {
		value = KEY_VAL(sp, *p);
		if (value == K_CR || value == K_NL) {
			len = p - t;
			if (db_append(sp, 1, lno++, t, len))
				goto ret;
			t = p + 1;
		}
	}
	if (p > t) {
		len = p - t;
		/*
		 * If the last thing from the shell isn't another prompt, wait
		 * up to 1/10 of a second for more stuff to show up, so that
		 * we don't break the output into two separate lines.  Don't
		 * want to hang indefinitely because some program is hanging,
		 * confused the shell, or whatever.
		 */
		if (!sscr_matchprompt(sp, t, len, &tlen) || tlen != 0) {
			tv.tv_sec = 0;
			tv.tv_usec = 100000;
			FD_ZERO(&rdfd);
			FD_SET(sc->sh_master, &rdfd);
			if (select(sc->sh_master + 1,
			    &rdfd, NULL, NULL, &tv) == 1) {
				memmove(bp, t, len);
				endp = bp + len;
				goto more;
			}
		}
		if (sscr_setprompt(sp, t, len))
			return (1);
		if (db_append(sp, 1, lno++, t, len))
			goto ret;
	}

	/* The cursor moves to EOF. */
	sp->lno = lno;
	sp->cno = len ? len - 1 : 0;
	rval = vs_refresh(sp, 1);

ret:	FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * sscr_setprompt --
 *
 * Set the prompt to the last line we got from the shell.
 *
 */
static int
sscr_setprompt(sp, buf, len)
	SCR *sp;
	char *buf;
	size_t len;
{
	SCRIPT *sc;

	sc = sp->script;
	if (sc->sh_prompt)
		free(sc->sh_prompt);
	MALLOC(sp, sc->sh_prompt, char *, len + 1);
	if (sc->sh_prompt == NULL) {
		sscr_end(sp);
		return (1);
	}
	memmove(sc->sh_prompt, buf, len);
	sc->sh_prompt_len = len;
	sc->sh_prompt[len] = '\0';
	return (0);
}

/*
 * sscr_matchprompt --
 *	Check to see if a line matches the prompt.  Nul's indicate
 *	parts that can change, in both content and size.
 */
static int
sscr_matchprompt(sp, lp, line_len, lenp)
	SCR *sp;
	char *lp;
	size_t line_len, *lenp;
{
	SCRIPT *sc;
	size_t prompt_len;
	char *pp;

	sc = sp->script;
	if (line_len < (prompt_len = sc->sh_prompt_len))
		return (0);

	for (pp = sc->sh_prompt;
	    prompt_len && line_len; --prompt_len, --line_len) {
		if (*pp == '\0') {
			for (; prompt_len && *pp == '\0'; --prompt_len, ++pp);
			if (!prompt_len)
				return (0);
			for (; line_len && *lp != *pp; --line_len, ++lp);
			if (!line_len)
				return (0);
		}
		if (*pp++ != *lp++)
			break;
	}

	if (prompt_len)
		return (0);
	if (lenp != NULL)
		*lenp = line_len;
	return (1);
}

/*
 * sscr_end --
 *	End the pipe to a shell.
 *
 * PUBLIC: int sscr_end __P((SCR *));
 */
int
sscr_end(sp)
	SCR *sp;
{
	SCRIPT *sc;

	if ((sc = sp->script) == NULL)
		return (0);

	/* Turn off the script flags. */
	F_CLR(sp, SC_SCRIPT);
	sscr_check(sp);

	/* Close down the parent's file descriptors. */
	if (sc->sh_master != -1)
	    (void)close(sc->sh_master);
	if (sc->sh_slave != -1)
	    (void)close(sc->sh_slave);

	/* This should have killed the child. */
	(void)proc_wait(sp, (long)sc->sh_pid, "script-shell", 0, 0);

	/* Free memory. */
	free(sc->sh_prompt);
	free(sc);
	sp->script = NULL;

	return (0);
}

/*
 * sscr_check --
 *	Set/clear the global scripting bit.
 */
static void
sscr_check(sp)
	SCR *sp;
{
	GS *gp;

	gp = sp->gp;
	for (sp = gp->dq.cqh_first; sp != (void *)&gp->dq; sp = sp->q.cqe_next)
		if (F_ISSET(sp, SC_SCRIPT)) {
			F_SET(gp, G_SCRWIN);
			return;
		}
	F_CLR(gp, G_SCRWIN);
}

#ifdef HAVE_SYS5_PTY
static int ptys_open __P((int, char *));
static int ptym_open __P((char *));

static int
sscr_pty(amaster, aslave, name, termp, winp)
	int *amaster, *aslave;
	char *name;
	struct termios *termp;
	void *winp;
{
	int master, slave, ttygid;

	/* open master terminal */
	if ((master = ptym_open(name)) < 0)  {
		errno = ENOENT;	/* out of ptys */
		return (-1);
	}

	/* open slave terminal */
	if ((slave = ptys_open(master, name)) >= 0) {
		*amaster = master;
		*aslave = slave;
	} else {
		errno = ENOENT;	/* out of ptys */
		return (-1);
	}

	if (termp)
		(void) tcsetattr(slave, TCSAFLUSH, termp);
#ifdef TIOCSWINSZ
	if (winp != NULL)
		(void) ioctl(slave, TIOCSWINSZ, (struct winsize *)winp);
#endif
	return (0);
}

/*
 * ptym_open --
 *	This function opens a master pty and returns the file descriptor
 *	to it.  pts_name is also returned which is the name of the slave.
 */
static int
ptym_open(pts_name)
	char *pts_name;
{
	int fdm;
	char *ptr, *ptsname();

	strcpy(pts_name, _PATH_SYSV_PTY);
	if ((fdm = open(pts_name, O_RDWR)) < 0 )
		return (-1);

	if (grantpt(fdm) < 0) {
		close(fdm);
		return (-2);
	}

	if (unlockpt(fdm) < 0) {
		close(fdm);
		return (-3);
	}

	if (unlockpt(fdm) < 0) {
		close(fdm);
		return (-3);
	}

	/* get slave's name */
	if ((ptr = ptsname(fdm)) == NULL) {
		close(fdm);
		return (-3);
	}
	strcpy(pts_name, ptr);
	return (fdm);
}

/*
 * ptys_open --
 *	This function opens the slave pty.
 */
static int
ptys_open(fdm, pts_name)
	int fdm;
	char *pts_name;
{
	int fds;

	if ((fds = open(pts_name, O_RDWR)) < 0) {
		close(fdm);
		return (-5);
	}

	if (ioctl(fds, I_PUSH, "ptem") < 0) {
		close(fds);
		close(fdm);
		return (-6);
	}

	if (ioctl(fds, I_PUSH, "ldterm") < 0) {
		close(fds);
		close(fdm);
		return (-7);
	}

	if (ioctl(fds, I_PUSH, "ttcompat") < 0) {
		close(fds);
		close(fdm);
		return (-8);
	}

	return (fds);
}

#else /* !HAVE_SYS5_PTY */

static int
sscr_pty(amaster, aslave, name, termp, winp)
	int *amaster, *aslave;
	char *name;
	struct termios *termp;
	void *winp;
{
	static char line[] = "/dev/ptyXX";
	register char *cp1, *cp2;
	register int master, slave, ttygid;
	struct group *gr;

	if ((gr = getgrnam("tty")) != NULL)
		ttygid = gr->gr_gid;
	else
		ttygid = -1;

	for (cp1 = "pqrs"; *cp1; cp1++) {
		line[8] = *cp1;
		for (cp2 = "0123456789abcdef"; *cp2; cp2++) {
			line[5] = 'p';
			line[9] = *cp2;
			if ((master = open(line, O_RDWR, 0)) == -1) {
				if (errno == ENOENT)
					return (-1);	/* out of ptys */
			} else {
				line[5] = 't';
				(void) chown(line, getuid(), ttygid);
				(void) chmod(line, S_IRUSR|S_IWUSR|S_IWGRP);
#ifdef HAVE_REVOKE
				(void) revoke(line);
#endif
				if ((slave = open(line, O_RDWR, 0)) != -1) {
					*amaster = master;
					*aslave = slave;
					if (name)
						strcpy(name, line);
					if (termp)
						(void) tcsetattr(slave, 
							TCSAFLUSH, termp);
#ifdef TIOCSWINSZ
					if (winp)
						(void) ioctl(slave, TIOCSWINSZ, 
							(char *)winp);
#endif
					return (0);
				}
				(void) close(master);
			}
		}
	}
	errno = ENOENT;	/* out of ptys */
	return (-1);
}
#endif /* HAVE_SYS5_PTY */
