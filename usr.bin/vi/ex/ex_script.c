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
static char sccsid[] = "@(#)ex_script.c	8.12 (Berkeley) 3/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <queue.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "script.h"

/*
 * XXX
 */
int openpty __P((int *, int *, char *, struct termios *, struct winsize *));

static int sscr_getprompt __P((SCR *, EXF *));
static int sscr_init __P((SCR *, EXF *));
static int sscr_matchprompt __P((SCR *, char *, size_t, size_t *));
static int sscr_setprompt __P((SCR *, char *, size_t));

/*
 * ex_script -- : sc[ript][!] [file]
 *
 *	Switch to script mode.
 */
int
ex_script(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	/* Vi only command. */
	if (!IN_VI_MODE(sp)) {
		msgq(sp, M_ERR,
		    "The script command is only available in vi mode.");
		return (1);
	}

	/* Switch to the new file. */
	if (cmdp->argc != 0 && ex_edit(sp, ep, cmdp))
		return (1);

	/*
	 * Create the shell, figure out the prompt.
	 *
	 * !!!
	 * The files just switched, use sp->ep.
	 */
	if (sscr_init(sp, sp->ep))
		return (1);

	return (0);
}

/*
 * sscr_init --
 *	Create a pty setup for a shell.
 */
static int
sscr_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	SCRIPT *sc;
	char *sh, *sh_path;

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

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &sc->sh_win) == -1) {
		msgq(sp, M_SYSERR, "tcgetattr");
		goto err;
	}

	if (openpty(&sc->sh_master,
	    &sc->sh_slave, sc->sh_name, &sc->sh_term, &sc->sh_win) == -1) {
		msgq(sp, M_SYSERR, "openpty");
		goto err;
	}

	/*
	 * Don't use vfork() here, because the signal semantics
	 * differ from implementation to implementation.
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
		 * The utility has default signal behavior.  Don't bother
		 * using sigaction(2) 'cause we want the default behavior.
		 */
		(void)signal(SIGINT, SIG_DFL);
		(void)signal(SIGQUIT, SIG_DFL);

		/*
		 * XXX
		 * So that shells that do command line editing turn it off.
		 */
		(void)putenv("TERM=emacs");
		(void)putenv("TERMCAP=emacs:");
		(void)putenv("EMACS=t");

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
		msgq(sp, M_ERR,
		    "Error: execl: %s: %s", sh_path, strerror(errno));
		_exit(127);
	default:
		break;
	}

	if (sscr_getprompt(sp, ep))
		return (1);

	F_SET(sp, S_REDRAW | S_SCRIPT);
	return (0);

}

/*
 * sscr_getprompt --
 *	Eat lines printed by the shell until a line with no trailing
 *	carriage return comes; set the prompt from that line.
 */
static int
sscr_getprompt(sp, ep)
	SCR *sp;
	EXF *ep;
{
	struct timeval tv;
	SCRIPT *sc;
	fd_set fdset;
	recno_t lline;
	size_t llen, len;
	u_int value;
	int nr;
	char *endp, *p, *t, buf[1024];

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
		msgq(sp, M_ERR, "Error: timed out.");
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
		value = term_key_val(sp, *p);
		if (value == K_CR || value == K_NL) {
			if (file_lline(sp, ep, &lline) ||
			    file_aline(sp, ep, 0, lline, t, p - t))
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
	if (file_lline(sp, ep, &lline) ||
	    file_aline(sp, ep, 0, lline, buf, llen)) {
prompterr:	sscr_end(sp);
		return (1);
	}

	return (sscr_setprompt(sp, buf, llen));
}

/*
 * sscr_exec --
 *	Take a line and hand it off to the shell.
 */
int
sscr_exec(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	SCRIPT *sc;
	recno_t last_lno;
	size_t blen, len, last_len, tlen;
	int matchprompt, nw, rval;
	char *bp, *p;

	/* If there's a prompt on the last line, append the command. */
	if (file_lline(sp, ep, &last_lno))
		return (1);
	if ((p = file_gline(sp, ep, last_lno, &last_len)) == NULL) {
		GETLINE_ERR(sp, last_lno);
		return (1);
	}
	if (sscr_matchprompt(sp, p, last_len, &tlen) && tlen == 0) {
		matchprompt = 1;
		GET_SPACE_RET(sp, bp, blen, last_len + 128);
		memmove(bp, p, last_len);
	} else
		matchprompt = 0;

	/* Get something to execute. */
	if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			goto err1;
		if (lno == 0)
			goto empty;
		else
			GETLINE_ERR(sp, lno);
		goto err1;
	}

	/* Empty lines aren't interesting. */
	if (len == 0)
		goto empty;

	/* Delete any prompt. */
	if (sscr_matchprompt(sp, p, len, &tlen)) {
		if (tlen == len) {
empty:			msgq(sp, M_BERR, "Nothing to execute.");
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
		if (file_sline(sp, ep, last_lno, bp, last_len + len))
err1:			rval = 1;
	}
	if (matchprompt)
		FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * sscr_input --
 *	Take a line from the shell and insert it into the file.
 */
int
sscr_input(sp)
	SCR *sp;
{
	struct timeval tv;
	SCRIPT *sc;
	EXF *ep;
	recno_t lno;
	size_t blen, len, tlen;
	u_int value;
	int nr, rval;
	char *bp, *endp, *p, *t;

	/* Find out where the end of the file is. */
	ep = sp->ep;
	if (file_lline(sp, ep, &lno))
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
		F_CLR(sp, S_SCRIPT);
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
		value = term_key_val(sp, *p);
		if (value == K_CR || value == K_NL) {
			len = p - t;
			if (file_aline(sp, ep, 1, lno++, t, len))
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
			FD_SET(sc->sh_master, &sp->rdfd);
			FD_CLR(STDIN_FILENO, &sp->rdfd);
			if (select(sc->sh_master + 1,
			    &sp->rdfd, NULL, NULL, &tv) == 1) {
				memmove(bp, t, len);
				endp = bp + len;
				goto more;
			}
		}
		if (sscr_setprompt(sp, t, len))
			return (1);
		if (file_aline(sp, ep, 1, lno++, t, len))
			goto ret;
	}

	/* The cursor moves to EOF. */
	sp->lno = lno;
	sp->cno = len ? len - 1 : 0;
	rval = sp->s_refresh(sp, ep);

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
	char* buf;
	size_t len;
{
	SCRIPT *sc;

	sc = sp->script;
	if (sc->sh_prompt)
		FREE(sc->sh_prompt, sc->sh_prompt_len);
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
 */
int
sscr_end(sp)
	SCR *sp;
{
	SCRIPT *sc;
	int rval;

	if ((sc = sp->script) == NULL)
		return (0);

	/* Turn off the script flag. */
	F_CLR(sp, S_SCRIPT);

	/* Close down the parent's file descriptors. */
	if (sc->sh_master != -1)
	    (void)close(sc->sh_master);
	if (sc->sh_slave != -1)
	    (void)close(sc->sh_slave);

	/* This should have killed the child. */
	rval = proc_wait(sp, (long)sc->sh_pid, "script-shell", 0);

	/* Free memory. */
	FREE(sc->sh_prompt, sc->sh_prompt_len);
	FREE(sc, sizeof(SCRIPT));
	sp->script = NULL;

	return (rval);
}
