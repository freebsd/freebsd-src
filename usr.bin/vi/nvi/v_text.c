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
static char sccsid[] = "@(#)v_text.c	8.23 (Berkeley) 1/9/94";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

/*
 * !!!
 * Repeated input in the historic vi is mostly wrong and this isn't very
 * backward compatible.  For example, if the user entered "3Aab\ncd" in
 * the historic vi, the "ab" was repeated 3 times, and the "\ncd" was then
 * appended to the result.  There was also a hack which I don't remember
 * right now, where "3o" would open 3 lines and then let the user fill them
 * in, to make screen movements on 300 baud modems more tolerable.  I don't
 * think it's going to be missed.
 */

#define	SET_TXT_STD(sp, f) {						\
	LF_INIT((f) | TXT_BEAUTIFY | TXT_CNTRLT | TXT_ESCAPE |		\
	    TXT_MAPINPUT | TXT_RECORD | TXT_RESOLVE);			\
	if (O_ISSET(sp, O_ALTWERASE))					\
		LF_SET(TXT_ALTWERASE);					\
	if (O_ISSET(sp, O_AUTOINDENT))					\
		LF_SET(TXT_AUTOINDENT);					\
	if (O_ISSET(sp, O_SHOWMATCH))					\
		LF_SET(TXT_SHOWMATCH);					\
	if (O_ISSET(sp, O_WRAPMARGIN))					\
		LF_SET(TXT_WRAPMARGIN);					\
	if (F_ISSET(sp, S_SCRIPT))					\
		LF_SET(TXT_CR);						\
	if (O_ISSET(sp, O_TTYWERASE))					\
		LF_SET(TXT_TTYWERASE);					\
}

/* 
 * !!!
 * There's a problem with the way that we do logging for change commands with
 * implied motions (e.g. A, I, O, cc, etc.).  Since the main vi loop logs the
 * starting cursor position before the change command "moves" the cursor, the
 * cursor position to which we return on undo will be where the user entered
 * the change command, not the start of the change.  Several of the following
 * routines re-log the cursor to make this work correctly.  Historic vi tried
 * to do the same thing, and mostly got it right.  (The only spectacular way
 * it fails is if the user entered 'o' from anywhere but the last character of
 * the line, the undo returned the cursor to the start of the line.  If the
 * user was on the last character of the line, the cursor returned to that
 * position.)
 */

static int v_CS __P((SCR *, EXF *, VICMDARG *, MARK *, MARK *, MARK *, u_int));

/*
 * v_iA -- [count]A
 *	Append text to the end of the line.
 */
int
v_iA(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	u_long cnt;
	size_t len;
	u_int flags;
	int first;
	char *p;

	SET_TXT_STD(sp, TXT_APPENDEOL);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (first = 1, lno = fm->lno,
	    cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		/* Move the cursor to the end of the line + 1. */
		if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0) {
				GETLINE_ERR(sp, lno);
				return (1);
			}
			lno = 1;
			len = 0;
		} else {
			/* Correct logging for implied cursor motion. */
			sp->cno = len == 0 ? 0 : len - 1;
			if (first == 1) {
				log_cursor(sp, ep);
				first = 0;
			}
			/* Start the change after the line. */
			sp->cno = len;
		}

		if (v_ntext(sp, ep,
		    &sp->tiq, NULL, p, len, rp, 0, OOBLNO, flags))
			return (1);

		SET_TXT_STD(sp, TXT_APPENDEOL | TXT_REPLAY);
		sp->lno = lno = rp->lno;
		sp->cno = rp->cno;
	}
	return (0);
}

/*
 * v_ia -- [count]a
 *	Append text to the cursor position.
 */
int
v_ia(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	u_long cnt;
	u_int flags;
	size_t len;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (lno = fm->lno,
	    cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		/*
		 * Move the cursor one column to the right and
		 * repaint the screen.
		 */
		if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0) {
				GETLINE_ERR(sp, lno);
				return (1);
			}
			lno = 1;
			len = 0;
			LF_SET(TXT_APPENDEOL);
		} else if (len) {
			if (len == sp->cno + 1) {
				sp->cno = len;
				LF_SET(TXT_APPENDEOL);
			} else
				++sp->cno;
		} else
			LF_SET(TXT_APPENDEOL);

		if (v_ntext(sp, ep,
		    &sp->tiq, NULL, p, len, rp, 0, OOBLNO, flags))
			return (1);

		SET_TXT_STD(sp, TXT_REPLAY);
		sp->lno = lno = rp->lno;
		sp->cno = rp->cno;
	}
	return (0);
}

/*
 * v_iI -- [count]I
 *	Insert text at the first non-blank character in the line.
 */
int
v_iI(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	u_long cnt;
	size_t len;
	u_int flags;
	int first;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (first = 1, lno = fm->lno,
	    cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		/*
		 * Move the cursor to the start of the line and repaint
		 * the screen.
		 */
		if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0) {
				GETLINE_ERR(sp, lno);
				return (1);
			}
			lno = 1;
			len = 0;
		} else {
			sp->cno = 0;
			if (nonblank(sp, ep, lno, &sp->cno))
				return (1);
			/* Correct logging for implied cursor motion. */
			if (first == 1) {
				log_cursor(sp, ep);
				first = 0;
			}
		}
		if (len == 0)
			LF_SET(TXT_APPENDEOL);

		if (v_ntext(sp, ep,
		    &sp->tiq, NULL, p, len, rp, 0, OOBLNO, flags))
			return (1);

		SET_TXT_STD(sp, TXT_REPLAY);
		sp->lno = lno = rp->lno;
		sp->cno = rp->cno;
	}
	return (0);
}

/*
 * v_ii -- [count]i
 *	Insert text at the cursor position.
 */
int
v_ii(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	u_long cnt;
	size_t len;
	u_int flags;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (lno = fm->lno,
	    cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0) {
				GETLINE_ERR(sp, fm->lno);
				return (1);
			}
			lno = 1;
			len = 0;
		}
		/* If len == sp->cno, it's a replay caused by a count. */
		if (len == 0 || len == sp->cno)
			LF_SET(TXT_APPENDEOL);

		if (v_ntext(sp, ep,
		    &sp->tiq, NULL, p, len, rp, 0, OOBLNO, flags))
			return (1);

		/*
		 * On replay, if the line isn't empty, advance the insert
		 * by one (make it an append).
		 */
		SET_TXT_STD(sp, TXT_REPLAY);
		sp->lno = lno = rp->lno;
		if ((sp->cno = rp->cno) != 0)
			++sp->cno;
	}
	return (0);
}

/*
 * v_iO -- [count]O
 *	Insert text above this line.
 */
int
v_iO(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t ai_line, lno;
	size_t len;
	u_long cnt;
	u_int flags;
	int first;
	char *p;

	SET_TXT_STD(sp, TXT_APPENDEOL);
	if (F_ISSET(vp, VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (first = 1, cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		if (sp->lno == 1) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0)
				goto insert;
			p = NULL;
			len = 0;
			ai_line = OOBLNO;
		} else {
insert:			p = "";
			sp->cno = 0;
			/* Correct logging for implied cursor motion. */
			if (first == 1) {
				log_cursor(sp, ep);
				first = 0;
			}
			if (file_iline(sp, ep, sp->lno, p, 0))
				return (1);
			if ((p = file_gline(sp, ep, sp->lno, &len)) == NULL) {
				GETLINE_ERR(sp, sp->lno);
				return (1);
			}
			ai_line = sp->lno + 1;
		}

		if (v_ntext(sp, ep,
		    &sp->tiq, NULL, p, len, rp, 0, ai_line, flags))
			return (1);

		SET_TXT_STD(sp, TXT_APPENDEOL | TXT_REPLAY);
		sp->lno = lno = rp->lno;
		sp->cno = rp->cno;
	}
	return (0);
}

/*
 * v_io -- [count]o
 *	Insert text after this line.
 */
int
v_io(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t ai_line, lno;
	size_t len;
	u_long cnt;
	u_int flags;
	int first;
	char *p;

	SET_TXT_STD(sp, TXT_APPENDEOL);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (first = 1,
	    cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		if (sp->lno == 1) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0)
				goto insert;
			p = NULL;
			len = 0;
			ai_line = OOBLNO;
		} else {
insert:			p = "";
			sp->cno = 0;
			/* Correct logging for implied cursor motion. */
			if (first == 1) {
				log_cursor(sp, ep);
				first = 0;
			}
			len = 0;
			if (file_aline(sp, ep, 1, sp->lno, p, len))
				return (1);
			if ((p = file_gline(sp, ep, ++sp->lno, &len)) == NULL) {
				GETLINE_ERR(sp, sp->lno);
				return (1);
			}
			ai_line = sp->lno - 1;
		}

		if (v_ntext(sp, ep,
		    &sp->tiq, NULL, p, len, rp, 0, ai_line, flags))
			return (1);

		SET_TXT_STD(sp, TXT_APPENDEOL | TXT_REPLAY);
		sp->lno = lno = rp->lno;
		sp->cno = rp->cno;
	}
	return (0);
}

/*
 * v_Change -- [buffer][count]C
 *	Change line command.
 */
int
v_Change(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (v_CS(sp, ep, vp, fm, tm, rp, 0));
}

/*
 * v_Subst -- [buffer][count]S
 *	Line substitute command.
 */
int
v_Subst(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_int flags;

	/*
	 * The S command is the same as a 'C' command from the beginning
	 * of the line.  This is hard to do in the parser, so do it here.
	 *
	 * If autoindent is on, the change is from the first *non-blank*
	 * character of the line, not the first character.  And, to make
	 * it just a bit more exciting, the initial space is handled as
	 * auto-indent characters.
	 */
	LF_INIT(0);
	if (O_ISSET(sp, O_AUTOINDENT)) {
		fm->cno = 0;
		if (nonblank(sp, ep, fm->lno, &fm->cno))
			return (1);
		LF_SET(TXT_AICHARS);
	} else
		fm->cno = 0;
	sp->cno = fm->cno;
	return (v_CS(sp, ep, vp, fm, tm, rp, flags));
}

/*
 * v_CS --
 *	C and S commands.
 */
static int
v_CS(sp, ep, vp, fm, tm, rp, iflags)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
	u_int iflags;
{
	recno_t lno;
	size_t len;
	char *p;
	u_int flags;

	SET_TXT_STD(sp, iflags);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);

	/*
	 * There are two cases -- if a count is supplied, we do a line
	 * mode change where we delete the lines and then insert text
	 * into a new line.  Otherwise, we replace the current line.
	 */
	tm->lno = fm->lno + (F_ISSET(vp, VC_C1SET) ? vp->count - 1 : 0);
	if (fm->lno != tm->lno) {
		/* Make sure that the to line is real. */
		if (file_gline(sp, ep, tm->lno, NULL) == NULL) {
			v_eof(sp, ep, fm);
			return (1);
		}

		/* Cut the lines. */
		if (cut(sp, ep,
		    NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
		    fm, tm, CUT_LINEMODE))
			return (1);

		/* Insert a line while we still can... */
		if (file_iline(sp, ep, fm->lno, "", 0))
			return (1);
		++fm->lno;
		++tm->lno;

		/* Delete the lines. */
		if (delete(sp, ep, fm, tm, 1))
			return (1);

		/* Get the inserted line. */
		if ((p = file_gline(sp, ep, --fm->lno, &len)) == NULL) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		tm = NULL;
		sp->lno = fm->lno;
		sp->cno = 0;
		LF_SET(TXT_APPENDEOL);
	} else { 
		/* The line may be empty, but that's okay. */
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0) {
				GETLINE_ERR(sp, tm->lno);
				return (1);
			}
			len = 0;
			LF_SET(TXT_APPENDEOL);
		} else {
			if (cut(sp, ep,
			    NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
			    fm, tm, CUT_LINEMODE))
				return (1);
			tm->cno = len;
			if (len == 0)
				LF_SET(TXT_APPENDEOL);
			LF_SET(TXT_EMARK | TXT_OVERWRITE);
		}
	}
	/* Correct logging for implied cursor motion. */
	log_cursor(sp, ep);
	return (v_ntext(sp, ep,
	    &sp->tiq, tm, p, len, rp, 0, OOBLNO, flags));
}

/*
 * v_change -- [buffer][count]c[count]motion
 *	Change command.
 */
int
v_change(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t blen, len;
	u_int flags;
	int lmode, rval;
	char *bp, *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);

	/*
	 * Move the cursor to the start of the change.  Note, if autoindent
	 * is turned on, the cc command in line mode changes from the first
	 * *non-blank* character of the line, not the first character.  And,
	 * to make it just a bit more exciting, the initial space is handled
	 * as auto-indent characters.
	 */
	lmode = F_ISSET(vp, VC_LMODE) ? CUT_LINEMODE : 0;
	if (lmode) {
		fm->cno = 0;
		if (O_ISSET(sp, O_AUTOINDENT)) {
			if (nonblank(sp, ep, fm->lno, &fm->cno))
				return (1);
			LF_SET(TXT_AICHARS);
		}
	}
	sp->lno = fm->lno;
	sp->cno = fm->cno;

	/* Correct logging for implied cursor motion. */
	log_cursor(sp, ep);

	/*
	 * If changing within a single line, the line either currently has
	 * text or it doesn't.  If it doesn't, just insert text.  Otherwise,
	 * copy it and overwrite it.
	 */
	if (fm->lno == tm->lno) {
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			if (p == NULL) {
				if (file_lline(sp, ep, &lno))
					return (1);
				if (lno != 0) {
					GETLINE_ERR(sp, fm->lno);
					return (1);
				}
			}
			len = 0;
			LF_SET(TXT_APPENDEOL);
		} else {
			if (cut(sp, ep,
			    NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
			    fm, tm, lmode))
				return (1);
			if (len == 0)
				LF_SET(TXT_APPENDEOL);
			LF_SET(TXT_EMARK | TXT_OVERWRITE);
		}
		return (v_ntext(sp, ep,
		    &sp->tiq, tm, p, len, rp, 0, OOBLNO, flags));
	}

	/*
	 * It's trickier if changing over multiple lines.  If we're in
	 * line mode we delete all of the lines and insert a replacement
	 * line which the user edits.  If there was leading whitespace
	 * in the first line being changed, we copy it and use it as the
	 * replacement.  If we're not in line mode, we just delete the
	 * text and start inserting.
	 *
	 * Copy the text.
	 */
	if (cut(sp, ep,
	    NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL, fm, tm, lmode))
		return (1);

	/* If replacing entire lines and there's leading text. */
	if (lmode && fm->cno) {
		/* Get a copy of the first line changed. */
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		/* Copy the leading text elsewhere. */
		GET_SPACE_RET(sp, bp, blen, fm->cno);
		memmove(bp, p, fm->cno);
	} else
		bp = NULL;

	/* Delete the text. */
	if (delete(sp, ep, fm, tm, lmode))
		return (1);

	/* If replacing entire lines, insert a replacement line. */
	if (lmode) {
		if (file_iline(sp, ep, fm->lno, bp, fm->cno))
			return (1);
		sp->lno = fm->lno;
		len = sp->cno = fm->cno;
	}

	/* Get the line we're editing. */
	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno != 0) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		len = 0;
	}

	/* Check to see if we're appending to the line. */
	if (fm->cno >= len)
		LF_SET(TXT_APPENDEOL);

	/* No to mark. */
	tm = NULL;

	rval = v_ntext(sp, ep, &sp->tiq, tm, p, len, rp, 0, OOBLNO, flags);

	if (bp != NULL)
		FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * v_Replace -- [count]R
 *	Overwrite multiple characters.
 */
int
v_Replace(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	u_long cnt;
	size_t len;
	u_int flags;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	if ((p = file_gline(sp, ep, rp->lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno != 0) {
			GETLINE_ERR(sp, rp->lno);
			return (1);
		}
		len = 0;
		LF_SET(TXT_APPENDEOL);
	} else {
		if (len == 0)
			LF_SET(TXT_APPENDEOL);
		LF_SET(TXT_OVERWRITE | TXT_REPLACE);
	}
	tm->lno = rp->lno;
	tm->cno = len ? len : 0;
	if (v_ntext(sp, ep, &sp->tiq, tm, p, len, rp, 0, OOBLNO, flags))
		return (1);

	/*
	 * Special case.  The historic vi handled [count]R badly, in that R
	 * would replace some number of characters, and then the count would
	 * append count-1 copies of the replacing chars to the replaced space.
	 * This seems wrong, so this version counts R commands.  There is some
	 * trickiness in moving back to where the user stopped replacing after
	 * each R command.  Basically, if the user ended with a newline, we
	 * want to use rp->cno (which will be 0).  Otherwise, use the column
	 * after the returned cursor, unless it would be past the end of the
	 * line, in which case we append to the line.
	 */
	while (--cnt) {
		if ((p = file_gline(sp, ep, rp->lno, &len)) == NULL)
			GETLINE_ERR(sp, rp->lno);
		SET_TXT_STD(sp, TXT_REPLAY);

		sp->lno = rp->lno;

		if (len == 0 || rp->cno == len - 1) {
			sp->cno = len;
			LF_SET(TXT_APPENDEOL);
		} else {
			sp->cno = rp->cno;
			if (rp->cno != 0)
				++sp->cno;
			LF_SET(TXT_OVERWRITE | TXT_REPLACE);
		}

		if (v_ntext(sp, ep,
		    &sp->tiq, tm, p, len, rp, 0, OOBLNO, flags))
			return (1);
	}
	return (0);
}

/*
 * v_subst -- [buffer][count]s
 *	Substitute characters.
 */
int
v_subst(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t len;
	u_int flags;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno != 0) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		len = 0;
		LF_SET(TXT_APPENDEOL);
	} else {
		if (len == 0)
			LF_SET(TXT_APPENDEOL);
		LF_SET(TXT_EMARK | TXT_OVERWRITE);
	}

	tm->lno = fm->lno;
	tm->cno = fm->cno + (F_ISSET(vp, VC_C1SET) ? vp->count : 1);
	if (tm->cno > len)
		tm->cno = len;

	if (p != NULL && cut(sp, ep,
	    NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL, fm, tm, 0))
		return (1);

	return (v_ntext(sp, ep,
	    &sp->tiq, tm, p, len, rp, 0, OOBLNO, flags));
}
