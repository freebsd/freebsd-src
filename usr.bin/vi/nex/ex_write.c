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
static char sccsid[] = "@(#)ex_write.c	8.19 (Berkeley) 12/18/93";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

enum which {WQ, WRITE, XIT};

static int exwr __P((SCR *, EXF *, EXCMDARG *, enum which));

/*
 * ex_wq --	:wq[!] [>>] [file]
 *	Write to a file.
 */
int
ex_wq(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	int force;

	if (exwr(sp, ep, cmdp, WQ))
		return (1);

	force = F_ISSET(cmdp, E_FORCE);
	if (!force && ep->refcnt <= 1 && file_unedited(sp) != NULL) {
		msgq(sp, M_ERR,
		    "More files to edit; use \":n\" to go to the next file");
		return (1);
	}

	F_SET(sp, force ? S_EXIT_FORCE : S_EXIT);
	return (0);
}

/*
 * ex_write --	:write[!] [>>] [file]
 *		:write [!] [cmd]
 *	Write to a file.
 */
int
ex_write(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (exwr(sp, ep, cmdp, WRITE));
}


/*
 * ex_xit -- :x[it]! [file]
 *
 *	Write out any modifications and quit.
 */
int
ex_xit(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	int force;

	if (F_ISSET((ep), F_MODIFIED) && exwr(sp, ep, cmdp, XIT))
		return (1);

	force = F_ISSET(cmdp, E_FORCE);
	if (!force && ep->refcnt <= 1 && file_unedited(sp) != NULL) {
		msgq(sp, M_ERR,
		    "More files to edit; use \":n\" to go to the next file");
		return (1);
	}

	F_SET(sp, force ? S_EXIT_FORCE : S_EXIT);
	return (0);
}

/*
 * exwr --
 *	The guts of the ex write commands.
 */
static int
exwr(sp, ep, cmdp, cmd)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	EX_PRIVATE *exp;
	MARK rm;
	int flags;
	char *name, *p;

	/* All write commands can have an associated '!'. */
	LF_INIT(FS_POSSIBLE);
	if (F_ISSET(cmdp, E_FORCE))
		LF_SET(FS_FORCE);

	/* Skip any leading whitespace. */
	if (cmdp->argc != 0)
		for (p = cmdp->argv[0]->bp; *p && isblank(*p); ++p);

	/* If no arguments, just write the file back. */
	if (cmdp->argc == 0 || *p == '\0') {
		if (F_ISSET(cmdp, E_ADDR2_ALL))
			LF_SET(FS_ALL);
		return (file_write(sp, ep,
		    &cmdp->addr1, &cmdp->addr2, NULL, flags));
	}

	/* If "write !" it's a pipe to a utility. */
	exp = EXP(sp);
	if (cmd == WRITE && *p == '!') {
		for (++p; *p && isblank(*p); ++p);
		if (*p == '\0') {
			msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
			return (1);
		}
		/* Expand the argument. */
		if (argv_exp1(sp, ep, cmdp, p, strlen(p), 0))
			return (1);
		if (filtercmd(sp, ep, &cmdp->addr1, &cmdp->addr2,
		    &rm, cmdp->argv[1]->bp, FILTER_WRITE))
			return (1);
		sp->lno = rm.lno;
		return (0);
	}

	/* If "write >>" it's an append to a file. */
	if (cmd != XIT && p[0] == '>' && p[1] == '>') {
		LF_SET(FS_APPEND);

		/* Skip ">>" and whitespace. */
		for (p += 2; *p && isblank(*p); ++p);
	}

	/* Build an argv so we get an argument count and file expansion. */
	if (argv_exp2(sp, ep, cmdp, p, strlen(p), 0))
		return (1);

	switch (cmdp->argc) {
	case 1:
		/*
		 * Nothing to expand, write the current file. 
		 * XXX
		 * Should never happen, already checked this case.
		 */
		name = NULL;
		break;
	case 2:
		/* One new argument, write it. */
		name = cmdp->argv[exp->argsoff - 1]->bp;
		set_alt_name(sp, name);
		break;
	default:
		/* If expanded to more than one argument, object. */
		msgq(sp, M_ERR, "%s expanded into too many file names",
		    cmdp->argv[0]->bp);
		msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	if (F_ISSET(cmdp, E_ADDR2_ALL))
		LF_SET(FS_ALL);
	return (file_write(sp, ep, &cmdp->addr1, &cmdp->addr2, name, flags));
}

/*
 * ex_writefp --
 *	Write a range of lines to a FILE *.
 */
int
ex_writefp(sp, ep, name, fp, fm, tm, nlno, nch)
	SCR *sp;
	EXF *ep;
	char *name;
	FILE *fp;
	MARK *fm, *tm;
	u_long *nlno, *nch;
{
	register u_long ccnt, fline, tline;
	size_t len;
	char *p;

	fline = fm->lno;
	tline = tm->lno;

	if (nlno != NULL) {
		*nch = 0;
		*nlno = 0;
	}
	ccnt = 0;

	/*
	 * The vi filter code has multiple processes running simultaneously,
	 * and one of them calls ex_writefp().  The "unsafe" function calls
	 * in this code are to file_gline() and msgq().  File_gline() is safe,
	 * see the comment in filter.c:filtercmd() for details.  We don't call
	 * msgq if the multiple process bit in the EXF is set.
	 *
	 * !!!
	 * Historic vi permitted files of 0 length to be written.  However,
	 * since the way vi got around dealing with "empty" files was to
	 * always have a line in the file no matter what, it wrote them as
	 * files of a single, empty line.  We write empty files.
	 *
	 * "Alex, I'll take vi trivia for $1000."
	 */
	if (tline != 0)
		for (; fline <= tline; ++fline) {
			if ((p = file_gline(sp, ep, fline, &len)) == NULL)
				break;
			if (fwrite(p, 1, len, fp) != len) {
				msgq(sp, M_SYSERR, name);
				(void)fclose(fp);
				return (1);
			}
			ccnt += len;
			if (putc('\n', fp) != '\n')
				break;
			++ccnt;
		}
	if (fclose(fp)) {
		if (!F_ISSET(ep, F_MULTILOCK))
			msgq(sp, M_SYSERR, name);
		return (1);
	}
	if (nlno != NULL) {
		*nch = ccnt;
		*nlno = tm->lno == 0 ? 0 : tm->lno - fm->lno + 1;
	}
	return (0);
}
