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
static char sccsid[] = "@(#)cut.c	8.20 (Berkeley) 1/23/94";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"

static int	cb_line __P((SCR *, EXF *, recno_t, size_t, size_t, TEXT **));
static int	cb_rotate __P((SCR *));

/* 
 * cut --
 *	Put a range of lines/columns into a buffer.
 *
 * There are two buffer areas, both found in the global structure.  The first
 * is the linked list of all the buffers the user has named, the second is the
 * default buffer storage.  There is a pointer, too, which is the current
 * default buffer, i.e. it may point to the default buffer or a named buffer
 * depending on into what buffer the last text was cut.  In both delete and
 * yank operations, text is cut into either the buffer named by the user, or
 * the default buffer.  If it's a delete of information on more than a single
 * line, the contents of the numbered buffers are rotated up one, the contents
 * of the buffer named '9' are discarded, and the text is also cut into the
 * buffer named '1'.
 *
 * In all cases, upper-case buffer names are the same as lower-case names,
 * with the exception that they cause the buffer to be appended to instead
 * of replaced.
 *
 * !!!
 * The contents of the default buffer would disappear after most operations in
 * historic vi.  It's unclear that this is useful, so we don't bother.
 *
 * When users explicitly cut text into the numeric buffers, historic vi became
 * genuinely strange.  I've never been able to figure out what was supposed to
 * happen.  It behaved differently if you deleted text than if you yanked text,
 * and, in the latter case, the text was appended to the buffer instead of
 * replacing the contents.  Hopefully it's not worth getting right.
 */
int
cut(sp, ep, cbp, namep, fm, tm, flags)
	SCR *sp;
	EXF *ep;
	CB *cbp;
	CHAR_T *namep;
	int flags;
	MARK *fm, *tm;
{
	CHAR_T name;
	TEXT *tp;
	recno_t lno;
	size_t len;
	int append, namedbuffer, setdefcb;

#if defined(DEBUG) && 0
	TRACE(sp, "cut: from {%lu, %d}, to {%lu, %d}%s\n",
	    fm->lno, fm->cno, tm->lno, tm->cno,
	    LF_ISSET(CUT_LINEMODE) ? " LINE MODE" : "");
#endif
	if (cbp == NULL) {
		if (LF_ISSET(CUT_DELETE) &&
		    (LF_ISSET(CUT_LINEMODE) || fm->lno != tm->lno)) {
			(void)cb_rotate(sp);
			name = '1';
			goto defcb;
		}
		if (namep == NULL) {
			cbp = sp->gp->dcb_store;
			append = namedbuffer = 0;
			setdefcb = 1;
		} else {
			name = *namep;
defcb:			CBNAME(sp, cbp, name);
			append = isupper(name);
			namedbuffer = setdefcb = 1;
		}
	} else
		append = namedbuffer = setdefcb = 0;

	/*
	 * If this is a new buffer, create it and add it into the list.
	 * Otherwise, if it's not an append, free its current contents.
	 */
	if (cbp == NULL) {
		CALLOC(sp, cbp, CB *, 1, sizeof(CB));
		cbp->name = name;
		CIRCLEQ_INIT(&cbp->textq);
		if (namedbuffer) {
			LIST_INSERT_HEAD(&sp->gp->cutq, cbp, q);
		} else
			sp->gp->dcb_store = cbp;
	} else if (!append) {
		text_lfree(&cbp->textq);
		cbp->len = 0;
		cbp->flags = 0;
	}

	/* In line mode, it's pretty easy, just cut the lines. */
	if (LF_ISSET(CUT_LINEMODE)) {
		for (lno = fm->lno; lno <= tm->lno; ++lno) {
			if (cb_line(sp, ep, lno, 0, 0, &tp))
				goto mem;
			CIRCLEQ_INSERT_TAIL(&cbp->textq, tp, q);
			cbp->len += tp->len;
		}
		cbp->flags |= CB_LMODE;
	} else {
		/* Get the first line. */
		len = fm->lno < tm->lno ? 0 : tm->cno - fm->cno;
		if (cb_line(sp, ep, fm->lno, fm->cno, len, &tp))
			goto mem;
		CIRCLEQ_INSERT_TAIL(&cbp->textq, tp, q);
		cbp->len += tp->len;

		/* Get the intermediate lines. */
		for (lno = fm->lno; ++lno < tm->lno;) {
			if (cb_line(sp, ep, lno, 0, 0, &tp))
				goto mem;
			CIRCLEQ_INSERT_TAIL(&cbp->textq, tp, q);
			cbp->len += tp->len;
		}

		/* Get the last line. */
		if (tm->lno > fm->lno && tm->cno > 0) {
			if (cb_line(sp, ep, lno, 0, tm->cno, &tp)) {
mem:				if (append)
					msgq(sp, M_ERR,
					    "Contents of %s buffer lost.",
					    charname(sp, name));
				text_lfree(&cbp->textq);
				cbp->len = 0;
				cbp->flags = 0;
				return (1);
			}
			CIRCLEQ_INSERT_TAIL(&cbp->textq, tp, q);
			cbp->len += tp->len;
		}
	}
	if (setdefcb)
		sp->gp->dcbp = cbp;	/* Repoint default buffer. */
	return (0);
}

/*
 * cb_rotate --
 *	Rotate the numbered buffers up one.
 */
static int
cb_rotate(sp)
	SCR *sp;
{
	CB *cbp, *del_cbp;

	del_cbp = NULL;
	for (cbp = sp->gp->cutq.lh_first; cbp != NULL; cbp = cbp->q.le_next)
		switch(cbp->name) {
		case '1':
			cbp->name = '2';
			break;
		case '2':
			cbp->name = '3';
			break;
		case '3':
			cbp->name = '4';
			break;
		case '4':
			cbp->name = '5';
			break;
		case '5':
			cbp->name = '6';
			break;
		case '6':
			cbp->name = '7';
			break;
		case '7':
			cbp->name = '8';
			break;
		case '8':
			cbp->name = '9';
			break;
		case '9':
			del_cbp = cbp;
			break;
		}
	if (del_cbp != NULL) {
		LIST_REMOVE(del_cbp, q);
		text_lfree(&del_cbp->textq);
		FREE(del_cbp, sizeof(CB));
	}
	return (0);
}

/*
 * cb_line --
 *	Cut a portion of a single line.
 */
static int
cb_line(sp, ep, lno, fcno, clen, newp)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t fcno, clen;
	TEXT **newp;
{
	TEXT *tp;
	size_t len;
	char *p;

	if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
		GETLINE_ERR(sp, lno);
		return (1);
	}

	if ((*newp = tp = text_init(sp, NULL, 0, len)) == NULL)
		return (1);

	/*
	 * A length of zero means to cut from the MARK to the end
	 * of the line.
	 */
	if (len != 0) {
		if (clen == 0)
			clen = len - fcno;
		memmove(tp->lb, p + fcno, clen);
		tp->len = clen;
	}
	return (0);
}

/*
 * text_init --
 *	Allocate a new TEXT structure.
 */
TEXT *
text_init(sp, p, len, total_len)
	SCR *sp;
	const char *p;
	size_t len, total_len;
{
	TEXT *tp;

	MALLOC(sp, tp, TEXT *, sizeof(TEXT));
	if (tp == NULL)
		return (NULL);
	/* ANSI C doesn't define a call to malloc(2) for 0 bytes. */
	if (tp->lb_len = total_len) {
		MALLOC(sp, tp->lb, CHAR_T *, tp->lb_len);
		if (tp->lb == NULL) {
			free(tp);
			return (NULL);
		}
		if (p != NULL && len != 0)
			memmove(tp->lb, p, len);
	} else
		tp->lb = NULL;
	tp->len = len;
	tp->ai = tp->insert = tp->offset = tp->owrite = 0;
	tp->wd = NULL;
	tp->wd_len = 0;
	return (tp);
}

/*
 * text_lfree --
 *	Free a chain of text structures.
 */
void
text_lfree(headp)
	TEXTH *headp;
{
	TEXT *tp;

	while ((tp = headp->cqh_first) != (void *)headp) {
		CIRCLEQ_REMOVE(headp, tp, q);
		text_free(tp);
	}
}

/*
 * text_free --
 *	Free a text structure.
 */
void
text_free(tp)
	TEXT *tp;
{
	if (tp->lb != NULL)
		FREE(tp->lb, tp->lb_len);
	if (tp->wd != NULL)
		FREE(tp->wd, tp->wd_len);
	FREE(tp, sizeof(TEXT));
}

/*
 * put --
 *	Put text buffer contents into the file.
 *
 * !!!
 * Historically, pasting into a file with no lines in vi would preserve
 * the single blank line.  This is almost certainly a result of the fact
 * that historic vi couldn't deal with a file that had no lines in it.
 * This implementation treats that as a bug, and does not retain the
 * blank line.
 */	
int
put(sp, ep, cbp, namep, cp, rp, append)
	SCR *sp;
	EXF *ep;
	CB *cbp;
	CHAR_T *namep;
	MARK *cp, *rp;
	int append;
{
	CHAR_T name;
	TEXT *ltp, *tp;
	recno_t lno;
	size_t blen, clen, len;
	char *bp, *p, *t;

	if (cbp == NULL)
		if (namep == NULL) {
			cbp = sp->gp->dcbp;
			if (cbp == NULL) {
				msgq(sp, M_ERR, "The default buffer is empty.");
				return (1);
			}
		} else {
			name = *namep;
			CBNAME(sp, cbp, name);
			if (cbp == NULL) {
				msgq(sp, M_ERR,
				    "Buffer %s is empty.", charname(sp, name));
				return (1);
			}
		}
	tp = cbp->textq.cqh_first;

	/*
	 * It's possible to do a put into an empty file, meaning that the
	 * cut buffer simply becomes the file.  It's a special case so
	 * that we can ignore it in general.
	 *
	 * Historical practice is that the cursor ends up on the first
	 * non-blank character of the first line inserted.
	 */
	if (cp->lno == 1) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0) {
			for (; tp != (void *)&cbp->textq;
			     ++lno, tp = tp->q.cqe_next)
				if (file_aline(sp, ep, 1, lno, tp->lb, tp->len))
					return (1);
			rp->lno = 1;
			rp->cno = 0;
			(void)nonblank(sp, ep, rp->lno, &rp->cno);
			goto ret;
		}
	}
			
	/* If a line mode buffer, append each new line into the file. */
	if (F_ISSET(cbp, CB_LMODE)) {
		lno = append ? cp->lno : cp->lno - 1;
		rp->lno = lno + 1;
		for (; tp != (void *)&cbp->textq; ++lno, tp = tp->q.cqe_next)
			if (file_aline(sp, ep, 1, lno, tp->lb, tp->len))
				return (1);
		rp->cno = 0;
		(void)nonblank(sp, ep, rp->lno, &rp->cno);
		goto ret;
	}

	/*
	 * If buffer was cut in character mode, replace the current line with
	 * one built from the portion of the first line to the left of the
	 * split plus the first line in the CB.  Append each intermediate line
	 * in the CB.  Append a line built from the portion of the first line
	 * to the right of the split plus the last line in the CB.
	 *
	 * Get the first line.
	 */
	lno = cp->lno;
	if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
		GETLINE_ERR(sp, lno);
		return (1);
	}

	GET_SPACE_RET(sp, bp, blen, tp->len + len + 1);
	t = bp;

	/* Original line, left of the split. */
	if (len > 0 && (clen = cp->cno + (append ? 1 : 0)) > 0) {
		memmove(bp, p, clen);
		p += clen;
		t += clen;
	}

	/* First line from the CB. */
	memmove(t, tp->lb, tp->len);
	t += tp->len;

	/* Calculate length left in original line. */
	clen = len ? len - cp->cno - (append ? 1 : 0) : 0;

	/*
	 * If no more lines in the CB, append the rest of the original
	 * line and quit.  Otherwise, build the last line before doing
	 * the intermediate lines, because the line changes will lose
	 * the cached line.
	 */
	if (tp->q.cqe_next == (void *)&cbp->textq) {
		/*
		 * Historical practice is that if a non-line mode put
		 * is inside a single line, the cursor ends up on the
		 * last character inserted.
		 */
		rp->lno = lno;
		rp->cno = (t - bp) - 1;

		if (clen > 0) {
			memmove(t, p, clen);
			t += clen;
		}
		if (file_sline(sp, ep, lno, bp, t - bp))
			goto mem;
	} else {
		/*
		 * Have to build both the first and last lines of the
		 * put before doing any sets or we'll lose the cached
		 * line.  Build both the first and last lines in the
		 * same buffer, so we don't have to have another buffer
		 * floating around.
		 *
		 * Last part of original line; check for space, reset
		 * the pointer into the buffer.
		 */
		ltp = cbp->textq.cqh_last;
		len = t - bp;
		ADD_SPACE_RET(sp, bp, blen, ltp->len + clen);
		t = bp + len;

		/* Add in last part of the CB. */
		memmove(t, ltp->lb, ltp->len);
		if (clen)
			memmove(t + ltp->len, p, clen);
		clen += ltp->len;

		/*
		 * Now: bp points to the first character of the first
		 * line, t points to the last character of the last
		 * line, t - bp is the length of the first line, and
		 * clen is the length of the last.  Just figured you'd
		 * want to know.
		 *
		 * Output the line replacing the original line.
		 */
		if (file_sline(sp, ep, lno, bp, t - bp))
			goto mem;

		/*
		 * Historical practice is that if a non-line mode put
		 * covers multiple lines, the cursor ends up on the
		 * first character inserted.  (Of course.)
		 */
		rp->lno = lno;
		rp->cno = (t - bp) - 1;

		/* Output any intermediate lines in the CB. */
		for (tp = tp->q.cqe_next;
		    tp->q.cqe_next != (void *)&cbp->textq;
		    ++lno, tp = tp->q.cqe_next)
			if (file_aline(sp, ep, 1, lno, tp->lb, tp->len))
				goto mem;

		if (file_aline(sp, ep, 1, lno, t, clen)) {
mem:			FREE_SPACE(sp, bp, blen);
			return (1);
		}
	}
	FREE_SPACE(sp, bp, blen);

	/* Reporting... */
ret:	sp->rptlines[L_PUT] += lno - cp->lno;

	return (0);
}
