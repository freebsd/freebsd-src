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
static char sccsid[] = "@(#)v_ntext.c	8.95 (Berkeley) 3/24/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
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
#include "seq.h"
#include "vcmd.h"
#include "excmd.h"

static int	 txt_abbrev __P((SCR *, TEXT *, CHAR_T *, int, int *, int *));
static void	 txt_ai_resolve __P((SCR *, TEXT *));
static TEXT	*txt_backup __P((SCR *, EXF *, TEXTH *, TEXT *, u_int));
static void	 txt_err __P((SCR *, EXF *, TEXTH *));
static int	 txt_hex __P((SCR *, TEXT *, int *, CHAR_T *));
static int	 txt_indent __P((SCR *, TEXT *));
static int	 txt_margin __P((SCR *, TEXT *, int *, CHAR_T *));
static int	 txt_outdent __P((SCR *, TEXT *));
static void	 txt_showmatch __P((SCR *, EXF *));
static void	 txt_Rcleanup __P((SCR *,
		    TEXTH *, TEXT *, const char *, const size_t));
static int	 txt_resolve __P((SCR *, EXF *, TEXTH *));
static void	 txt_unmap __P((SCR *, TEXT *, u_int *));

/* Cursor character (space is hard to track on the screen). */
#if defined(DEBUG) && 0
#undef	CURSOR_CH
#define	CURSOR_CH	'+'
#endif

/* Local version of BINC. */
#define	TBINC(sp, lp, llen, nlen) {					\
	if ((nlen) > llen && binc(sp, &(lp), &(llen), nlen))		\
		goto err;						\
}

/*
 * v_ntext --
 *	Read in text from the user.
 *
 * !!!
 * Historic vi did a special screen optimization for tab characters.  For
 * the keystrokes "iabcd<esc>0C<tab>", the tab would overwrite the rest of
 * the string when it was displayed.  Because this implementation redisplays
 * the entire line on each keystroke, the "bcd" gets pushed to the right as
 * we ignore that the user has "promised" to change the rest of the characters.
 * Users have noticed, but this isn't worth fixing, and, the way that the
 * historic vi did it results in an even worse bug.  Given the keystrokes
 * "iabcd<esc>0R<tab><esc>", the "bcd" disappears, and magically reappears
 * on the second <esc> key.
 */
int
v_ntext(sp, ep, tiqh, tm, lp, len, rp, prompt, ai_line, flags)
	SCR *sp;
	EXF *ep;
	TEXTH *tiqh;
	MARK *tm;		/* To MARK. */
	const char *lp;		/* Input line. */
	const size_t len;	/* Input line length. */
	MARK *rp;		/* Return MARK. */
	int prompt;		/* Prompt to display. */
	recno_t ai_line;	/* Line number to use for autoindent count. */
	u_int flags;		/* TXT_ flags. */
{
				/* State of abbreviation checks. */
	enum { A_NOTSET, A_NOTWORD, A_INWORD } abb;
				/* State of the "[^0]^D" sequences. */
	enum { C_NOTSET, C_CARATSET, C_NOCHANGE, C_ZEROSET } carat_st;
				/* State of the hex input character. */
	enum { H_NOTSET, H_NEXTCHAR, H_INHEX } hex;
				/* State of quotation. */
	enum { Q_NOTSET, Q_NEXTCHAR, Q_THISCHAR } quoted;
	CH ikey;		/* Input character structure. */
	CHAR_T ch;		/* Input character. */
	TEXT *tp, *ntp, ait;	/* Input and autoindent text structures. */
	size_t rcol;		/* 0-N: insert offset in the replay buffer. */
	size_t col;		/* Current column. */
	u_long margin;		/* Wrapmargin value. */
	u_int iflags;		/* Input flags. */
	int ab_cnt, ab_turnoff;	/* Abbreviation count, if turned off. */
	int eval;		/* Routine return value. */
	int replay;		/* If replaying a set of input. */
	int showmatch;		/* Showmatch set on this character. */
	int testnr;		/* Test first character for nul replay. */
	int max, tmp;
	int unmap_tst;		/* Input map needs testing. */
	char *p;

	/*
	 * Set the input flag, so tabs get displayed correctly
	 * and everyone knows that the text buffer is in use.
	 */
	F_SET(sp, S_INPUT);

	/* Local initialization. */
	eval = 0;

	/*
	 * Get one TEXT structure with some initial buffer space, reusing
	 * the last one if it's big enough.  (All TEXT bookkeeping fields
	 * default to 0 -- text_init() handles this.)  If changing a line,
	 * copy it into the TEXT buffer.
	 */
	if (tiqh->cqh_first != (void *)tiqh) {
		tp = tiqh->cqh_first;
		if (tp->q.cqe_next != (void *)tiqh || tp->lb_len < len + 32) {
			text_lfree(tiqh);
			goto newtp;
		}
		tp->ai = tp->insert = tp->offset = tp->owrite = 0;
		if (lp != NULL) {
			tp->len = len;
			memmove(tp->lb, lp, len);
		} else
			tp->len = 0;
	} else {
newtp:		if ((tp = text_init(sp, lp, len, len + 32)) == NULL)
			return (1);
		CIRCLEQ_INSERT_HEAD(tiqh, tp, q);
	}

	/* Set the starting line number. */
	tp->lno = sp->lno;

	/*
	 * Set the insert and overwrite counts.  If overwriting characters,
	 * do insertion afterward.  If not overwriting characters, assume
	 * doing insertion.  If change is to a mark, emphasize it with an
	 * END_CH.
	 */
	if (len) {
		if (LF_ISSET(TXT_OVERWRITE)) {
			tp->owrite = (tm->cno - sp->cno) + 1;
			tp->insert = (len - tm->cno) - 1;
		} else
			tp->insert = len - sp->cno;

		if (LF_ISSET(TXT_EMARK))
			tp->lb[tm->cno] = END_CH;
	}

	/*
	 * Many of the special cases in this routine are to handle autoindent
	 * support.  Somebody decided that it would be a good idea if "^^D"
	 * and "0^D" deleted all of the autoindented characters.  In an editor
	 * that takes single character input from the user, this beggars the
	 * imagination.  Note also, "^^D" resets the next lines' autoindent,
	 * but "0^D" doesn't.
	 *
	 * We assume that autoindent only happens on empty lines, so insert
	 * and overwrite will be zero.  If doing autoindent, figure out how
	 * much indentation we need and fill it in.  Update input column and
	 * screen cursor as necessary.
	 */
	if (LF_ISSET(TXT_AUTOINDENT) && ai_line != OOBLNO) {
		if (txt_auto(sp, ep, ai_line, NULL, 0, tp))
			return (1);
		sp->cno = tp->ai;
	} else {
		/*
		 * The cc and S commands have a special feature -- leading
		 * <blank> characters are handled as autoindent characters.
		 * Beauty!
		 */
		if (LF_ISSET(TXT_AICHARS)) {
			tp->offset = 0;
			tp->ai = sp->cno;
		} else
			tp->offset = sp->cno;
	}

	/* If getting a command buffer from the user, there may be a prompt. */
	if (LF_ISSET(TXT_PROMPT)) {
		tp->lb[sp->cno++] = prompt;
		++tp->len;
		++tp->offset;
	}

	/*
	 * If appending after the end-of-line, add a space into the buffer
	 * and move the cursor right.  This space is inserted, i.e. pushed
	 * along, and then deleted when the line is resolved.  Assumes that
	 * the cursor is already positioned at the end of the line.  This
	 * avoids the nastiness of having the cursor reside on a magical
	 * column, i.e. a column that doesn't really exist.  The only down
	 * side is that we may wrap lines or scroll the screen before it's
	 * strictly necessary.  Not a big deal.
	 */
	if (LF_ISSET(TXT_APPENDEOL)) {
		tp->lb[sp->cno] = CURSOR_CH;
		++tp->len;
		++tp->insert;
	}

	/*
	 * Historic practice is that the wrapmargin value was a distance
	 * from the RIGHT-HAND column, not the left.  It's more useful to
	 * us as a distance from the left-hand column.
	 *
	 * !!!
	 * Replay commands are not affected by wrapmargin values.  What
	 * I found surprising was that people actually depend on it, as
	 * in this gem of a macro which centers lines:
	 *
	 *	map #c $mq81a ^V^[81^V|D`qld0:s/  / /g^V^M$p
	 *
	 * XXX
	 * Setting margin causes a significant performance hit.  Normally
	 * we don't update the screen if there are keys waiting, but we
	 * have to if margin is set, otherwise the screen routines don't
	 * know where the cursor is.
	 */
	if (LF_ISSET(TXT_REPLAY) || !LF_ISSET(TXT_WRAPMARGIN))
		margin = 0;
	else if ((margin = O_VAL(sp, O_WRAPMARGIN)) != 0)
		margin = sp->cols - margin;

	/* Initialize abbreviations checks. */
	if (F_ISSET(sp->gp, G_ABBREV) && LF_ISSET(TXT_MAPINPUT)) {
		abb = A_INWORD;
		ab_cnt = ab_turnoff = 0;
	} else
		abb = A_NOTSET;

	/*
	 * Set up the dot command.  Dot commands are done by saving the
	 * actual characters and replaying the input.  We have to push
	 * the characters onto the key stack and then handle them normally,
	 * otherwise things like wrapmargin will fail.
	 *
	 * XXX
	 * It would be nice if we could swallow backspaces and such, but
	 * it's not all that easy to do.  Another possibility would be to
	 * recognize full line insertions, which could be performed quickly,
	 * without replay.
	 */
nullreplay:
	rcol = 0;
	if (replay = LF_ISSET(TXT_REPLAY)) {
		/*
		 * !!!
		 * Historically, it wasn't an error to replay non-existent
		 * input.  This test is necessary, we get here by the user
		 * doing an input command followed by a nul.
		 *
		 * !!!
		 * Historically, vi did not remap or reabbreviate replayed
		 * input.  It did, however, beep at you if you changed an
		 * abbreviation and then replayed the input.  We're not that
		 * compatible.
		 */
		if (VIP(sp)->rep == NULL)
			return (0);
		if (term_push(sp, VIP(sp)->rep, VIP(sp)->rep_cnt, 0, CH_NOMAP))
			return (1);
		testnr = 0;
		abb = A_NOTSET;
		LF_CLR(TXT_RECORD);
	} else
		testnr = 1;

	unmap_tst = LF_ISSET(TXT_MAPINPUT) && LF_ISSET(TXT_INFOLINE);
	iflags = LF_ISSET(TXT_MAPCOMMAND | TXT_MAPINPUT);
	for (showmatch = 0,
	    carat_st = C_NOTSET, hex = H_NOTSET, quoted = Q_NOTSET;;) {
		/*
		 * Reset the line and update the screen.  (The txt_showmatch()
		 * code refreshes the screen for us.)  Don't refresh unless
		 * we're about to wait on a character or we need to know where
		 * the cursor really is.
		 */
		if (showmatch || margin || !KEYS_WAITING(sp)) {
			if (sp->s_change(sp, ep, tp->lno, LINE_RESET))
				goto err;
			if (showmatch) {
				showmatch = 0;
				txt_showmatch(sp, ep);
			} else if (sp->s_refresh(sp, ep))
				goto err;
		}

		/* Get the next character. */
next_ch:	if (term_key(sp, &ikey, quoted == Q_THISCHAR ?
		    iflags & ~(TXT_MAPCOMMAND | TXT_MAPINPUT) :
		    iflags) != INP_OK)
			goto err;
		ch = ikey.ch;

		/* Abbreviation check.  See comment in txt_abbrev(). */
#define	MAX_ABBREVIATION_EXPANSION	256
		if (ikey.flags & CH_ABBREVIATED) {
			if (++ab_cnt > MAX_ABBREVIATION_EXPANSION) {
				term_ab_flush(sp,
			"Abbreviation exceeded maximum number of characters");
				ab_cnt = 0;
				continue;
			}
		} else
			ab_cnt = 0;

		/*
		 * !!!
		 * Historic feature.  If the first character of the input is
		 * a nul, replay the previous input.  This isn't documented
		 * anywhere, and is a great test of vi clones.
		 */
		if (ch == '\0' && testnr) {
			LF_SET(TXT_REPLAY);
			goto nullreplay;
		}
		testnr = 0;

		/*
		 * Check to see if the character fits into the input (and
		 * replay, if necessary) buffers.  It isn't necessary to
		 * have tp->len bytes, since it doesn't consider overwrite
		 * characters, but not worth fixing.
		 */
		if (LF_ISSET(TXT_RECORD)) {
			TBINC(sp, VIP(sp)->rep, VIP(sp)->rep_len, rcol + 1);
			VIP(sp)->rep[rcol++] = ch;
		}
		TBINC(sp, tp->lb, tp->lb_len, tp->len + 1);

		/*
		 * If the character was quoted, replace the last character
		 * (the literal mark) with the new character.  If quoted
		 * by someone else, simply insert the character.
		 *
		 * !!!
		 * Extension -- if the quoted character is HEX_CH, enter hex
		 * mode.  If the user enters "<HEX_CH>[isxdigit()]*" we will
		 * try to use the value as a character.  Anything else resets
		 * hex mode.
		 */
		if (ikey.flags & CH_QUOTED)
			goto insq_ch;
		if (quoted == Q_THISCHAR) {
			--sp->cno;
			++tp->owrite;
			quoted = Q_NOTSET;

			if (ch == HEX_CH)
				hex = H_NEXTCHAR;
			goto insq_ch;
		}

		switch (ikey.value) {
		case K_CR:
		case K_NL:				/* New line. */
#define	LINE_RESOLVE {							\
			/*						\
			 * Handle abbreviations.  If there was one,	\
			 * discard the replay characters.		\
			 */						\
			if (abb == A_INWORD && !replay) {		\
				if (txt_abbrev(sp, tp, &ch,		\
				    LF_ISSET(TXT_INFOLINE), &tmp,	\
				    &ab_turnoff))			\
					goto err;			\
				if (tmp) {				\
					if (LF_ISSET(TXT_RECORD))	\
						rcol -= tmp;		\
					goto next_ch;			\
				}					\
			}						\
			if (abb != A_NOTSET)				\
				abb = A_NOTWORD;			\
			if (unmap_tst)					\
				txt_unmap(sp, tp, &iflags);		\
			/* Handle hex numbers. */			\
			if (hex == H_INHEX) {				\
				if (txt_hex(sp, tp, &tmp, &ch))		\
					goto err;			\
				if (tmp) {				\
					hex = H_NOTSET;			\
					goto next_ch;			\
				}					\
			}						\
			/* Clean up for the 'R' command. */		\
			if (LF_ISSET(TXT_REPLACE))			\
				txt_Rcleanup(sp, tiqh, tp, lp, len);	\
			/* Delete any appended cursor. */		\
			if (LF_ISSET(TXT_APPENDEOL)) {			\
				--tp->len;				\
				--tp->insert;				\
			}						\
}
			LINE_RESOLVE;

			/* CR returns from the vi command line. */
			if (LF_ISSET(TXT_CR)) {
				/*
				 * If a script window and not the colon
				 * line, push a <cr> so it gets executed.
				 */
				if (F_ISSET(sp, S_SCRIPT) &&
				    !LF_ISSET(TXT_INFOLINE))
					(void)term_push(sp,
					    "\r", 1, 0, CH_NOMAP);
				goto k_escape;
			}

			/*
			 * Historic practice was to delete any <blank>
			 * characters following the inserted newline.
			 * This affects the 'R', 'c', and 's' commands.
			 */
			for (p = tp->lb + sp->cno + tp->owrite;
			    tp->insert && isblank(*p);
			    ++p, ++tp->owrite, --tp->insert);

			/*
			 * Move any remaining insert characters into
			 * a new TEXT structure.
			 */
			if ((ntp = text_init(sp,
			    tp->lb + sp->cno + tp->owrite,
			    tp->insert, tp->insert + 32)) == NULL)
				goto err;

			/* Set bookkeeping for the new line. */
			ntp->lno = tp->lno + 1;
			ntp->insert = tp->insert;

			/*
			 * Note if the user inserted any characters on this
			 * line.  Done before calling txt_ai_resolve() because
			 * it changes the value of sp->cno without making the
			 * corresponding changes to tp->ai.
			 */
			tmp = sp->cno <= tp->ai;

			/*
			 * Resolve autoindented characters for the old line.
			 * Reset the autoindent line value.  0^D keeps the ai
			 * line from changing, ^D changes the level, even if
			 * there are no characters in the old line.  Note,
			 * if using the current tp structure, use the cursor
			 * as the length, the user may have erased autoindent
			 * characters.
			 */
			if (LF_ISSET(TXT_AUTOINDENT)) {
				txt_ai_resolve(sp, tp);

				if (carat_st == C_NOCHANGE) {
					if (txt_auto(sp, ep,
					    OOBLNO, &ait, ait.ai, ntp))
						goto err;
					FREE_SPACE(sp, ait.lb, ait.lb_len);
				} else
					if (txt_auto(sp, ep,
					    OOBLNO, tp, sp->cno, ntp))
						goto err;
				carat_st = C_NOTSET;
			}

			/*
			 * If the user hasn't entered any characters, delete
			 * any autoindent characters.
			 *
			 * !!!
			 * Historic vi didn't get the insert test right, if
			 * there were characters after the cursor, entering
			 * a <cr> left the autoindent characters on the line.
			 */
			if (tmp)
				sp->cno = 0;

			/* Reset bookkeeping for the old line. */
			tp->len = sp->cno;
			tp->ai = tp->insert = tp->owrite = 0;

			/* New cursor position. */
			sp->cno = ntp->ai;

			/* New lines are TXT_APPENDEOL if nothing to insert. */
			if (ntp->insert == 0) {
				TBINC(sp, ntp->lb, ntp->lb_len, ntp->len + 1);
				LF_SET(TXT_APPENDEOL);
				ntp->lb[sp->cno] = CURSOR_CH;
				++ntp->insert;
				++ntp->len;
			}

			/* Update the old line. */
			if (sp->s_change(sp, ep, tp->lno, LINE_RESET))
				goto err;

			/*
			 * Swap old and new TEXT's, and insert the new TEXT
			 * into the queue.  (DON'T insert until the old line
			 * has been updated, or the inserted line count in
			 * line.c:file_gline() will be wrong.)
			 */
			tp = ntp;
			CIRCLEQ_INSERT_TAIL(tiqh, tp, q);

			/* Reset the cursor. */
			sp->lno = tp->lno;

			/* Update the new line. */
			if (sp->s_change(sp, ep, tp->lno, LINE_INSERT))
				goto err;

			/* Set the renumber bit. */
			F_SET(sp, S_RENUMBER);

			/* Refresh if nothing waiting. */
			if ((margin || !KEYS_WAITING(sp)) &&
			    sp->s_refresh(sp, ep))
				goto err;
			goto next_ch;
		case K_ESCAPE:				/* Escape. */
			if (!LF_ISSET(TXT_ESCAPE))
				goto ins_ch;

			LINE_RESOLVE;

			/*
			 * If there aren't any trailing characters in the line
			 * and the user hasn't entered any characters, delete
			 * the autoindent characters.
			 */
			if (!tp->insert && sp->cno <= tp->ai) {
				tp->len = tp->owrite = 0;
				sp->cno = 0;
			} else if (LF_ISSET(TXT_AUTOINDENT))
				txt_ai_resolve(sp, tp);

			/* If there are insert characters, copy them down. */
k_escape:		if (tp->insert && tp->owrite)
				memmove(tp->lb + sp->cno,
				    tp->lb + sp->cno + tp->owrite, tp->insert);
			tp->len -= tp->owrite;

			/*
			 * Delete any lines that were inserted into the text
			 * structure and then erased.
			 */
			while (tp->q.cqe_next != (void *)tiqh) {
				ntp = tp->q.cqe_next;
				CIRCLEQ_REMOVE(tiqh, ntp, q);
				text_free(ntp);
			}

			/*
			 * If not resolving the lines into the file, end
			 * it with a nul.
			 *
			 * XXX
			 * This is wrong, should pass back a length.
			 */
			if (LF_ISSET(TXT_RESOLVE)) {
				if (txt_resolve(sp, ep, tiqh))
					goto err;
				/*
				 * Clear input flag -- input buffer no longer
				 * valid.
				 */
				F_CLR(sp, S_INPUT);
			} else {
				TBINC(sp, tp->lb, tp->lb_len, tp->len + 1);
				tp->lb[tp->len] = '\0';
			}

			/*
			 * Set the return cursor position to rest on the last
			 * inserted character.
			 */
			if (rp != NULL) {
				rp->lno = tp->lno;
				rp->cno = sp->cno ? sp->cno - 1 : 0;
				if (sp->s_change(sp, ep, rp->lno, LINE_RESET))
					goto err;
			}
			goto ret;
		case K_CARAT:			/* Delete autoindent chars. */
			if (LF_ISSET(TXT_AUTOINDENT) && sp->cno <= tp->ai)
				carat_st = C_CARATSET;
			goto ins_ch;
		case K_ZERO:			/* Delete autoindent chars. */
			if (LF_ISSET(TXT_AUTOINDENT) && sp->cno <= tp->ai)
				carat_st = C_ZEROSET;
			goto ins_ch;
		case K_CNTRLD:			/* Delete autoindent char. */
			/*
			 * If in the first column or no characters to erase,
			 * ignore the ^D (this matches historic practice).  If
			 * not doing autoindent or already inserted non-ai
			 * characters, it's a literal.  The latter test is done
			 * in the switch, as the CARAT forms are N + 1, not N.
			 */
			if (!LF_ISSET(TXT_AUTOINDENT))
				goto ins_ch;
			if (sp->cno == 0 || tp->ai == 0)
				break;
			switch (carat_st) {
			case C_CARATSET:	/* ^^D */
				if (sp->cno > tp->ai + tp->offset + 1)
					goto ins_ch;

				/* Save the ai string for later. */
				ait.lb = NULL;
				ait.lb_len = 0;
				TBINC(sp, ait.lb, ait.lb_len, tp->ai);
				memmove(ait.lb, tp->lb, tp->ai);
				ait.ai = ait.len = tp->ai;

				carat_st = C_NOCHANGE;
				goto leftmargin;
			case C_ZEROSET:		/* 0^D */
				if (sp->cno > tp->ai + tp->offset + 1)
					goto ins_ch;
				carat_st = C_NOTSET;
leftmargin:			tp->lb[sp->cno - 1] = ' ';
				tp->owrite += sp->cno - tp->offset;
				tp->ai = 0;
				sp->cno = tp->offset;
				break;
			case C_NOTSET:		/* ^D */
				if (sp->cno > tp->ai + tp->offset)
					goto ins_ch;
				(void)txt_outdent(sp, tp);
				break;
			default:
				abort();
			}
			break;
		case K_VERASE:			/* Erase the last character. */
			/*
			 * If can erase over the prompt, return.  Len is 0
			 * if backspaced over the prompt, 1 if only CR entered.
			 */
			if (LF_ISSET(TXT_BS) && sp->cno <= tp->offset) {
				tp->len = 0;
				goto ret;
			}

			/*
			 * If at the beginning of the line, try and drop back
			 * to a previously inserted line.
			 */
			if (sp->cno == 0) {
				if ((ntp = txt_backup(sp,
				    ep, tiqh, tp, flags)) == NULL)
					goto err;
				tp = ntp;
				break;
			}

			/* If nothing to erase, bell the user. */
			if (sp->cno <= tp->offset) {
				msgq(sp, M_BERR,
				    "No more characters to erase.");
				break;
			}

			/* Drop back one character. */
			--sp->cno;

			/*
			 * Increment overwrite, decrement ai if deleted.
			 *
			 * !!!
			 * Historic vi did not permit users to use erase
			 * characters to delete autoindent characters.
			 */
			++tp->owrite;
			if (sp->cno < tp->ai)
				--tp->ai;
			break;
		case K_VINTR:
			/*
			 * !!!
			 * Historically, <interrupt> exited the user from
			 * editing the infoline, and returned to the main
			 * screen.  It also beeped the terminal, but that
			 * seems excessive.
			 */
			if (LF_ISSET(TXT_INFOLINE)) {
				tp->lb[tp->len = 0] = '\0';
				goto ret;
			}
			goto ins_ch;
		case K_VWERASE:			/* Skip back one word. */
			/*
			 * If at the beginning of the line, try and drop back
			 * to a previously inserted line.
			 */
			if (sp->cno == 0) {
				if ((ntp = txt_backup(sp,
				    ep, tiqh, tp, flags)) == NULL)
					goto err;
				tp = ntp;
			}

			/*
			 * If at offset, nothing to erase so bell the user.
			 */
			if (sp->cno <= tp->offset) {
				msgq(sp, M_BERR,
				    "No more characters to erase.");
				break;
			}

			/*
			 * First werase goes back to any autoindent
			 * and second werase goes back to the offset.
			 *
			 * !!!
			 * Historic vi did not permit users to use erase
			 * characters to delete autoindent characters.
			 */
			if (tp->ai && sp->cno > tp->ai)
				max = tp->ai;
			else {
				tp->ai = 0;
				max = tp->offset;
			}

			/* Skip over trailing space characters. */
			while (sp->cno > max && isblank(tp->lb[sp->cno - 1])) {
				--sp->cno;
				++tp->owrite;
			}
			if (sp->cno == max)
				break;
			/*
			 * There are three types of word erase found on UNIX
			 * systems.  They can be identified by how the string
			 * /a/b/c is treated -- as 1, 3, or 6 words.  Historic
			 * vi had two classes of characters, and strings were
			 * delimited by them and <blank>'s, so, 6 words.  The
			 * historic tty interface used <blank>'s to delimit
			 * strings, so, 1 word.  The algorithm offered in the
			 * 4.4BSD tty interface (as stty altwerase) treats it
			 * as 3 words -- there are two classes of characters,
			 * and strings are delimited by them and <blank>'s.
			 * The difference is that the type of the first erased
			 * character erased is ignored, which is exactly right
			 * when erasing pathname components.  Here, the options
			 * TXT_ALTWERASE and TXT_TTYWERASE specify the 4.4BSD
			 * tty interface and the historic tty driver behavior,
			 * respectively, and the default is the same as the
			 * historic vi behavior.
			 */
			if (LF_ISSET(TXT_TTYWERASE))
				while (sp->cno > max) {
					--sp->cno;
					++tp->owrite;
					if (isblank(tp->lb[sp->cno - 1]))
						break;
				}
			else {
				if (LF_ISSET(TXT_ALTWERASE)) {
					--sp->cno;
					++tp->owrite;
					if (isblank(tp->lb[sp->cno - 1]))
						break;
				}
				if (sp->cno > max)
					tmp = inword(tp->lb[sp->cno - 1]);
				while (sp->cno > max) {
					--sp->cno;
					++tp->owrite;
					if (tmp != inword(tp->lb[sp->cno - 1])
					    || isblank(tp->lb[sp->cno - 1]))
						break;
				}
			}
			break;
		case K_VKILL:			/* Restart this line. */
			/*
			 * If at the beginning of the line, try and drop back
			 * to a previously inserted line.
			 */
			if (sp->cno == 0) {
				if ((ntp = txt_backup(sp,
				    ep, tiqh, tp, flags)) == NULL)
					goto err;
				tp = ntp;
			}

			/* If at offset, nothing to erase so bell the user. */
			if (sp->cno <= tp->offset) {
				msgq(sp, M_BERR,
				    "No more characters to erase.");
				break;
			}

			/*
			 * First kill goes back to any autoindent
			 * and second kill goes back to the offset.
			 *
			 * !!!
			 * Historic vi did not permit users to use erase
			 * characters to delete autoindent characters.
			 */
			if (tp->ai && sp->cno > tp->ai)
				max = tp->ai;
			else {
				tp->ai = 0;
				max = tp->offset;
			}
			tp->owrite += sp->cno - max;
			sp->cno = max;
			break;
		case K_CNTRLT:			/* Add autoindent char. */
			if (!LF_ISSET(TXT_CNTRLT))
				goto ins_ch;
			if (txt_indent(sp, tp))
				goto err;
			goto ebuf_chk;
		case K_CNTRLZ:
			(void)sp->s_suspend(sp);
			break;
#ifdef	HISTORIC_PRACTICE_IS_TO_INSERT_NOT_REPAINT
		case K_FORMFEED:
			F_SET(sp, S_REFRESH);
			break;
#endif
		case K_RIGHTBRACE:
		case K_RIGHTPAREN:
			showmatch = LF_ISSET(TXT_SHOWMATCH);
			goto ins_ch;
		case K_VLNEXT:			/* Quote the next character. */
			/* If in hex mode, see if we've entered a hex value. */
			if (hex == H_INHEX) {
				if (txt_hex(sp, tp, &tmp, &ch))
					goto err;
				if (tmp) {
					hex = H_NOTSET;
					goto next_ch;
				}
			}
			ch = '^';
			quoted = Q_NEXTCHAR;
			goto insq_ch;
		default:			/* Insert the character. */
ins_ch:			/*
	 		 * Historically, vi eliminated nul's out of hand.  If
			 * the beautify option was set, it also deleted any
			 * unknown ASCII value less than space (040) and the
			 * del character (0177), except for tabs.  Unknown is
			 * a key word here.  Most vi documentation claims that
			 * it deleted everything but <tab>, <nl> and <ff>, as
			 * that's what the original 4BSD documentation said.
			 * This is obviously wrong, however, as <esc> would be
			 * included in that list.  What we do is eliminate any
			 * unquoted, iscntrl() character that wasn't a replay
			 * and wasn't handled specially, except <tab> or <ff>.
			 */
			if (LF_ISSET(TXT_BEAUTIFY) && iscntrl(ch) &&
			    ikey.value != K_FORMFEED && ikey.value != K_TAB) {
				msgq(sp, M_BERR,
				    "Illegal character; quote to enter.");
				break;
			}
insq_ch:		/*
			 * If entering a non-word character after a word, check
			 * for abbreviations.  If there was one, discard the
			 * replay characters.  If entering a blank character,
			 * check for unmap commands, as well.
			 */
			if (!inword(ch)) {
				if (abb == A_INWORD && !replay) {
					if (txt_abbrev(sp, tp, &ch,
					    LF_ISSET(TXT_INFOLINE),
					    &tmp, &ab_turnoff))
						goto err;
					if (tmp) {
						if (LF_ISSET(TXT_RECORD))
							rcol -= tmp;
						goto next_ch;
					}
				}
				if (isblank(ch) && unmap_tst)
					txt_unmap(sp, tp, &iflags);
			}
			/* If in hex mode, see if we've entered a hex value. */
			if (hex == H_INHEX && !isxdigit(ch)) {
				if (txt_hex(sp, tp, &tmp, &ch))
					goto err;
				if (tmp) {
					hex = H_NOTSET;
					goto next_ch;
				}
			}
			/* Check to see if we've crossed the margin. */
			if (margin) {
				if (sp->s_column(sp, ep, &col))
					goto err;
				if (col >= margin) {
					if (txt_margin(sp, tp, &tmp, &ch))
						goto err;
					if (tmp)
						goto next_ch;
				}
			}
			if (abb != A_NOTSET)
				abb = inword(ch) ? A_INWORD : A_NOTWORD;

			if (tp->owrite)		/* Overwrite a character. */
				--tp->owrite;
			else if (tp->insert) {	/* Insert a character. */
				++tp->len;
				if (tp->insert == 1)
					tp->lb[sp->cno + 1] = tp->lb[sp->cno];
				else
					memmove(tp->lb + sp->cno + 1,
					    tp->lb + sp->cno, tp->insert);
			}

			tp->lb[sp->cno++] = ch;

			/*
			 * If we've reached the end of the buffer, then we
			 * need to switch into insert mode.  This happens
			 * when there's a change to a mark and the user puts
			 * in more characters than the length of the motion.
			 */
ebuf_chk:		if (sp->cno >= tp->len) {
				TBINC(sp, tp->lb, tp->lb_len, tp->len + 1);
				LF_SET(TXT_APPENDEOL);
				tp->lb[sp->cno] = CURSOR_CH;
				++tp->insert;
				++tp->len;
			}

			if (hex == H_NEXTCHAR)
				hex = H_INHEX;
			if (quoted == Q_NEXTCHAR)
				quoted = Q_THISCHAR;
			break;
		}
#if defined(DEBUG) && 1
		if (sp->cno + tp->insert + tp->owrite != tp->len)
			msgq(sp, M_ERR,
			    "len %u != cno: %u ai: %u insert %u overwrite %u",
			    tp->len, sp->cno, tp->ai, tp->insert, tp->owrite);
		tp->len = sp->cno + tp->insert + tp->owrite;
#endif
	}

	/* Clear input flag. */
ret:	F_CLR(sp, S_INPUT);

	if (LF_ISSET(TXT_RECORD))
		VIP(sp)->rep_cnt = rcol;
	return (eval);

	/* Error jump. */
err:	eval = 1;
	txt_err(sp, ep, tiqh);
	goto ret;
}

/*
 * txt_abbrev --
 *	Handle abbreviations.
 */
static int
txt_abbrev(sp, tp, pushcp, isinfoline, didsubp, turnoffp)
	SCR *sp;
	TEXT *tp;
	CHAR_T *pushcp;
	int isinfoline, *didsubp, *turnoffp;
{
	CHAR_T ch;
	SEQ *qp;
	size_t len, off;
	char *p;

	/*
	 * Find the start of the "word".  Historically, abbreviations
	 * could be preceded by any non-word character.
	 */
	for (off = sp->cno - 1, p = tp->lb + off, len = 0;; --p, --off) {
		if (!inword(*p)) {
			++p;
			break;
		}
		++len;
		if (off == tp->ai || off == tp->offset)
			break;
	}

	/*
	 * !!!
	 * Historic vi exploded abbreviations on the command line.  This has
	 * obvious problems in that unabbreviating the string can be extremely
	 * tricky, particularly if the string has, say, an embedded escape
	 * character.  Personally, I think it's a stunningly bad idea.  Other
	 * examples of problems this caused in historic vi are:
	 *	:ab foo bar
	 *	:ab foo baz
	 * results in "bar" being abbreviated to "baz", which wasn't what the
	 * user had in mind at all.  Also, the commands:
	 *	:ab foo bar
	 *	:unab foo<space>
	 * resulted in an error message that "bar" wasn't mapped.  Finally,
	 * since the string was already exploded by the time the unabbreviate
	 * command got it, all it knew was that an abbreviation had occurred.
	 * Cleverly, it checked the replacement string for its unabbreviation
	 * match, which meant that the commands:
	 *	:ab foo1 bar
	 *	:ab foo2 bar
	 *	:unab foo2
	 * unabbreviates "foo1", and the commands:
	 *	:ab foo bar
	 *	:ab bar baz
	 * unabbreviates "foo"!
	 *
	 * Anyway, people neglected to first ask my opinion before they wrote
	 * macros that depend on this stuff, so, we make this work as follows.
	 * When checking for an abbreviation on the command line, if we get a
	 * string which is <blank> terminated and which starts at the beginning
	 * of the line, we check to see it is the abbreviate or unabbreviate
	 * commands.  If it is, turn abbreviations off and return as if no
	 * abbreviation was found.  Note also, minor trickiness, so that if the
	 * user erases the line and starts another command, we go ahead an turn
	 * abbreviations back on.
	 *
	 * This makes the layering look like a Nachos Supreme.
	 */
	*didsubp = 0;
	if (isinfoline)
		if (off == tp->ai || off == tp->offset)
			if (ex_is_abbrev(p, len)) {
				*turnoffp = 1;
				return (0);
			} else
				*turnoffp = 0;
		else
			if (*turnoffp)
				return (0);

	/* Check for any abbreviations. */
	if ((qp = seq_find(sp, NULL, p, len, SEQ_ABBREV, NULL)) == NULL)
		return (0);

	/*
	 * Push the abbreviation onto the tty stack.  Historically, characters
	 * resulting from an abbreviation expansion were themselves subject to
	 * map expansions, O_SHOWMATCH matching etc.  This means the expanded
	 * characters will be re-tested for abbreviations.  It's difficult to
	 * know what historic practice in this case was, since abbreviations
	 * were applied to :colon command lines, so entering abbreviations that
	 * looped was tricky, although possible.  In addition, obvious loops
	 * didn't work as expected.  (The command ':ab a b|ab b c|ab c a' will
	 * silently only implement and/or display the last abbreviation.)
	 *
	 * This implementation doesn't recover well from such abbreviations.
	 * The main input loop counts abbreviated characters, and, when it
	 * reaches a limit, discards any abbreviated characters on the queue.
	 * It's difficult to back up to the original position, as the replay
	 * queue would have to be adjusted, and the line state when an initial
	 * abbreviated character was received would have to be saved.
	 */
	ch = *pushcp;
	if (term_push(sp, &ch, 1, 0, CH_ABBREVIATED))
		return (1);
	if (term_push(sp, qp->output, qp->olen, 0, CH_ABBREVIATED))
		return (1);

	/*
	 * Move the cursor to the start of the abbreviation,
	 * adjust the length.
	 */
	sp->cno -= len;
	tp->len -= len;

	/* Copy any insert characters back. */
	if (tp->insert)
		memmove(tp->lb + sp->cno + tp->owrite,
		    tp->lb + sp->cno + tp->owrite + len, tp->insert);

	/*
	 * We return the length of the abbreviated characters.  This is so
	 * the calling routine can replace the replay characters with the
	 * abbreviation.  This means that subsequent '.' commands will produce
	 * the same text, regardless of intervening :[un]abbreviate commands.
	 * This is historic practice.
	 */
	*didsubp = len;
	return (0);
}

/*
 * txt_unmap --
 *	Handle the unmap command.
 */
static void
txt_unmap(sp, tp, iflagsp)
	SCR *sp;
	TEXT *tp;
	u_int *iflagsp;
{
	size_t len, off;
	char *p;

	/* Find the beginning of this "word". */
	for (off = sp->cno - 1, p = tp->lb + off, len = 0;; --p, --off) {
		if (isblank(*p)) {
			++p;
			break;
		}
		++len;
		if (off == tp->ai || off == tp->offset)
			break;
	}

	/*
	 * !!!
	 * Historic vi exploded input mappings on the command line.  See the
	 * txt_abbrev() routine for an explanation of the problems inherent
	 * in this.
	 *
	 * We make this work as follows.  If we get a string which is <blank>
	 * terminated and which starts at the beginning of the line, we check
	 * to see it is the unmap command.  If it is, we return that the input
	 * mapping should be turned off.  Note also, minor trickiness, so that
	 * if the user erases the line and starts another command, we go ahead
	 * an turn mapping back on.
	 */
	if ((off == tp->ai || off == tp->offset) && ex_is_unmap(p, len))
		*iflagsp &= ~TXT_MAPINPUT;
	else
		*iflagsp |= TXT_MAPINPUT;
}


/* Offset to next column of stop size. */
#define	STOP_OFF(c, stop)	(stop - (c) % stop)

/*
 * txt_ai_resolve --
 *	When a line is resolved by <esc> or <cr>, review autoindent
 *	characters.
 */
static void
txt_ai_resolve(sp, tp)
	SCR *sp;
	TEXT *tp;
{
	u_long ts;
	int del;
	size_t cno, len, new, old, scno, spaces, tab_after_sp, tabs;
	char *p;

	/*
	 * If the line is empty, has an offset, or no autoindent
	 * characters, we're done.
	 */
	if (!tp->len || tp->offset || !tp->ai)
		return;

	/*
	 * The autoindent characters plus any leading <blank> characters
	 * in the line are resolved into the minimum number of characters.
	 * Historic practice.
	 */
	ts = O_VAL(sp, O_TABSTOP);

	/* Figure out the last <blank> screen column. */
	for (p = tp->lb, scno = 0, len = tp->len,
	    spaces = tab_after_sp = 0; len-- && isblank(*p); ++p)
		if (*p == '\t') {
			if (spaces)
				tab_after_sp = 1;
			scno += STOP_OFF(scno, ts);
		} else {
			++spaces;
			++scno;
		}

	/*
	 * If there are no spaces, or no tabs after spaces and less than
	 * ts spaces, it's already minimal.
	 */
	if (!spaces || !tab_after_sp && spaces < ts)
		return;

	/* Count up spaces/tabs needed to get to the target. */
	for (cno = 0, tabs = 0; cno + STOP_OFF(cno, ts) <= scno; ++tabs)
		cno += STOP_OFF(cno, ts);
	spaces = scno - cno;

	/*
	 * Figure out how many characters we're dropping -- if we're not
	 * dropping any, it's already minimal, we're done.
	 */
	old = p - tp->lb;
	new = spaces + tabs;
	if (old == new)
		return;

	/* Shift the rest of the characters down, adjust the counts. */
	del = old - new;
	memmove(p - del, p, tp->len - old);
	sp->cno -= del;
	tp->len -= del;

	/* Fill in space/tab characters. */
	for (p = tp->lb; tabs--;)
		*p++ = '\t';
	while (spaces--)
		*p++ = ' ';
}

/*
 * txt_auto --
 *	Handle autoindent.  If aitp isn't NULL, use it, otherwise,
 *	retrieve the line.
 */
int
txt_auto(sp, ep, lno, aitp, len, tp)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t len;
	TEXT *aitp, *tp;
{
	size_t nlen;
	char *p, *t;

	if (aitp == NULL) {
		if ((t = file_gline(sp, ep, lno, &len)) == NULL)
			return (0);
	} else
		t = aitp->lb;

	/* Count whitespace characters. */
	for (p = t; len > 0; ++p, --len)
		if (!isblank(*p))
			break;

	/* Set count, check for no indentation. */
	if ((nlen = (p - t)) == 0)
		return (0);

	/* Make sure the buffer's big enough. */
	BINC_RET(sp, tp->lb, tp->lb_len, tp->len + nlen);

	/* Copy the buffer's current contents up. */
	if (tp->len != 0)
		memmove(tp->lb + nlen, tp->lb, tp->len);
	tp->len += nlen;

	/* Copy the indentation into the new buffer. */
	memmove(tp->lb, t, nlen);

	/* Set the autoindent count. */
	tp->ai = nlen;
	return (0);
}

/*
 * txt_backup --
 *	Back up to the previously edited line.
 */
static TEXT *
txt_backup(sp, ep, tiqh, tp, flags)
	SCR *sp;
	EXF *ep;
	TEXTH *tiqh;
	TEXT *tp;
	u_int flags;
{
	TEXT *ntp;
	recno_t lno;
	size_t total;

	/* Get a handle on the previous TEXT structure. */
	if ((ntp = tp->q.cqe_prev) == (void *)tiqh) {
		msgq(sp, M_BERR, "Already at the beginning of the insert");
		return (tp);
	}

	/* Make sure that we have enough space. */
	total = ntp->len + tp->insert;
	if (LF_ISSET(TXT_APPENDEOL))
		++total;
	if (total > ntp->lb_len &&
	    binc(sp, &ntp->lb, &ntp->lb_len, total))
		return (NULL);

	/*
	 * Append a cursor or copy inserted bytes to the end of the old line.
	 * Test for appending a cursor first, because the TEXT insert field
	 * will be 1 if we're appending a cursor.  I don't think there's a
	 * third case, so abort() if there is.
	 */
	if (LF_ISSET(TXT_APPENDEOL)) {
		ntp->lb[ntp->len] = CURSOR_CH;
		ntp->insert = 1;
	} else if (tp->insert) {
		memmove(ntp->lb + ntp->len, tp->lb + tp->owrite, tp->insert);
		ntp->insert = tp->insert;
	} else
		abort();

	/* Set bookkeeping information. */
	sp->lno = ntp->lno;
	sp->cno = ntp->len;
	ntp->len += ntp->insert;

	/* Release the current TEXT. */
	lno = tp->lno;
	CIRCLEQ_REMOVE(tiqh, tp, q);
	text_free(tp);

	/* Update the old line on the screen. */
	if (sp->s_change(sp, ep, lno, LINE_DELETE))
		return (NULL);

	/* Return the old line. */
	return (ntp);
}

/*
 * txt_err --
 *	Handle an error during input processing.
 */
static void
txt_err(sp, ep, tiqh)
	SCR *sp;
	EXF *ep;
	TEXTH *tiqh;
{
	recno_t lno;
	size_t len;

	/*
	 * The problem with input processing is that the cursor is at an
	 * indeterminate position since some input may have been lost due
	 * to a malloc error.  So, try to go back to the place from which
	 * the cursor started, knowing that it may no longer be available.
	 *
	 * We depend on at least one line number being set in the text
	 * chain.
	 */
	for (lno = tiqh->cqh_first->lno;
	    file_gline(sp, ep, lno, &len) == NULL && lno > 0; --lno);

	sp->lno = lno == 0 ? 1 : lno;
	sp->cno = 0;

	/* Redraw the screen, just in case. */
	F_SET(sp, S_REDRAW);
}

/*
 * txt_hex --
 *	Let the user insert any character value they want.
 *
 * !!!
 * This is an extension.  The pattern "^Vx[0-9a-fA-F]*" is a way
 * for the user to specify a character value which their keyboard
 * may not be able to enter.
 */
static int
txt_hex(sp, tp, was_hex, pushcp)
	SCR *sp;
	TEXT *tp;
	int *was_hex;
	CHAR_T *pushcp;
{
	CHAR_T ch, savec;
	size_t len, off;
	u_long value;
	char *p, *wp;

	/*
	 * Null-terminate the string.  Since nul isn't a legal hex value,
	 * this should be okay, and lets us use a local routine, which
	 * presumably understands the character set, to convert the value.
	 */
	savec = tp->lb[sp->cno];
	tp->lb[sp->cno] = 0;

	/* Find the previous HEX_CH. */
	for (off = sp->cno - 1, p = tp->lb + off, len = 0;; --p, --off) {
		if (*p == HEX_CH) {
			wp = p + 1;
			break;
		}
		++len;
		/* If not on this line, there's nothing to do. */
		if (off == tp->ai || off == tp->offset)
			goto nothex;
	}

	/* If no length, then it wasn't a hex value. */
	if (len == 0)
		goto nothex;

	/* Get the value. */
	value = strtol(wp, NULL, 16);
	if (value == LONG_MIN || value == LONG_MAX || value > MAX_CHAR_T) {
nothex:		tp->lb[sp->cno] = savec;
		*was_hex = 0;
		return (0);
	}

	ch = *pushcp;
	if (term_push(sp, &ch, 1, 0, CH_NOMAP | CH_QUOTED))
		return (1);
	ch = value;
	if (term_push(sp, &ch, 1, 0, CH_NOMAP | CH_QUOTED))
		return (1);

	tp->lb[sp->cno] = savec;

	/* Move the cursor to the start of the hex value, adjust the length. */
	sp->cno -= len + 1;
	tp->len -= len + 1;

	/* Copy any insert characters back. */
	if (tp->insert)
		memmove(tp->lb + sp->cno + tp->owrite,
		    tp->lb + sp->cno + tp->owrite + len + 1, tp->insert);

	*was_hex = 1;
	return (0);
}

/*
 * Txt_indent and txt_outdent are truly strange.  ^T and ^D do movements
 * to the next or previous shiftwidth value, i.e. for a 1-based numbering,
 * with shiftwidth=3, ^T moves a cursor on the 7th, 8th or 9th column to
 * the 10th column, and ^D moves it back.
 *
 * !!!
 * The ^T and ^D characters in historical vi only had special meaning when
 * they were the first characters typed after entering text input mode.
 * Since normal erase characters couldn't erase autoindent (in this case
 * ^T) characters, this meant that inserting text into previously existing
 * text was quite strange, ^T only worked if it was the first keystroke,
 * and then it could only be erased by using ^D.  This implementation treats
 * ^T specially anywhere it occurs in the input, and permits the standard
 * erase characters to erase characters inserted using it.
 *
 * XXX
 * Technically, txt_indent, txt_outdent should part of the screen interface,
 * as they require knowledge of the size of a space character on the screen.
 * (Not the size of tabs, because tabs are logically composed of spaces.)
 * They're left in the text code  because they're complicated, not to mention
 * the gruesome awareness that if spaces aren't a single column on the screen
 * for any language, we're into some serious, ah, for lack of a better word,
 * "issues".
 */

/*
 * txt_indent --
 *	Handle ^T indents.
 */
static int
txt_indent(sp, tp)
	SCR *sp;
	TEXT *tp;
{
	u_long sw, ts;
	size_t cno, off, scno, spaces, tabs;

	ts = O_VAL(sp, O_TABSTOP);
	sw = O_VAL(sp, O_SHIFTWIDTH);

	/* Get the current screen column. */
	for (off = scno = 0; off < sp->cno; ++off)
		if (tp->lb[off] == '\t')
			scno += STOP_OFF(scno, ts);
		else
			++scno;

	/* Count up spaces/tabs needed to get to the target. */
	for (cno = scno, scno += STOP_OFF(scno, sw), tabs = 0;
	    cno + STOP_OFF(cno, ts) <= scno; ++tabs)
		cno += STOP_OFF(cno, ts);
	spaces = scno - cno;

	/* Put space/tab characters in place of any overwrite characters. */
	for (; tp->owrite && tabs; --tp->owrite, --tabs, ++tp->ai)
		tp->lb[sp->cno++] = '\t';
	for (; tp->owrite && spaces; --tp->owrite, --spaces, ++tp->ai)
		tp->lb[sp->cno++] = ' ';

	if (!tabs && !spaces)
		return (0);

	/* Make sure there's enough room. */
	BINC_RET(sp, tp->lb, tp->lb_len, tp->len + spaces + tabs);

	/* Move the insert characters out of the way. */
	if (tp->insert)
		memmove(tp->lb + sp->cno + spaces + tabs,
		    tp->lb + sp->cno, tp->insert);

	/* Add new space/tab characters. */
	for (; tabs--; ++tp->len, ++tp->ai)
		tp->lb[sp->cno++] = '\t';
	for (; spaces--; ++tp->len, ++tp->ai)
		tp->lb[sp->cno++] = ' ';
	return (0);
}

/*
 * txt_outdent --
 *	Handle ^D outdents.
 *
 */
static int
txt_outdent(sp, tp)
	SCR *sp;
	TEXT *tp;
{
	u_long sw, ts;
	size_t cno, off, scno, spaces;

	ts = O_VAL(sp, O_TABSTOP);
	sw = O_VAL(sp, O_SHIFTWIDTH);

	/* Get the current screen column. */
	for (off = scno = 0; off < sp->cno; ++off)
		if (tp->lb[off] == '\t')
			scno += STOP_OFF(scno, ts);
		else
			++scno;

	/* Get the previous shiftwidth column. */
	for (cno = scno; --scno % sw != 0;);

	/* Decrement characters until less than or equal to that slot. */
	for (; cno > scno; --sp->cno, --tp->ai, ++tp->owrite)
		if (tp->lb[--off] == '\t')
			cno -= STOP_OFF(cno, ts);
		else
			--cno;

	/* Spaces needed to get to the target. */
	spaces = scno - cno;

	/* Maybe just a delete. */
	if (spaces == 0)
		return (0);

	/* Make sure there's enough room. */
	BINC_RET(sp, tp->lb, tp->lb_len, tp->len + spaces);

	/* Use up any overwrite characters. */
	for (; tp->owrite && spaces; --spaces, ++tp->ai, --tp->owrite)
		tp->lb[sp->cno++] = ' ';

	/* Maybe that was enough. */
	if (spaces == 0)
		return (0);

	/* Move the insert characters out of the way. */
	if (tp->insert)
		memmove(tp->lb + sp->cno + spaces,
		    tp->lb + sp->cno, tp->insert);

	/* Add new space characters. */
	for (; spaces--; ++tp->len, ++tp->ai)
		tp->lb[sp->cno++] = ' ';
	return (0);
}

/*
 * txt_resolve --
 *	Resolve the input text chain into the file.
 */
static int
txt_resolve(sp, ep, tiqh)
	SCR *sp;
	EXF *ep;
	TEXTH *tiqh;
{
	TEXT *tp;
	recno_t lno;

	/* The first line replaces a current line. */
	tp = tiqh->cqh_first;
	if (file_sline(sp, ep, tp->lno, tp->lb, tp->len))
		return (1);

	/* All subsequent lines are appended into the file. */
	for (lno = tp->lno; (tp = tp->q.cqe_next) != (void *)&sp->tiq; ++lno)
		if (file_aline(sp, ep, 0, lno, tp->lb, tp->len))
			return (1);
	return (0);
}

/*
 * txt_showmatch --
 *	Show a character match.
 *
 * !!!
 * Historic vi tried to display matches even in the :colon command line.
 * I think not.
 */
static void
txt_showmatch(sp, ep)
	SCR *sp;
	EXF *ep;
{
	struct timeval second;
	VCS cs;
	MARK m;
	fd_set zero;
	int cnt, endc, startc;

	/*
	 * Do a refresh first, in case the v_ntext() code hasn't done
	 * one in awhile, so the user can see what we're complaining
	 * about.
	 */
	if (sp->s_refresh(sp, ep))
		return;
	/*
	 * We don't display the match if it's not on the screen.  Find
	 * out what the first character on the screen is.
	 */
	if (sp->s_position(sp, ep, &m, 0, P_TOP))
		return;

	/* Initialize the getc() interface. */
	cs.cs_lno = sp->lno;
	cs.cs_cno = sp->cno - 1;
	if (cs_init(sp, ep, &cs))
		return;
	startc = (endc = cs.cs_ch)  == ')' ? '(' : '{';

	/* Search for the match. */
	for (cnt = 1;;) {
		if (cs_prev(sp, ep, &cs))
			return;
		if (cs.cs_lno < m.lno ||
		    cs.cs_lno == m.lno && cs.cs_cno < m.cno)
			return;
		if (cs.cs_flags != 0) {
			if (cs.cs_flags == CS_EOF || cs.cs_flags == CS_SOF) {
				(void)sp->s_bell(sp);
				return;
			}
			continue;
		}
		if (cs.cs_ch == endc)
			++cnt;
		else if (cs.cs_ch == startc && --cnt == 0)
			break;
	}

	/* Move to the match. */
	m.lno = sp->lno;
	m.cno = sp->cno;
	sp->lno = cs.cs_lno;
	sp->cno = cs.cs_cno;
	(void)sp->s_refresh(sp, ep);

	/*
	 * Sleep(3) is eight system calls.  Do it fast -- besides,
	 * I don't want to wait an entire second.
	 */
	FD_ZERO(&zero);
	second.tv_sec = O_VAL(sp, O_MATCHTIME) / 10;
	second.tv_usec = (O_VAL(sp, O_MATCHTIME) % 10) * 100000L;
	(void)select(0, &zero, &zero, &zero, &second);

	/* Return to the current location. */
	sp->lno = m.lno;
	sp->cno = m.cno;
	(void)sp->s_refresh(sp, ep);
}

/*
 * txt_margin --
 *	Handle margin wrap.
 *
 * !!!
 * Historic vi belled the user each time a character was entered after
 * crossing the margin until a space was entered which could be used to
 * break the line.  I don't, it tends to wake the cats.
 */
static int
txt_margin(sp, tp, didbreak, pushcp)
	SCR *sp;
	TEXT *tp;
	int *didbreak;
	CHAR_T *pushcp;
{
	CHAR_T ch;
	size_t len, off, tlen;
	char *p, *wp;

	/* Find the closest previous blank. */
	for (off = sp->cno - 1, p = tp->lb + off, len = 0;; --p, --off) {
		if (isblank(*p)) {
			wp = p + 1;
			break;
		}
		++len;
		/* If it's the beginning of the line, there's nothing to do. */
		if (off == tp->ai || off == tp->offset) {
			*didbreak = 0;
			return (0);
		}
	}

	/*
	 * Historic practice is to delete any trailing whitespace
	 * from the previous line.
	 */
	for (tlen = len;; --p, --off) {
		if (!isblank(*p))
			break;
		++tlen;
		if (off == tp->ai || off == tp->offset)
			break;
	}

	ch = *pushcp;
	if (term_push(sp, &ch, 1, 0, CH_NOMAP))
		return (1);
	if (len && term_push(sp, wp, len, 0, CH_NOMAP | CH_QUOTED))
		return (1);
	ch = '\n';
	if (term_push(sp, &ch, 1, 0, CH_NOMAP))
		return (1);

	sp->cno -= tlen;
	tp->owrite += tlen;
	*didbreak = 1;
	return (0);
}

/*
 * txt_Rcleanup --
 *	Resolve the input line for the 'R' command.
 */
static void
txt_Rcleanup(sp, tiqh, tp, lp, len)
	SCR *sp;
	TEXTH *tiqh;
	TEXT *tp;
	const char *lp;
	const size_t len;
{
	size_t tmp;

	/*
	 * The 'R' command restores any overwritable characters in the
	 * first line to the original characters.  Check to make sure
	 * that the cursor hasn't moved beyond the end of the original
	 * line.
	 */
	if (tp != tiqh->cqh_first || tp->owrite == 0 || sp->cno >= len)
		return;

	/* Restore whatever we can restore from the original line. */
	tmp = MIN(tp->owrite, len - sp->cno);
	memmove(tp->lb + sp->cno, lp + sp->cno, tmp);

	/*
	 * There can be more overwrite characters if the user extended the
	 * line and then erased it.  What we have to do is delete whatever
	 * the user inserted and then erased.  Regardless, we increase the
	 * insert character count to make the TEXT structure look right.
	 * (There shouldn't be any insert characters as 'R' replaces the
	 * entire line; if there are, this code isn't going to work).
	 */
	if (tp->owrite > tmp)
		tp->len -= tp->owrite - tmp;
	tp->owrite = 0;
	tp->insert = tmp;
}
