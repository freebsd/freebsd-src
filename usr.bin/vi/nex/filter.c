/*-
 * Copyright (c) 1991, 1993
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
static char sccsid[] = "@(#)filter.c	8.26 (Berkeley) 1/2/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"

static int	filter_ldisplay __P((SCR *, FILE *));

/*
 * filtercmd --
 *	Run a range of lines through a filter utility and optionally
 *	replace the original text with the stdout/stderr output of
 *	the utility.
 */
int
filtercmd(sp, ep, fm, tm, rp, cmd, ftype)
	SCR *sp;
	EXF *ep;
	MARK *fm, *tm, *rp;
	char *cmd;
	enum filtertype ftype;
{
	struct sigaction act, oact;
	struct stat osb, sb;
	struct termios term;
	FILE *ifp, *ofp;		/* GCC: can't be uninitialized. */
	pid_t parent_writer_pid, utility_pid;
	recno_t lno, nread;
	int input[2], isig, output[2], rval;
	char *name;

	/* Set return cursor position; guard against a line number of zero. */
	*rp = *fm;
	if (fm->lno == 0)
		rp->lno = 1;

	/*
	 * There are three different processes running through this code.
	 * They are the utility, the parent-writer and the parent-reader.
	 * The parent-writer is the process that writes from the file to
	 * the utility, the parent reader is the process that reads from
	 * the utility.
	 *
	 * Input and output are named from the utility's point of view.
	 * The utility reads from input[0] and the parent(s) write to
	 * input[1].  The parent(s) read from output[0] and the utility
	 * writes to output[1].
	 *
	 * In the FILTER_READ case, the utility isn't expected to want
	 * input.  Redirect its input from /dev/null.  Otherwise open
	 * up utility input pipe.
	 */
	ifp = ofp = NULL;
	input[0] = input[1] = output[0] = output[1] = -1;
	if (ftype == FILTER_READ) {
		if ((input[0] = open(_PATH_DEVNULL, O_RDONLY, 0)) < 0) {
			msgq(sp, M_ERR,
			    "filter: %s: %s", _PATH_DEVNULL, strerror(errno));
			return (1);
		}
	} else {
		if (pipe(input) < 0) {
			msgq(sp, M_SYSERR, "pipe");
			goto err;
		}
		if ((ifp = fdopen(input[1], "w")) == NULL) {
			msgq(sp, M_SYSERR, "fdopen");
			goto err;
		}
	}

	/* Open up utility output pipe. */
	if (pipe(output) < 0) {
		msgq(sp, M_SYSERR, "pipe");
		goto err;
	}
	if ((ofp = fdopen(output[0], "r")) == NULL) {
		msgq(sp, M_SYSERR, "fdopen");
		goto err;
	}

	/*
	 * Save ex/vi terminal settings, and restore the original ones.
	 * Restoration so that users can do things like ":r! cat /dev/tty".
	 */
	EX_LEAVE(sp, isig, act, oact, sb, osb, term);

	/* Fork off the utility process. */
	switch (utility_pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_SYSERR, "vfork");
err:		if (input[0] != -1)
			(void)close(input[0]);
		if (ifp != NULL)
			(void)fclose(ifp);
		else if (input[1] != -1)
			(void)close(input[1]);
		if (ofp != NULL)
			(void)fclose(ofp);
		else if (output[0] != -1)
			(void)close(output[0]);
		if (output[1] != -1)
			(void)close(output[1]);
		rval = 1;
		goto ret;
	case 0:				/* Utility. */
		/*
		 * The utility has default signal behavior.  Don't bother
		 * using sigaction(2) 'cause we want the default behavior.
		 */
		(void)signal(SIGINT, SIG_DFL);
		(void)signal(SIGQUIT, SIG_DFL);

		/*
		 * Redirect stdin from the read end of the input pipe,
		 * and redirect stdout/stderr to the write end of the
		 * output pipe.
		 */
		(void)dup2(input[0], STDIN_FILENO);
		(void)dup2(output[1], STDOUT_FILENO);
		(void)dup2(output[1], STDERR_FILENO);

		/* Close the utility's file descriptors. */
		(void)close(input[0]);
		(void)close(input[1]);
		(void)close(output[0]);
		(void)close(output[1]);

		if ((name = strrchr(O_STR(sp, O_SHELL), '/')) == NULL)
			name = O_STR(sp, O_SHELL);
		else
			++name;

		execl(O_STR(sp, O_SHELL), name, "-c", cmd, NULL);
		msgq(sp, M_ERR, "Error: execl: %s: %s",
		    O_STR(sp, O_SHELL), strerror(errno));
		_exit (127);
		/* NOTREACHED */
	default:			/* Parent-reader, parent-writer. */
		/* Close the pipe ends neither parent will use. */
		(void)close(input[0]);
		(void)close(output[1]);
		break;
	}

	/*
	 * FILTER_READ:
	 *
	 * Reading is the simple case -- we don't need a parent writer,
	 * so the parent reads the output from the read end of the output
	 * pipe until it finishes, then waits for the child.  Ex_readfp
	 * appends to the MARK, and closes ofp.
	 *
	 * !!!
	 * Set the return cursor to the last line read in.  Historically,
	 * this behaves differently from ":r file" command, which leaves
	 * the cursor at the first line read in.  Check to make sure that
	 * it's not past EOF because we were reading into an empty file.
	 */
	if (ftype == FILTER_READ) {
		rval = ex_readfp(sp, ep, "filter", ofp, fm, &nread, 0);
		sp->rptlines[L_ADDED] += nread;
		if (fm->lno == 0)
			rp->lno = nread;
		else
			rp->lno += nread;
		goto uwait;
	}

	/*
	 * FILTER, FILTER_WRITE
	 *
	 * Here we need both a reader and a writer.  Temporary files are
	 * expensive and we'd like to avoid disk I/O.  Using pipes has the
	 * obvious starvation conditions.  It's done as follows:
	 *
	 *	fork
	 *	child
	 *		write lines out
	 *		exit
	 *	parent
	 *		FILTER:
	 *			read lines into the file
	 *			delete old lines
	 *		FILTER_WRITE
	 *			read and display lines
	 *		wait for child
	 *
	 * XXX
	 * We get away without locking the underlying database because we know
	 * that none of the records that we're reading will be modified until
	 * after we've read them.  This depends on the fact that the current
	 * B+tree implementation doesn't balance pages or similar things when
	 * it inserts new records.  When the DB code has locking, we should
	 * treat vi as if it were multiple applications sharing a database, and
	 * do the required locking.  If necessary a work-around would be to do
	 * explicit locking in the line.c:file_gline() code, based on the flag
	 * set here.
	 */
	rval = 0;
	F_SET(ep, F_MULTILOCK);
	switch (parent_writer_pid = fork()) {
	case -1:			/* Error. */
		rval = 1;
		msgq(sp, M_SYSERR, "fork");
		(void)close(input[1]);
		(void)close(output[0]);
		break;
	case 0:				/* Parent-writer. */
		/*
		 * Write the selected lines to the write end of the
		 * input pipe.  Ifp is closed by ex_writefp.
		 */
		(void)close(output[0]);
		_exit(ex_writefp(sp, ep, "filter", ifp, fm, tm, NULL, NULL));

		/* NOTREACHED */
	default:			/* Parent-reader. */
		(void)close(input[1]);
		if (ftype == FILTER_WRITE)
			/*
			 * Read the output from the read end of the output
			 * pipe and display it.  Filter_ldisplay closes ofp.
			 */
			rval = filter_ldisplay(sp, ofp);
		else {
			/*
			 * Read the output from the read end of the output
			 * pipe.  Ex_readfp appends to the MARK and closes
			 * ofp.
			 */
			rval = ex_readfp(sp, ep, "filter", ofp, tm, &nread, 0);
			sp->rptlines[L_ADDED] += nread;
		}

		/* Wait for the parent-writer. */
		rval |= proc_wait(sp,
		    (long)parent_writer_pid, "parent-writer", 1);

		/* Delete any lines written to the utility. */
		if (ftype == FILTER && rval == 0) {
			for (lno = tm->lno; lno >= fm->lno; --lno)
				if (file_dline(sp, ep, lno)) {
					rval = 1;
					break;
				}
			if (rval == 0)
				sp->rptlines[L_DELETED] +=
				    (tm->lno - fm->lno) + 1;
		}
		/*
		 * If the filter had no output, we may have just deleted
		 * the cursor.  Don't do any real error correction, we'll
		 * try and recover later.
		 */
		 if (rp->lno > 1 && file_gline(sp, ep, rp->lno, NULL) == NULL)
			--rp->lno;
		break;
	}
	F_CLR(ep, F_MULTILOCK);

uwait:	rval |= proc_wait(sp, (long)utility_pid, cmd, 0);

	/* Restore ex/vi terminal settings. */
ret:	EX_RETURN(sp, isig, act, oact, sb, osb, term);

	return (rval);
}

/*
 * proc_wait --
 *	Wait for one of the processes.
 *
 * !!!
 * The pid_t type varies in size from a short to a long depending on the
 * system.  It has to be cast into something or the standard promotion
 * rules get you.  I'm using a long based on the belief that nobody is
 * going to make it unsigned and it's unlikely to be a quad.
 */
int
proc_wait(sp, pid, cmd, okpipe)
	SCR *sp;
	long pid;
	const char *cmd;
	int okpipe;
{
	extern const char *const sys_siglist[];
	size_t len;
	int pstat;

	/* Wait for the utility to finish. */
	(void)waitpid((pid_t)pid, &pstat, 0);

	/*
	 * Display the utility's exit status.  Ignore SIGPIPE from the
	 * parent-writer, as that only means that the utility chose to
	 * exit before reading all of its input.
	 */
	if (WIFSIGNALED(pstat) && (!okpipe || WTERMSIG(pstat) != SIGPIPE)) {
		for (; isblank(*cmd); ++cmd);
		len = strlen(cmd);
		msgq(sp, M_ERR, "%.*s%s: received signal: %s%s.",
		    MIN(len, 20), cmd, len > 20 ? "..." : "",
		    sys_siglist[WTERMSIG(pstat)],
		    WCOREDUMP(pstat) ? "; core dumped" : "");
		return (1);
	}
	if (WIFEXITED(pstat) && WEXITSTATUS(pstat)) {
		for (; isblank(*cmd); ++cmd);
		len = strlen(cmd);
		msgq(sp, M_ERR, "%.*s%s: exited with status %d",
		    MIN(len, 20), cmd, len > 20 ? "..." : "",
		    WEXITSTATUS(pstat));
		return (1);
	}
	return (0);
}

/*
 * filter_ldisplay --
 *	Display a line output from a utility.
 *
 * XXX
 * This should probably be combined with some of the ex_print()
 * routines into a single display routine.
 */
static int
filter_ldisplay(sp, fp)
	SCR *sp;
	FILE *fp;
{
	EX_PRIVATE *exp;
	size_t len;

	exp = EXP(sp);
	while (!ex_getline(sp, fp, &len)) {
		(void)ex_printf(EXCOOKIE, "%.*s\n", (int)len, exp->ibp);
		if (ferror(sp->stdfp)) {
			msgq(sp, M_SYSERR, NULL);
			(void)fclose(fp);
			return (1);
		}
	}
	if (fclose(fp)) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	return (0);
}
