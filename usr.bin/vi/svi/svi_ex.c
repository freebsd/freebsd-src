/*-
 * Copyright (c) 1993, 1994
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
static char sccsid[] = "@(#)svi_ex.c	8.39 (Berkeley) 3/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <curses.h>
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
#include "vcmd.h"
#include "excmd.h"
#include "svi_screen.h"
#include "../sex/sex_screen.h"

static int	svi_ex_done __P((SCR *, EXF *, MARK *));
static int	svi_ex_scroll __P((SCR *, int, int, CH *));

/*
 * svi_ex_cmd --
 *	Execute an ex command.
 */
int
svi_ex_cmd(sp, ep, exp, rp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *exp;
	MARK *rp;
{
	SVI_PRIVATE *svp;
	int rval;

	svp = SVP(sp);
	svp->exlcontinue = svp->exlinecount = svp->extotalcount = 0;

	(void)svi_busy(sp, NULL);
	rval = exp->cmd->fn(sp, ep, exp);

	/* No longer interruptible. */
	F_CLR(sp, S_INTERRUPTIBLE);

	(void)msg_rpt(sp, 0);
	(void)ex_fflush(EXCOOKIE);

	/*
	 * If displayed anything, figure out if we have to wait.  If the
	 * screen wasn't trashed, only one line and there are no waiting
	 * messages, don't wait, but don't overwrite it with mode information
	 * either.  If there's a screen under this one, change the line to
	 * inverse video.
	 */
	if (svp->extotalcount > 0)
		if (!F_ISSET(sp, S_REFRESH) && svp->extotalcount == 1 &&
		    (sp->msgq.lh_first == NULL ||
		    F_ISSET(sp->msgq.lh_first, M_EMPTY)))
			F_SET(sp, S_UPDATE_MODE);
		else
			(void)svi_ex_scroll(sp, 1, 0, NULL);
	return (svi_ex_done(sp, ep, rp) || rval);
}

/*
 * svi_ex_run --
 *	Execute strings of ex commands.
 */
int
svi_ex_run(sp, ep, rp)
	SCR *sp;
	EXF *ep;
	MARK *rp;
{
	enum input (*get) __P((SCR *, EXF *, TEXTH *, int, u_int));
	struct termios rawt, t;
	CH ikey;
	SVI_PRIVATE *svp;
	TEXT *tp;
	int flags, in_exmode, rval;

	svp = SVP(sp);
	svp->exlcontinue = svp->exlinecount = svp->extotalcount = 0;

	/*
	 * There's some tricky stuff going on here to handle when a user has
	 * mapped a key to multiple ex commands.  Historic practice was that
	 * vi ran without any special actions, as if the user were entering
	 * the characters, until ex trashed the screen, e.g. something like a
	 * '!' command.  At that point, we no longer know what the screen
	 * looks like, so we can't afford to overwrite anything.  The solution
	 * is to go into real ex mode until we get to the end of the command
	 * strings.
	 */
	get = svi_get;
	flags = TXT_BS | TXT_PROMPT;
	for (in_exmode = rval = 0;;) {
		if (get(sp, ep, &sp->tiq, ':', flags) != INP_OK) {
			rval = 1;
			break;
		}

		/*
		 * Len is 0 if the user backspaced over the prompt,
		 * 1 if only a CR was entered.
		 */
		tp = sp->tiq.cqh_first;
		if (tp->len == 0)
			break;

		if (!in_exmode)
			(void)svi_busy(sp, NULL);

		(void)ex_icmd(sp, ep, tp->lb, tp->len);
		(void)ex_fflush(EXCOOKIE);

		/*
		 * The file or screen may have changed, in which case,
		 * the main editor loop takes care of it.
		 */
		if (F_ISSET(sp, S_MAJOR_CHANGE))
			break;

		/*
		 * If continue not required, and one or no lines, and there
		 * are no waiting messages, don't wait, but don't overwrite
		 * it with mode information either.  If there's a screen under
		 * this one, change the line to inverse video.
		 */
		if (!F_ISSET(sp, S_CONTINUE) &&
		    (svp->extotalcount == 0 || svp->extotalcount == 1 &&
		    (sp->msgq.lh_first == NULL ||
		    F_ISSET(sp->msgq.lh_first, M_EMPTY)))) {
			if (svp->extotalcount == 1)
				F_SET(sp, S_UPDATE_MODE);
			break;
		}

		/* If the screen is trashed, go into ex mode. */
		if (!in_exmode && F_ISSET(sp, S_REFRESH)) {
			/* Initialize the terminal state. */
			if (F_ISSET(sp->gp, G_STDIN_TTY)) {
				SEX_RAW(t, rawt);
				get = sex_get;
			} else
				get = sex_get_notty;
			flags = TXT_CR | TXT_NLECHO | TXT_PROMPT;
			in_exmode = 1;
		}

		/*
		 * If the user hasn't already indicated that they're done,
		 * they may continue in ex mode by entering a ':'.
		 */
		if (F_ISSET(sp, S_INTERRUPTED))
			break;

		/* No longer interruptible. */
		F_CLR(sp, S_INTERRUPTIBLE);

		if (in_exmode) {
			(void)write(STDOUT_FILENO,
			    CONTMSG, sizeof(CONTMSG) - 1);
			for (;;) {
				if (term_user_key(sp, &ikey) != INP_OK) {
					rval = 1;
					goto ret;
				}
				if (ikey.ch == ' ' || ikey.ch == ':')
					break;
				if (ikey.value == K_CR || ikey.value == K_NL)
					break;
				sex_bell(sp);
			}
		} else
			(void)svi_ex_scroll(sp, 1, 1, &ikey);
		if (ikey.ch != ':')
                        break;

		if (in_exmode)
			(void)write(STDOUT_FILENO, "\r\n", 2);
		else {
			++svp->extotalcount;
			++svp->exlinecount;
		}
	}

ret:	if (in_exmode) {
		/* Reset the terminal state. */
		if (F_ISSET(sp->gp, G_STDIN_TTY) && SEX_NORAW(t))
			rval = 1;
		F_SET(sp, S_REFRESH);
	} else
		if (svi_ex_done(sp, ep, rp))
			rval = 1;
	F_CLR(sp, S_CONTINUE);
	return (rval);
}

/*
 * svi_ex_done --
 *	Cleanup from dipping into ex.
 */
static int
svi_ex_done(sp, ep, rp)
	SCR *sp;
	EXF *ep;
	MARK *rp;
{
	SMAP *smp;
	SVI_PRIVATE *svp;
	recno_t lno;
	size_t cnt, len;

	/*
	 * The file or screen may have changed, in which case,
	 * the main editor loop takes care of it.
	 */
	if (F_ISSET(sp, S_MAJOR_CHANGE))
		return (0);

	/*
	 * Otherwise, the only cursor modifications will be real, however, the
	 * underlying line may have changed; don't trust anything.  This code
	 * has been a remarkably fertile place for bugs.
	 *
	 * Repaint the entire screen if at least half the screen is trashed.
	 * Else, repaint only over the overwritten lines.  The "-2" comes
	 * from one for the mode line and one for the fact that it's an offset.
	 * Note the check for small screens.
	 *
	 * Don't trust ANYTHING.
	 */
	svp = SVP(sp);
	if (svp->extotalcount >= HALFTEXT(sp))
		F_SET(sp, S_REDRAW);
	else
		for (cnt = sp->rows - 2; svp->extotalcount--; --cnt)
			if (cnt > sp->t_rows) {
				MOVE(sp, cnt, 0);
				clrtoeol();
			} else {
				smp = HMAP + cnt;
				SMAP_FLUSH(smp);
				if (svi_line(sp, ep, smp, NULL, NULL))
					return (1);
			}
	/*
	 * Do a reality check on a cursor value, and make sure it's okay.
	 * If necessary, change it.  Ex keeps track of the line number,
	 * but ex doesn't care about the column and it may have disappeared.
	 */
	if (file_gline(sp, ep, sp->lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno != 0)
			GETLINE_ERR(sp, sp->lno);
		sp->lno = 1;
		sp->cno = 0;
	} else if (sp->cno >= len)
		sp->cno = len ? len - 1 : 0;

	rp->lno = sp->lno;
	rp->cno = sp->cno;
	return (0);
}

/*
 * svi_ex_write --
 *	Write out the ex messages.
 *
 * There is no tab or character translation going on, so, whatever the ex
 * and/or curses routines do with special characters is all that gets done.
 * This is probably okay, I don't see any reason that user's tab settings
 * should affect ex output, and ex should have displayed everything else
 * exactly as it wanted it on the screen.
 */
int
svi_ex_write(cookie, line, llen)
	void *cookie;
	const char *line;
	int llen;
{
	SCR *sp;
	SVI_PRIVATE *svp;
	size_t oldy, oldx;
	int len, rlen;
	const char *p;

	/*
	 * XXX
	 * If it's a 4.4BSD system, we could just use fpurge(3).
	 * This shouldn't be too expensive, though.
	 */
	sp = cookie;
	svp = SVP(sp);
	if (F_ISSET(sp, S_INTERRUPTED))
		return (llen);

	p = line;			/* In case of a write of 0. */
	for (rlen = llen; llen;) {
		/* Get the next line. */
		if ((p = memchr(line, '\n', llen)) == NULL)
			len = llen;
		else
			len = p - line;

		/*
		 * The max is sp->cols characters, and we may
		 * have already written part of the line.
		 */
		if (len + svp->exlcontinue > sp->cols)
			len = sp->cols - svp->exlcontinue;

		/*
		 * If the first line output, do nothing.
		 * If the second line output, draw the divider line.
		 * If drew a full screen, remove the divider line.
		 * If it's a continuation line, move to the continuation
		 * point, else, move the screen up.
		 */
		if (svp->exlcontinue == 0) {
			if (svp->extotalcount == 1) {
				MOVE(sp, INFOLINE(sp) - 1, 0);
				clrtoeol();
				if (svi_divider(sp))
					return (-1);
				F_SET(svp, SVI_DIVIDER);
				++svp->extotalcount;
				++svp->exlinecount;
			}
			if (svp->extotalcount == sp->t_maxrows &&
			    F_ISSET(svp, SVI_DIVIDER)) {
				--svp->extotalcount;
				--svp->exlinecount;
				F_CLR(svp, SVI_DIVIDER);
			}
			if (svp->extotalcount != 0 &&
			    svi_ex_scroll(sp, 0, 0, NULL))
				return (-1);
			MOVE(sp, INFOLINE(sp), 0);
			++svp->extotalcount;
			++svp->exlinecount;
			if (F_ISSET(sp, S_INTERRUPTIBLE) &&
			    F_ISSET(sp, S_INTERRUPTED))
				break;
		} else
			MOVE(sp, INFOLINE(sp), svp->exlcontinue);

		/* Display the line. */
		if (len)
			ADDNSTR(line, len);

		/* Clear to EOL. */
		getyx(stdscr, oldy, oldx);
		if (oldx < sp->cols)
			clrtoeol();

		/* If we loop, it's a new line. */
		svp->exlcontinue = 0;

		/* Reset for the next line. */
		line += len;
		llen -= len;
		if (p != NULL) {
			++line;
			--llen;
		}
	}
	/* Refresh the screen, even if it's a partial. */
	refresh();

	/* Set up next continuation line. */
	if (p == NULL)
		getyx(stdscr, oldy, svp->exlcontinue);
	return (rlen);
}

/*
 * svi_ex_scroll --
 *	Scroll the screen for ex output.
 */
static int
svi_ex_scroll(sp, mustwait, colon_ok, chp)
	SCR *sp;
	int mustwait, colon_ok;
	CH *chp;
{
	CH ikey;
	SVI_PRIVATE *svp;

	/*
	 * Scroll the screen.  Instead of scrolling the entire screen, delete
	 * the line above the first line output so preserve the maximum amount
	 * of the screen.
	 */
	svp = SVP(sp);
	if (svp->extotalcount >= sp->rows) {
		MOVE(sp, 0, 0);
	} else
		MOVE(sp, INFOLINE(sp) - svp->extotalcount, 0);

	deleteln();

	/* If there are screens below us, push them back into place. */
	if (sp->q.cqe_next != (void *)&sp->gp->dq) {
		MOVE(sp, INFOLINE(sp), 0);
		insertln();
	}

	/* If just displayed a full screen, wait. */
	if (mustwait || svp->exlinecount == sp->t_maxrows) {
		MOVE(sp, INFOLINE(sp), 0);
		if (F_ISSET(sp, S_INTERRUPTIBLE)) {
			ADDNSTR(CONTMSG_I, (int)sizeof(CONTMSG_I) - 1);
		} else {
			ADDNSTR(CONTMSG, (int)sizeof(CONTMSG) - 1);
		}
		clrtoeol();
		refresh();
		for (;;) {
			if (term_user_key(sp, &ikey) != INP_OK)
				return (-1);
			if (ikey.ch == ' ')
				break;
			if (colon_ok && ikey.ch == ':')
				break;
			if (ikey.value == K_CR || ikey.value == K_NL)
				break;
			if (ikey.ch == QUIT_CH &&
			    F_ISSET(sp, S_INTERRUPTIBLE)) {
				F_SET(sp, S_INTERRUPTED);
				break;
			}
			svi_bell(sp);
		}
		if (chp != NULL)
			*chp = ikey;
		svp->exlinecount = 0;
	}
	return (0);
}
